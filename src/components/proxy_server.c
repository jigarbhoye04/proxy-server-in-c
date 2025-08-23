#include "../../include/proxy/proxy_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// PHASE 6: Core Proxy Server Implementation

int proxy_server_init(int port) {
    printf("[INIT] Initializing proxy server on port %d...\n", port);

    // Phase 3: Initialize platform-specific networking
    platform_init();

    // Initialize synchronization primitives
    if (sem_init(&semaphore, 0, MAX_CLIENTS) != 0) {
        printf("[INIT] Failed to initialize semaphore\n");
        return -1;
    }

    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("[INIT] Failed to initialize mutex\n");
        return -1;
    }

    // Phase 2: Initialize thread pool
    thread_pool = thread_pool_create();
    if (thread_pool == NULL) {
        printf("[INIT] Failed to create thread pool\n");
        return -1;
    }

    // Phase 4: Initialize optimized cache
    optimized_cache = cache_create();
    if (optimized_cache == NULL) {
        printf("[INIT] Failed to create optimized cache\n");
        return -1;
    }

    // Phase 5: Initialize connection pool
    connection_pool = connection_pool_create(MAX_POOL_SIZE);
    if (connection_pool == NULL) {
        printf("[INIT] Failed to create connection pool\n");
        return -1;
    }

    printf("[INIT] All modules initialized successfully\n");
    return 0;
}

void proxy_server_start(void) {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_length;

    // Create server socket
    server_socket = create_server_socket(port_number);
    if (server_socket < 0) {
        printf("[SERVER] Failed to create server socket\n");
        return;
    }

    printf("[SERVER] Proxy server listening on port %d\n", port_number);
    printf("[SERVER] Ready to accept connections...\n");

    // Main server loop
    while (1) {
        client_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_length);

        if (client_socket < 0) {
            print_socket_error("Accept failed");
            continue;
        }

        printf("[SERVER] Client connected from %s:%d (socket %d)\n",
               inet_ntoa(client_address.sin_addr),
               ntohs(client_address.sin_port),
               client_socket);

        // Add task to thread pool for processing
        if (thread_pool_add_task(thread_pool, client_socket) != 0) {
            printf("[SERVER] Failed to add task to thread pool\n");
            socket_close(client_socket);
        }
    }

    socket_close(server_socket);
}

void proxy_server_shutdown(void) {
    printf("[SHUTDOWN] Shutting down proxy server...\n");

    // Cleanup all modules
    if (thread_pool) {
        thread_pool_destroy(thread_pool);
        thread_pool = NULL;
    }

    if (optimized_cache) {
        cache_destroy(optimized_cache);
        optimized_cache = NULL;
    }

    if (connection_pool) {
        connection_pool_destroy(connection_pool);
        connection_pool = NULL;
    }

    // Cleanup synchronization primitives
    sem_destroy(&semaphore);
    pthread_mutex_destroy(&lock);

    // Platform cleanup
    platform_cleanup();

    printf("[SHUTDOWN] Proxy server shutdown complete\n");
}

int create_server_socket(int port) {
    int server_socket;
    struct sockaddr_in server_address;
    int reuse = 1;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        print_socket_error("Socket creation failed");
        return -1;
    }

    // Set socket options for reuse
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
                   (const char*)&reuse, sizeof(reuse)) < 0) {
        print_socket_error("Setsockopt failed");
        socket_close(server_socket);
        return -1;
    }

    // Setup server address
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_socket_error("Bind failed");
        socket_close(server_socket);
        return -1;
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        print_socket_error("Listen failed");
        socket_close(server_socket);
        return -1;
    }

    return server_socket;
}

void handle_client_request(int client_socket) {
    char request_buffer[MAX_REQUEST_SIZE];
    int bytes_received;
    struct ParsedRequest* parsed_request;

    // Receive request from client
    bytes_received = recv(client_socket, request_buffer, sizeof(request_buffer) - 1, 0);
    if (bytes_received <= 0) {
        printf("[REQUEST] Failed to receive data from client\n");
        socket_close(client_socket);
        return;
    }

    request_buffer[bytes_received] = '\0';
    printf("[REQUEST] Received %d bytes from client\n", bytes_received);

    // Validate HTTP request
    if (!validate_http_request(request_buffer, bytes_received)) {
        printf("[REQUEST] Invalid HTTP request received\n");
        send_error_response(client_socket, 400, "Bad Request");
        socket_close(client_socket);
        return;
    }

    // Parse the request
    parsed_request = ParsedRequest_create();
    if (!parsed_request) {
        printf("[REQUEST] Failed to create parsed request\n");
        send_error_response(client_socket, 500, "Internal Server Error");
        socket_close(client_socket);
        return;
    }

    if (ParsedRequest_parse(parsed_request, request_buffer, bytes_received) < 0) {
        printf("[REQUEST] Failed to parse HTTP request\n");
        send_error_response(client_socket, 400, "Bad Request");
        ParsedRequest_destroy(parsed_request);
        socket_close(client_socket);
        return;
    }

    // Forward request to destination server
    if (forward_request_to_server(parsed_request, client_socket) < 0) {
        printf("[REQUEST] Failed to forward request to server\n");
        send_error_response(client_socket, 502, "Bad Gateway");
    }

    // Cleanup
    ParsedRequest_destroy(parsed_request);
    socket_close(client_socket);
}

int send_error_response(int client_socket, int error_code, const char* message) {
    char response[1024];
    int response_length;

    response_length = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>%d %s</h1></body></html>",
        error_code, message, (int)(39 + strlen(message) + 20), error_code, message);

    return send(client_socket, response, response_length, 0);
}

