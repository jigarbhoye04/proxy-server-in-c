#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

// PHASE 6: Main Proxy Server Module
// Core server logic and request handling

#include "platform.h"
#include "http_parser.h"
#include "thread_pool.h"
#include "connection_pool.h"
#include "cache.h"

// Server configuration
#define DEFAULT_PORT 8080
#define MAX_REQUEST_SIZE 4096
#define MAX_RESPONSE_SIZE 1048576  // 1MB

// Global server state
extern int port_number;
extern thread_pool_t* thread_pool;
extern optimized_cache_t* optimized_cache;
extern connection_pool_t* connection_pool;

// Core server functions
int proxy_server_init(int port);
void proxy_server_start(void);
void proxy_server_shutdown(void);

// Request handling functions
void handle_client_request(int client_socket);
int forward_request_to_server(struct ParsedRequest* request, int client_socket);
int send_error_response(int client_socket, int error_code, const char* message);

// Utility functions
int create_server_socket(int port);
int parse_request_url(const char* url, char* host, int* port, char* path);

#endif // PROXY_SERVER_H
