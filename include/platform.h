#ifndef PLATFORM_H
#define PLATFORM_H

// PHASE 6: Platform Compatibility Module
// Abstracts Windows/Linux differences for networking and threading

#ifdef _WIN32
    // Windows includes
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    // Windows compatibility defines
    #define SHUT_RDWR SD_BOTH
    #define close closesocket
    
#else
    // Unix/Linux includes
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/wait.h>
    
#endif

// Platform abstraction functions
void platform_init(void);
void platform_cleanup(void);
int get_socket_error(void);
void print_socket_error(const char* message);

// Cross-platform utility functions
void bzero(void *s, size_t n);
void bcopy(const void *src, void *dest, size_t n);

#endif // PLATFORM_H
