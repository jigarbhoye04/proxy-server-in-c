#include "../../include/proxy/platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// PHASE 6: Platform Compatibility Implementation

#ifdef _WIN32
    // Windows socket initialization state
    static int winsock_initialized = 0;
    
    void platform_init(void) {
        if (!winsock_initialized) {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
                printf("[PLATFORM] Failed to initialize Winsock\n");
                exit(1);
            }
            winsock_initialized = 1;
            printf("[PLATFORM] Winsock initialized successfully\n");
        }
    }
    
    void platform_cleanup(void) {
        if (winsock_initialized) {
            WSACleanup();
            winsock_initialized = 0;
            printf("[PLATFORM] Winsock cleaned up\n");
        }
    }
    
    int get_socket_error(void) {
        return WSAGetLastError();
    }
    
    void print_socket_error(const char* message) {
        int error = WSAGetLastError();
        printf("[PLATFORM] %s: Winsock error %d\n", message, error);
    }
    
    // Cross-platform socket close
    int socket_close(socket_t sock) {
        return closesocket(sock);
    }
    
    // Windows bzero implementation
    void bzero(void *s, size_t n) {
        memset(s, 0, n);
    }
    
    // Windows bcopy implementation  
    void bcopy(const void *src, void *dest, size_t n) {
        memmove(dest, src, n);
    }
    
#else
    // Unix/Linux implementation
    void platform_init(void) {
        printf("[PLATFORM] Unix/Linux networking initialized\n");
    }
    
    void platform_cleanup(void) {
        printf("[PLATFORM] Unix/Linux networking cleaned up\n");
    }
    
    int get_socket_error(void) {
        return errno;
    }
    
    void print_socket_error(const char* message) {
        perror(message);
    }
    
    // Cross-platform socket close
    int socket_close(socket_t sock) {
        return close(sock);
    }
    
    // Unix already has bzero and bcopy, but provide stubs for consistency
    void bzero(void *s, size_t n) {
        memset(s, 0, n);
    }
    
    void bcopy(const void *src, void *dest, size_t n) {
        memmove(dest, src, n);
    }
    
#endif
