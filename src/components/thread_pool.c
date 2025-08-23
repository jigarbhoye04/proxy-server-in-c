#include "../../include/proxy/thread_pool.h"
#include "../../include/proxy/proxy_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Thread Pool Implementation

// Global synchronization primitives (declared as extern - defined in main)
extern sem_t semaphore;
extern pthread_mutex_t lock;

// Worker thread function
void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    
    printf("[THREAD] Worker thread started\n");
    
    while (1) {
        pthread_mutex_lock(&pool->queue_mutex);
        
        // Wait for tasks in the queue
        while (pool->task_queue_head == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_condition, &pool->queue_mutex);
        }
        
        // Check if we should shutdown
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        // Get task from queue
        task_t* task = pool->task_queue_head;
        if (task) {
            pool->task_queue_head = task->next;
            if (pool->task_queue_head == NULL) {
                pool->task_queue_tail = NULL;
            }
            pool->queue_size--;
        }
        
        pthread_mutex_unlock(&pool->queue_mutex);
        
        // Process the task
        if (task) {
            printf("[WORKER] Processing client socket %d\n", task->client_socket);
            
            // Wait for semaphore (connection limiting)
            sem_wait(&semaphore);
            
            // Handle the client request
            handle_client_request(task->client_socket);
            
            // Release semaphore
            sem_post(&semaphore);
            
            // Free the task
            free(task);
        }
    }
    
    printf("[THREAD] Worker thread exiting\n");
    return NULL;
}

thread_pool_t* thread_pool_create(void) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    if (!pool) {
        printf("[POOL] Failed to allocate memory for thread pool\n");
        return NULL;
    }
    
    // Initialize pool structure
    pool->task_queue_head = NULL;
    pool->task_queue_tail = NULL;
    pool->queue_size = 0;
    pool->shutdown = 0;
    
    // Initialize synchronization
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
        printf("[POOL] Failed to initialize queue mutex\n");
        free(pool);
        return NULL;
    }
    
    if (pthread_cond_init(&pool->queue_condition, NULL) != 0) {
        printf("[POOL] Failed to initialize queue condition\n");
        pthread_mutex_destroy(&pool->queue_mutex);
        free(pool);
        return NULL;
    }
    
    // Create worker threads
    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        if (pthread_create(&pool->workers[i], NULL, worker_thread, pool) != 0) {
            printf("[POOL] Failed to create worker thread %d\n", i);
            
            // Cleanup already created threads
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->queue_condition);
            
            for (int j = 0; j < i; j++) {
                pthread_join(pool->workers[j], NULL);
            }
            
            pthread_mutex_destroy(&pool->queue_mutex);
            pthread_cond_destroy(&pool->queue_condition);
            free(pool);
            return NULL;
        }
    }
    
    printf("[POOL] Thread pool created with %d worker threads\n", NUM_WORKER_THREADS);
    return pool;
}

int thread_pool_add_task(thread_pool_t* pool, int client_socket) {
    if (!pool || client_socket <= 0) {
        return -1;
    }
    
    // Create new task
    task_t* task = malloc(sizeof(task_t));
    if (!task) {
        printf("[POOL] Failed to allocate memory for task\n");
        return -1;
    }
    
    task->client_socket = client_socket;
    task->next = NULL;
    
    // Add task to queue
    pthread_mutex_lock(&pool->queue_mutex);
    
    if (pool->task_queue_tail) {
        pool->task_queue_tail->next = task;
    } else {
        pool->task_queue_head = task;
    }
    pool->task_queue_tail = task;
    pool->queue_size++;
    
    printf("[POOL] Task added to queue (queue size: %d)\n", pool->queue_size);
    
    // Signal waiting threads
    pthread_cond_signal(&pool->queue_condition);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    return 0;
}

void thread_pool_destroy(thread_pool_t* pool) {
    if (!pool) return;
    
    printf("[POOL] Shutting down thread pool...\n");
    
    // Signal shutdown
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_condition);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Wait for all worker threads to finish
    for (int i = 0; i < NUM_WORKER_THREADS; i++) {
        pthread_join(pool->workers[i], NULL);
    }
    
    // Clean up remaining tasks
    pthread_mutex_lock(&pool->queue_mutex);
    task_t* current = pool->task_queue_head;
    while (current) {
        task_t* next = current->next;
        socket_close(current->client_socket);  // Close any pending client connections
        free(current);
        current = next;
    }
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Destroy synchronization objects
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_condition);
    
    // Free the pool
    free(pool);
    
    printf("[POOL] Thread pool destroyed\n");
}
