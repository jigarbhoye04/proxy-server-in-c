// PHASE 6: Modular Proxy Server Main
// Clean, readable main server implementation using modular components

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
    printf("[SERVER] All optimization phases active:\n");
    printf("[SERVER]   - Phase 1: Memory safety and error handling\n");
    printf("[SERVER]   - Phase 2: Thread pool with %d workers\n", NUM_WORKER_THREADS);
    printf("[SERVER]   - Phase 3: Cross-platform compatibility\n");
    printf("[SERVER]   - Phase 4: O(1) hash table cache\n");
    printf("[SERVER]   - Phase 5: Connection pooling\n");
    printf("[SERVER]   - Phase 6: Modular architecture\n");
    printf("[SERVER] ================================================\n");

    // Start the server (this will run indefinitely)
    proxy_server_start();

    // This should not be reached unless server shuts down
    proxy_server_shutdown();
    return 0;
}