int forward_request_to_server(struct ParsedRequest* request, int client_socket) {
    if (!request || client_socket <= 0) {
        printf("[FORWARD] Invalid parameters\n");
        return -1;
    }

    char host[256];
    int port = 80;
    char request_buffer[MAX_REQUEST_SIZE];
    char response_buffer[MAX_RESPONSE_SIZE];

    printf("[FORWARD] Request details - Method: %s, Path: %s, Host: %s\n", 
           request->method ? request->method : "NULL",
           request->path ? request->path : "NULL",
           request->host ? request->host : "NULL");

    // Extract host and port from request
    if (!request->host || extract_host_port(request->host, host, &port) < 0) {
        printf("[FORWARD] Failed to extract host and port from: %s\n", 
               request->host ? request->host : "NULL");
        return -1;
    }

    printf("[FORWARD] Extracted host: %s, port: %d\n", host, port);

    // Check cache first (use full URL as cache key)
    char cache_key[512];
    snprintf(cache_key, sizeof(cache_key), "%s", request->path);
    
    cache_node_t* cached = cache_get(optimized_cache, cache_key);
    if (cached) {
        // Send cached response
        printf("[FORWARD] Sending cached response (%d bytes)\n", cached->data_size);
        send(client_socket, cached->data, cached->data_size, 0);
        return 0;
    }

    // Get connection from pool
    int server_socket = connection_pool_get(connection_pool, host, port);
    if (server_socket < 0) {
        // Create new connection
        server_socket = create_persistent_connection(host, port);
        if (server_socket < 0) {
            printf("[FORWARD] Failed to connect to %s:%d\n", host, port);
            return -1;
        }
    }

    // Extract path from full URL for HTTP request
    char actual_path[256] = "/";
    if (strstr(request->path, "http://")) {
        char* path_start = strstr(request->path + 7, "/");
        if (path_start) {
            strncpy(actual_path, path_start, sizeof(actual_path) - 1);
            actual_path[sizeof(actual_path) - 1] = '\0';
        }
    } else if (request->path[0] == '/') {
        strncpy(actual_path, request->path, sizeof(actual_path) - 1);
        actual_path[sizeof(actual_path) - 1] = '\0';
    }

    // Build and send request
    int request_len = snprintf(request_buffer, sizeof(request_buffer),
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: ProxyServer/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        request->method, actual_path, host);

    printf("[FORWARD] Sending request to %s:%d: %s %s\n", host, port, request->method, actual_path);

    if (send(server_socket, request_buffer, request_len, 0) < 0) {
        print_socket_error("Failed to send request to server");
        socket_close(server_socket);
        return -1;
    }

    // Receive response with proper HTTP handling
    int total_received = 0;
    int bytes_received;
    int header_end_found = 0;
    
    // Set receive timeout to 5 seconds
    #ifdef _WIN32
    DWORD timeout = 5000; // 5 seconds in milliseconds
    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    #else
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    #endif
    
    while (total_received < MAX_RESPONSE_SIZE - 1) {
        bytes_received = recv(server_socket, 
                            response_buffer + total_received, 
                            MAX_RESPONSE_SIZE - total_received - 1, 0);
        if (bytes_received <= 0) {
            if (total_received > 0) {
                // Some data received, break and use what we have
                printf("[FORWARD] Connection closed by server, using %d bytes\n", total_received);
                break;
            } else {
                printf("[FORWARD] No data received from server\n");
                socket_close(server_socket);
                return -1;
            }
        }
        total_received += bytes_received;
        
        // Check for end of HTTP headers to determine if we got a complete response
        if (!header_end_found && total_received >= 4) {
            response_buffer[total_received] = '\0';
            if (strstr(response_buffer, "\r\n\r\n")) {
                header_end_found = 1;
                // For simple responses, we might have everything
                if (total_received < 2048) {
                    break;
                }
            }
        }
        
        // If we've received a good amount and headers are complete, break
        if (header_end_found && total_received > 1024) {
            break;
        }
    }

    if (total_received > 0) {
        response_buffer[total_received] = '\0';
        
        printf("[FORWARD] Received %d bytes from %s:%d\n", total_received, host, port);
        
        // Send response to client
        int sent = send(client_socket, response_buffer, total_received, 0);
        if (sent < 0) {
            print_socket_error("Failed to send response to client");
        } else {
            printf("[FORWARD] Sent %d bytes to client\n", sent);
        }
        
        // Cache the response using full URL as key
        cache_add(optimized_cache, cache_key, response_buffer, total_received);
        printf("[CACHE] Added entry for URL: %s (size: %d bytes)\n", cache_key, total_received);
    }

    // Return connection to pool
    connection_pool_return(connection_pool, server_socket, host, port, 1);

    return total_received > 0 ? 0 : -1;
}

int parse_request_url(const char* url, char* host, int* port, char* path) {
    if (!url || !host || !port || !path) {
        return -1;
    }

    // Default values
    *port = 80;
    strcpy(path, "/");

    // Skip "http://" if present
    const char* start = url;
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
    }

    // Find the end of host part (either ':' for port or '/' for path)
    const char* port_start = strchr(start, ':');
    const char* path_start = strchr(start, '/');

    if (port_start && (!path_start || port_start < path_start)) {
        // Port is specified
        int host_len = port_start - start;
        strncpy(host, start, host_len);
        host[host_len] = '\0';

        // Extract port
        *port = atoi(port_start + 1);

        // Extract path if present
        if (path_start) {
            strcpy(path, path_start);
        }
    } else if (path_start) {
        // No port, but path is specified
        int host_len = path_start - start;
        strncpy(host, start, host_len);
        host[host_len] = '\0';
        strcpy(path, path_start);
    } else {
        // Only host specified
        strcpy(host, start);
    }

    return 0;
}
