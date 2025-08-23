#ifndef PROXY_CONNECTION_POOL_H
#define PROXY_CONNECTION_POOL_H

#include <pthread.h>
#include <time.h>

// Connection Pool Module
// Manages persistent connections for improved performance

#define MAX_POOL_SIZE 20
#define CONNECTION_TIMEOUT 30  // 30 seconds keep-alive

// Connection pool entry structure
typedef struct {
    int socket_fd;
    char host[256];
    int port;
    time_t last_used;
    int in_use;
} connection_pool_entry_t;

// Connection pool structure
typedef struct {
    connection_pool_entry_t connections[MAX_POOL_SIZE];
    pthread_mutex_t pool_mutex;
    int pool_size;
} connection_pool_t;

// Connection pool management functions
connection_pool_t* connection_pool_create(int max_size);
int connection_pool_get(connection_pool_t* pool, char* host, int port);
void connection_pool_return(connection_pool_t* pool, int socket_fd, char* host, int port, int keep_alive);
void connection_pool_cleanup(connection_pool_t* pool);
void connection_pool_destroy(connection_pool_t* pool);

// Connection utilities
int create_persistent_connection(char* host_address, int port_number);

#endif // PROXY_CONNECTION_POOL_H
