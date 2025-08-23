#include "../../include/proxy/connection_pool.h"
#include "../../include/proxy/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <netdb.h>
#endif

// PHASE 6: Connection Pool Implementation

connection_pool_t* connection_pool_create(int max_size) {
    connection_pool_t* pool = malloc(sizeof(connection_pool_t));
    if (!pool) {
        printf("[CONN_POOL] Failed to allocate memory for connection pool\n");
        return NULL;
    }
    
    // Initialize pool structure
    pool->pool_size = 0;
    
    // Initialize all connections
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        memset(&pool->connections[i], 0, sizeof(connection_pool_entry_t));
        pool->connections[i].socket_fd = -1;
        pool->connections[i].in_use = 0;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&pool->pool_mutex, NULL) != 0) {
        printf("[CONN_POOL] Failed to initialize pool mutex\n");
        free(pool);
        return NULL;
    }
    
    printf("[CONN_POOL] Connection pool created with %d max connections\n", max_size);
    return pool;
}

int create_persistent_connection(char* host_address, int port_number) {
    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_socket < 0) {
        print_socket_error("Failed to create socket for remote server");
        return -1;
    }
    
    struct hostent* host = gethostbyname(host_address);
    if (host == NULL) {
        printf("[CONN] Failed to resolve host: %s\n", host_address);
        socket_close(remote_socket);
        return -1;
    }
    
    struct sockaddr_in remote_address;
    bzero(&remote_address, sizeof(remote_address));
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(port_number);
    bcopy(host->h_addr, &remote_address.sin_addr.s_addr, host->h_length);
    
    if (connect(remote_socket, (struct sockaddr*)&remote_address, sizeof(remote_address)) < 0) {
        print_socket_error("Failed to connect to remote server");
        socket_close(remote_socket);
        return -1;
    }
    
    printf("[CONN] Persistent connection established to %s:%d (socket %d)\n", 
           host_address, port_number, remote_socket);
    return remote_socket;
}

int connection_pool_get(connection_pool_t* pool, char* host, int port) {
    if (!pool || !host) {
        return -1;
    }
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    time_t current_time = time(NULL);
    
    // Look for existing connection to this host:port
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        connection_pool_entry_t* conn = &pool->connections[i];
        
        if (conn->socket_fd > 0 && !conn->in_use &&
            strcmp(conn->host, host) == 0 && conn->port == port) {
            
            // Check if connection hasn't timed out
            if (current_time - conn->last_used < CONNECTION_TIMEOUT) {
                conn->in_use = 1;
                conn->last_used = current_time;
                
                printf("[CONN_POOL] Reusing connection to %s:%d (socket %d)\n", 
                       host, port, conn->socket_fd);
                
                pthread_mutex_unlock(&pool->pool_mutex);
                return conn->socket_fd;
            } else {
                // Connection timed out, close it
                printf("[CONN_POOL] Connection to %s:%d timed out, closing socket %d\n", 
                       host, port, conn->socket_fd);
                socket_close(conn->socket_fd);
                memset(conn, 0, sizeof(connection_pool_entry_t));
                conn->socket_fd = -1;
                pool->pool_size--;
            }
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    
    // No suitable connection found, create a new one
    int new_socket = create_persistent_connection(host, port);
    if (new_socket > 0) {
        printf("[CONN_POOL] Created new connection to %s:%d (socket %d)\n", 
               host, port, new_socket);
    }
    
    return new_socket;
}

void connection_pool_return(connection_pool_t* pool, int socket_fd, char* host, int port, int keep_alive) {
    if (!pool || socket_fd <= 0 || !host) {
        return;
    }
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    if (!keep_alive) {
        // Connection doesn't support keep-alive, close it
        socket_close(socket_fd);
        printf("[CONN_POOL] Connection to %s:%d closed (no keep-alive)\n", host, port);
        pthread_mutex_unlock(&pool->pool_mutex);
        return;
    }
    
    // Try to store the connection in the pool
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        connection_pool_entry_t* conn = &pool->connections[i];
        
        if (conn->socket_fd == socket_fd) {
            // Connection already in pool, just mark as not in use
            conn->in_use = 0;
            conn->last_used = time(NULL);
            
            printf("[CONN_POOL] Connection to %s:%d returned to pool (socket %d)\n", 
                   host, port, socket_fd);
            
            pthread_mutex_unlock(&pool->pool_mutex);
            return;
        }
    }
    
    // Find empty slot to store the connection
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        connection_pool_entry_t* conn = &pool->connections[i];
        
        if (conn->socket_fd <= 0) {
            conn->socket_fd = socket_fd;
            strncpy(conn->host, host, sizeof(conn->host) - 1);
            conn->host[sizeof(conn->host) - 1] = '\0';
            conn->port = port;
            conn->last_used = time(NULL);
            conn->in_use = 0;
            pool->pool_size++;
            
            printf("[CONN_POOL] Connection to %s:%d added to pool (socket %d)\n", 
                   host, port, socket_fd);
            
            pthread_mutex_unlock(&pool->pool_mutex);
            return;
        }
    }
    
    // Pool is full, close the connection
    socket_close(socket_fd);
    printf("[CONN_POOL] Pool full, closed connection to %s:%d (socket %d)\n", 
           host, port, socket_fd);
    
    pthread_mutex_unlock(&pool->pool_mutex);
}

void connection_pool_cleanup(connection_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    time_t current_time = time(NULL);
    int cleaned = 0;
    
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        connection_pool_entry_t* conn = &pool->connections[i];
        
        if (conn->socket_fd > 0 && !conn->in_use &&
            current_time - conn->last_used >= CONNECTION_TIMEOUT) {
            
            printf("[CONN_POOL] Cleaning up timed out connection to %s:%d (socket %d)\n", 
                   conn->host, conn->port, conn->socket_fd);
            
            socket_close(conn->socket_fd);
            memset(conn, 0, sizeof(connection_pool_entry_t));
            conn->socket_fd = -1;
            pool->pool_size--;
            cleaned++;
        }
    }
    
    if (cleaned > 0) {
        printf("[CONN_POOL] Cleaned up %d timed out connections\n", cleaned);
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
}

void connection_pool_destroy(connection_pool_t* pool) {
    if (!pool) return;
    
    printf("[CONN_POOL] Destroying connection pool...\n");
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    // Close all connections
    for (int i = 0; i < MAX_POOL_SIZE; i++) {
        if (pool->connections[i].socket_fd > 0) {
            socket_close(pool->connections[i].socket_fd);
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    
    // Destroy mutex
    pthread_mutex_destroy(&pool->pool_mutex);
    
    // Free the pool
    free(pool);
    
    printf("[CONN_POOL] Connection pool destroyed\n");
}
