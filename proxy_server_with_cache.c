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
#define MAX_BYTES 10*(1<<10) //2^10=1024
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



int handle_request(int clientSocketID,struct ParsedRequest *request,char* tempReq){
    char* buff = (char*)malloc(sizeof(char)*MAX_BYTES);
    strcpy(buff,"GET");
    strcat(buff,request->path);
    strcat(buff," ");
    strcat(buff,request->version);
    strcat(buff,"\r\n");
    size_t len = strlen(buff);

    if(ParsedHeader_set(request,"Connection","close") < 0){
        printf("Failed to set the connection\n");
        printf(" or set header key is not working\n");
        return -1;
    }

    if(ParsedHeader_get(request,"Host") != NULL){
        if(ParsedHeader_set(request,"Host",request->host) < 0){
            printf("Failed to set the host\n");
            return -1;
        }
    }

    if(ParsedRequest_unparse_headers(request,buff+len,(size_t)(MAX_BYTES)-len) < 0){
        printf("Failed to unparse the headers\n");
        printf(" or unparse headers is not working\n");
        return -1;
    }

    int server_port = 80;//http always runs on port 80
    if(request->port != NULL){
        server_port = atoi(request->port);
    }
    int remoteSocketID = connectRemoteServer(request->host,server_port);
}


//thread function
void *thread_fn(void *newSocket){
    sem_wait(&semaphore);//wait for the semaphore to be available
    int p;
    sem_getvalue(&semaphore, &p);
    printf("Semaphore value: %d\n", p);

    int *t=(int*)newSocket;
    int socket = *t;

    int bytes_received_client,recv_len;

    char* buffer = (char*)calloc(MAX_BYTES,sizeof(char));
    bzero(buffer,MAX_BYTES);
    bytes_received_client = recv(socket,buffer,MAX_BYTES,0);//recieve the data from the client


    while(bytes_received_client > 0){
        recv_len=strlen(buffer);
        printf("Received from client: %s\n",buffer);

        if(substr(buffer,"\r\n\r\n")==NULL){ //"\r\n\r\n" this indicates the end of the request from header
            bytes_received_client = recv(socket,buffer+recv_len,MAX_BYTES-recv_len,0);
        }else{
            break;
        }
    }

    //now we have the request from the client
    //we need to parse the request

    //so first we make a copy of it so that later we can use it to search in cache
    char *tempReq = (char *)malloc(strlen(buffer)*sizeof(char)+1);//it allocates memory for the request and store it in tempReq pointer
    for(int i=0;i<strlen(buffer);i++){
        tempReq[i] = buffer[i];
    }


    struct cache_element *temp = find(tempReq);//search the cache for the request
    if(temp != NULL){
        //if the request is found in the cache
        //then we can send the data to the client
        int size = temp->len/sizeof(char); //size of the data
        //exaample: if the data is "hello" then size(actual) will be 20  and if char is of 4 bytes then size will be 5(characters we can say)
        int pos = 0;
        char response[MAX_BYTES];
        while(pos<size){
            bzero(response,MAX_BYTES);
            for(int i=0;i<MAX_BYTES && pos<size;i++){
                response[i] = temp->data[pos];
                pos++;
            }
            send(socket,response,strlen(response),0);//send data to socket
        }

        printf("Data received from cache\n");
        printf("Data sent to client: ");
        printf("%s\n\n",response);
    }else if(bytes_received_client > 0){//if the request is not found in the cache
        //then we need to send the request to the server
        //and then cache the response
        //so first we need to parse the request
        recv_len = strlen(buffer);
        struct ParsedRequest *req = ParsedRequest_create(); //to fetch the request and headers

        if(ParsedRequest_parse(req,buffer,recv_len) < 0){
            printf("Failed to parse the request\n");
            exit(1);
        }else{
            bzero(buffer,MAX_BYTES);
            if(strcmp(req->method,"GET")>0){
                //if the request is not a GET request
                //then we need to send a bad request response to the client
                char *bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
                send(socket,bad_request,strlen(bad_request),0);
                printf("Bad request\n"); 
            }else if(!strcmp(req->method,"GET")){
                if(req->host && req->path && checkHTTPversion(req->version)==1){//httpversion checks only http ver 1.0
                    //todo: handle request
                    bytes_received_client = handle_request(socket,req,tempReq);

                    if(bytes_received_client < 0){
                        sendErrorMessages(socket,500);
                    }
                }else{
                    sendErrorMessages(socket,500);
                }

            }else{
                printf("Method not supported apart from GET\n");
            }
        }

        ParsedRequest_destroy(req);//destroy the request object after use
    }else{
        printf("Failed to receive request from client\n");
        if(bytes_received_client == 0){
            printf("Client disconnected\n");
            shutdown(socket,SHUT_RDWR);
            close(socket);
            free(buffer);
            sem_post(&semaphore);//sem_signal
            sem_getvalue(&semaphore, &p);
            printf("Semaphore post value is: %d\n", p);
            free(tempReq);
            return NULL;
        }

    }

}


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
        pthread_create(&tid[client_count],NULL,thread_fn,(void*)&Connected_socketID[client_count]);
        client_count++;
    }

    close(proxy_socketID);
    return 0;
} 