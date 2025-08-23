#include "../include/proxy/proxy_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

// Global server state
int port_number = DEFAULT_PORT;
thread_pool_t* thread_pool = NULL;
optimized_cache_t* optimized_cache = NULL;
connection_pool_t* connection_pool = NULL;

// Global synchronization primitives
sem_t semaphore;
pthread_mutex_t lock;

// pthread library variable definition (defined once here, declared extern in pthread.h)
void (**_pthread_key_dest)(void *) = NULL;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    printf("\n[SERVER] Received signal %d, shutting down gracefully...\n", sig);
    proxy_server_shutdown();
    exit(0);
}

int main(int argc, char *argv[]) {
    printf("[SERVER] Starting HTTP Proxy Server - Phase 6 (Modular)\n");
    printf("[SERVER] ================================================\n");

    // Parse command line arguments
    if (argc == 2) {
        port_number = atoi(argv[1]);
        if (port_number <= 0 || port_number > 65535) {
            printf("[SERVER] Invalid port number: %s\n", argv[1]);
            printf("[SERVER] Usage: %s [port]\n", argv[0]);
            exit(1);
        }
    }

    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize the proxy server
    if (proxy_server_init(port_number) != 0) {
        printf("[SERVER] Failed to initialize proxy server\n");
        exit(1);
    }

    printf("[SERVER] Proxy server successfully initialized\n");
    // (this will run indefinitely)
    proxy_server_start();
    proxy_server_shutdown();
    return 0;
}
