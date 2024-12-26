#include "proxy_parse.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


#include <winsock2.h>
#include <ws2tcpip.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netdb.h>
// #include <arpa/inet.h>
// #include <sys/wait.h>


#define MAX_CLIENTS 10
typedef struct cache_element cache_element;

// lru cache (one element with linked list)
struct cache_element
{
    char *data;            // response data
    int len;               // length of data
    char *url;             // reuested url
    time_t lru_time_track; // tracks time for lru cache elemtns
    // struct cache_element* next;
    cache_element *next;
};

// to find the specific cache in memory
cache_element *find(char *url);

// to add elements in cache
int add_cache_element(char *data, int size, char *url);

// to remove cache elements
void remove_cache_elements();

int port_number = 8080;
int proxy_socketID; // socket id of the proxy server

// cause this is a multithreaaded we need to create multiple threads for multiple clients with unique sockets.
pthread_t tid[MAX_CLIENTS];

// semaphores for the cache
// will help to lock the cache when a thread is reading or writing to it
sem_t semaphore;
// mutex lock for the cache
pthread_mutex_t lock;

//defining the cache head
cache_element *cache_head = NULL;
int cache_size;//replace with the size of the cache


int main(int argc, char *argv[]){
    int client_socketdID, client_length; // client socket id and length
    struct sockaddr_in server_address, client_address; // server and client address

    //initializing the semaphore
    sem_init(&semaphore, MAX_CLIENTS,0);
    
    //initialize the mutex lock
    pthread_mutex_init(&lock, NULL);

    //check if the port number is provided
    if(argv == 2){
        // ./proxy <port number>
        port_number = atoi(argv[1]);
    }else{
        // printf("No port number provided, using default port number 8080\n");
        printf("Too few arguments\n");
        exit(1);//sys call
    }

    printf("Proxy server started on port %d\n", port_number);

    //openning socket of proxy web server(main socket)
    proxy_socketID = socket(AF_INET, SOCK_STREAM, 0);

    if(proxy_socketID < 0){
        perror("Failed to create a socket");
        exit(1);
    }

} 