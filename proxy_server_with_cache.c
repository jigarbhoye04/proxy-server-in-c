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

    //if created then we can reuse it..
    int resuse = 1;
    if(setsockopt(proxy_socketID, SOL_SOCKET, SO_REUSEADDR, (const char*)&resuse, sizeof(resuse) < 0)){
        perror("Failed to set socket options : (setsockopt-failed)");
        exit(1);
    }

    //each struct we define have garbage vaules on initialization so wee need to clean that and assign values
    //bzero makes all values 0
    bzero((char *)&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;//address family - ipv4
    server_address.sin_port = htons(port_number); //htons-it converts into number that netweork can understand like 8080 to 0x8080 etc..
    server_address.sin_addr.s_addr = INADDR_ANY; //any address can connect to this server

    //now binding the socket to the address
    if(bind(proxy_socketID, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
        perror("Port is not available");
        exit(1);
    }

    printf("Binding on port %d\n", port_number);
    printf("Proxy server is now listening on port %d\n", port_number);

    //listening to the port for clients
    int listen_status = listen(proxy_socketID, MAX_CLIENTS);
    if(listen_status < 0){
        perror("Failed to listen\n");
        exit(1);
    }

    //defining iterator for nu. of connected clients
    int client_count = 0;
    int Connected_socketID[MAX_CLIENTS];

    while(1){
        bzero((char*)&client_address, sizeof(client_address));//cleaned the client address and removed the garbage values
        client_length = sizeof(client_address);//storing the final length of the client address

        client_socketdID = accept(proxy_socketID,(struct sockaddr*)&client_address, (socklen_t*)&client_length);//accepting the client connection

        if(client_socketdID < 0){
            perror("Failed to accept the client connection");
            exit(1);
        }else{
            //adding the client to the connected clients list
            Connected_socketID[client_count] = client_socketdID;
        }

        printf("Client %d connected\n", client_count);


        //creating a pointer to the client address
        //typecased into sockaddr_in for easy access and because we know the client address is of type sockaddr_in
        struct sockaddr_in* client_pointer = (struct sockaddr_in*)&client_address;
        struct in_addr client_ip = client_pointer->sin_addr;//getting the client ip address
        char client_ip_address_str[INET_ADDRSTRLEN];//defining the ip address of the client
        inet_ntop(AF_INET, &client_ip, client_ip_address_str, INET_ADDRSTRLEN);//converting the ip address to string
        //inet_ntop is used to convert the ip address from newtowk format to presentation format

        printf("Client is connected on port %d with IP address %s\n", ntohs(client_pointer->sin_port), client_ip_address_str);

        //todo:creaate threads for each client
    }
} 