#ifndef PROXY_THREAD_POOL_H
#define PROXY_THREAD_POOL_H

#include <pthread.h>
#include <semaphore.h>

// PHASE 6: Thread Pool Module
// Manages worker threads and task queue for concurrent request handling

#define MAX_CLIENTS 200
#define NUM_WORKER_THREADS 4

// Task structure for thread pool
typedef struct task {
    int client_socket;
    struct task* next;
} task_t;

// Thread pool structure
typedef struct {
    pthread_t workers[NUM_WORKER_THREADS];
    task_t* task_queue_head;
    task_t* task_queue_tail;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_condition;
    int queue_size;
    int shutdown;
} thread_pool_t;

// Thread pool management functions
thread_pool_t* thread_pool_create(void);
int thread_pool_add_task(thread_pool_t* pool, int client_socket);
void thread_pool_destroy(thread_pool_t* pool);

// Worker thread function
void* worker_thread(void* arg);

// Global semaphore for connection limiting
extern sem_t semaphore;
extern pthread_mutex_t lock;

#endif // PROXY_THREAD_POOL_H
