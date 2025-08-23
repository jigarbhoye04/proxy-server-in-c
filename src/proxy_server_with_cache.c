#include "../include/proxy_parse.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

// PHASE 3: Platform Compatibility Layer
#ifdef _WIN32
    // Windows includes
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    // Windows compatibility defines
    #define SHUT_RDWR SD_BOTH
    #define close closesocket
    
    // Windows socket initialization
    static int winsock_initialized = 0;
    static void init_winsock() {
        if (!winsock_initialized) {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2,2), &wsaData);
            winsock_initialized = 1;
        }
    }
    
    static void cleanup_winsock() {
        if (winsock_initialized) {
            WSACleanup();
            winsock_initialized = 0;
        }
    }
    
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
    
    // Unix compatibility stubs
    static void init_winsock() { }
    static void cleanup_winsock() { }
#endif

// PHASE 3: Cross-platform socket error handling
int get_socket_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void print_socket_error(const char* message) {
#ifdef _WIN32
    fprintf(stderr, "%s: Error %d\n", message, WSAGetLastError());
#else
    perror(message);
#endif
}

// PHASE 3: Cross-platform function implementations
void bzero(void *s, size_t n) {
    memset(s, 0, n);
}

void bcopy(const void *src, void *dest, size_t n) {
    memcpy(dest, src, n);
}

char *substr(char *str, char *substr_to_find) {
    return strstr(str, substr_to_find);
}

// Simple stubs for proxy_parse functions - you'll need the real implementation
struct ParsedRequest* ParsedRequest_create() {
    return (struct ParsedRequest*)calloc(1, sizeof(struct ParsedRequest));
}

void ParsedRequest_destroy(struct ParsedRequest *pr) {
    if(pr) {
        if(pr->buf) free(pr->buf);
        // Free other allocated fields as needed
        free(pr);
    }
}

int ParsedRequest_parse(struct ParsedRequest * parse, const char *buf, int buflen) {
    // Simplified parser - you'll need the real implementation
    return 0; // Stub return
}

int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, size_t buflen) {
    // Simplified implementation
    return 0; // Stub return
}

int ParsedHeader_set(struct ParsedRequest *pr, const char * key, const char * value) {
    // Simplified implementation
    return 0; // Stub return
}

struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, const char * key) {
    // Simplified implementation
    return NULL; // Stub return
}

#define MAX_CLIENTS 10
#define MAX_BYTES 10*(1<<10) //2^10=1024
#define MAX_ELEMENT_SIZE 10*(2<<10) 
#define MAX_CACHE_SIZE 200*(1<<20) // 200*(10^20)

// PHASE 2: Thread Pool Implementation
#define THREAD_POOL_SIZE 4    // Fixed number of worker threads
#define TASK_QUEUE_SIZE 50    // Maximum pending tasks

// PHASE 4: Optimized Cache Implementation
#define HASH_TABLE_SIZE 1024  // Hash table size for O(1) lookups
#define DEFAULT_TTL 3600      // Default TTL: 1 hour

// PHASE 5: Connection Pool Configuration
#define MAX_POOL_SIZE 20      // Maximum connections in pool
#define KEEP_ALIVE_TIMEOUT 30 // Keep-alive timeout in seconds

// Task structure for thread pool
typedef struct task {
    int client_socket;
    struct task* next;
} task_t;

// Thread pool structure
typedef struct {
    pthread_t workers[THREAD_POOL_SIZE];
    task_t* task_queue_head;
    task_t* task_queue_tail;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    int queue_size;
    int shutdown;
} thread_pool_t;

// PHASE 5: Connection pool structures
typedef struct {
    int socket_fd;
    char host[256];
    int port;
    time_t last_used;
    int in_use;
} connection_pool_entry_t;

typedef struct {
    connection_pool_entry_t* connections;
    int max_connections;
    int active_connections;
    pthread_mutex_t mutex;
} connection_pool_t;

// PHASE 4: Optimized cache structures
typedef struct cache_node {
    char* url;
    char* data;
    size_t data_size;
    int len;                      // Length field for compatibility
    time_t timestamp;
    time_t created_time;          // When entry was created
    time_t last_access;           // Last access time for LRU
    time_t ttl;                   // Time to live (expiration)
    struct cache_node* next;      // For hash collision chaining
    struct cache_node* lru_prev;  // For LRU doubly-linked list
    struct cache_node* lru_next;  // For LRU doubly-linked list
    struct cache_node* prev;      // Alternative name for lru_prev
    struct cache_node* hash_next; // Next node in hash chain
} cache_node_t;

typedef struct {
    cache_node_t** hash_table;     // Hash table for O(1) lookup
    cache_node_t* lru_head;        // Most recently used
    cache_node_t* lru_tail;        // Least recently used
    int max_cache_size;
    int current_size;
    int entry_count;               // Number of entries
    pthread_mutex_t cache_mutex;
} optimized_cache_t;

// ============== PHASE 5: CONNECTION POOL IMPLEMENTATION ==============

connection_pool_t* connection_pool_create(int max_connections) {
    connection_pool_t* pool = malloc(sizeof(connection_pool_t));
    if (!pool) return NULL;
    
    pool->connections = malloc(sizeof(connection_pool_entry_t) * max_connections);
    if (!pool->connections) {
        free(pool);
        return NULL;
    }
    
    pool->max_connections = max_connections;
    pool->active_connections = 0;
    
    for (int i = 0; i < max_connections; i++) {
        memset(&pool->connections[i], 0, sizeof(connection_pool_entry_t));
        pool->connections[i].socket_fd = -1;
    }
    
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool->connections);
        free(pool);
        return NULL;
    }
    
    printf("[POOL] Connection pool created with %d max connections\n", max_connections);
    return pool;
}

int create_persistent_connection(char* host_address, int port_number) {
    int remoteSocketID = socket(AF_INET, SOCK_STREAM, 0);
    if (remoteSocketID < 0) {
        print_socket_error("Failed to create a socket for the remote server");
        return -1;
    }

    struct hostent* host = gethostbyname(host_address);
    if (host == NULL) {
        print_socket_error("Failed to get the host");
        close(remoteSocketID);
        return -1;
    }

    struct sockaddr_in remote_address;
    bzero((char*)&remote_address, sizeof(remote_address));
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(port_number);
    bcopy((char*)&host->h_addr, (char*)&remote_address.sin_addr.s_addr, host->h_length);

    if (connect(remoteSocketID, (struct sockaddr*)&remote_address, (size_t)sizeof(remote_address)) < 0) {
        print_socket_error("Failed to connect to the remote server");
        close(remoteSocketID);
        return -1;
    }

    printf("[CONN] Persistent connection established to %s:%d (socket %d)\n", 
           host_address, port_number, remoteSocketID);
    return remoteSocketID;
}

int connection_pool_get(connection_pool_t* pool, char* host, int port) {
    if (!pool) return -1;
    
    pthread_mutex_lock(&pool->mutex);
    
    time_t current_time = time(NULL);
    
    // Look for existing connection to this host:port
    for (int i = 0; i < pool->max_connections; i++) {
        if (pool->connections[i].socket_fd > 0 &&
            strcmp(pool->connections[i].host, host) == 0 &&
            pool->connections[i].port == port) {
            
            // Check if connection is still valid and not timed out
            if (current_time - pool->connections[i].last_used < KEEP_ALIVE_TIMEOUT) {
                int socket_fd = pool->connections[i].socket_fd;
                pool->connections[i].last_used = current_time;
                pool->connections[i].in_use = 1;
                
                pthread_mutex_unlock(&pool->mutex);
                return socket_fd;
            } else {
                // Connection timed out, close it
                printf("[TIMEOUT] Connection to %s:%d timed out, closing socket %d\n", 
                       host, port, pool->connections[i].socket_fd);
                close(pool->connections[i].socket_fd);
                memset(&pool->connections[i], 0, sizeof(connection_pool_entry_t));
                pool->connections[i].socket_fd = -1;
                pool->active_connections--;
            }
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return -1; // No suitable connection found
}

void connection_pool_return(connection_pool_t* pool, int socket_fd, char* host, int port, int keep_alive) {
    if (!pool || socket_fd <= 0) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    if (!keep_alive) {
        // Client/server doesn't support keep-alive, close the connection
        close(socket_fd);
        printf("[CLOSE] Connection to %s:%d closed (no keep-alive)\n", host, port);
        pthread_mutex_unlock(&pool->mutex);
        return;
    }
    
    // Try to add connection back to pool
    for (int i = 0; i < pool->max_connections; i++) {
        if (pool->connections[i].socket_fd == socket_fd) {
            // Update existing entry
            pool->connections[i].last_used = time(NULL);
            pool->connections[i].in_use = 0;
            printf("[RETURN] Returned connection to %s:%d to pool (socket %d)\n", 
                   host, port, socket_fd);
            pthread_mutex_unlock(&pool->mutex);
            return;
        }
    }
    
    // Try to find empty slot for new connection
    if (pool->active_connections < pool->max_connections) {
        for (int i = 0; i < pool->max_connections; i++) {
            if (pool->connections[i].socket_fd <= 0) {
                pool->connections[i].socket_fd = socket_fd;
                strncpy(pool->connections[i].host, host, sizeof(pool->connections[i].host) - 1);
                pool->connections[i].host[sizeof(pool->connections[i].host) - 1] = '\0';
                pool->connections[i].port = port;
                pool->connections[i].last_used = time(NULL);
                pool->connections[i].in_use = 0;
                pool->active_connections++;
                
                printf("[ADD] Added new connection to %s:%d to pool (socket %d)\n", 
                       host, port, socket_fd);
                pthread_mutex_unlock(&pool->mutex);
                return;
            }
        }
    }
    
    // Pool is full, close the connection
    close(socket_fd);
    printf("[FULL] Connection pool full, closed connection to %s:%d\n", host, port);
    pthread_mutex_unlock(&pool->mutex);
}

void connection_pool_cleanup(connection_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    time_t current_time = time(NULL);
    int cleaned = 0;
    
    for (int i = 0; i < pool->max_connections; i++) {
        if (pool->connections[i].socket_fd > 0 &&
            !pool->connections[i].in_use &&
            current_time - pool->connections[i].last_used >= KEEP_ALIVE_TIMEOUT) {
            
            printf("[CLEANUP] Cleaning up timed out connection to %s:%d (socket %d)\n", 
                   pool->connections[i].host, pool->connections[i].port, 
                   pool->connections[i].socket_fd);
            
            close(pool->connections[i].socket_fd);
            memset(&pool->connections[i], 0, sizeof(connection_pool_entry_t));
            pool->connections[i].socket_fd = -1;
            pool->active_connections--;
            cleaned++;
        }
    }
    
    if (cleaned > 0) {
        printf("[CLEANUP] Cleaned up %d timed out connections\n", cleaned);
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

void connection_pool_destroy(connection_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    // Close all connections
    for (int i = 0; i < pool->max_connections; i++) {
        if (pool->connections[i].socket_fd > 0) {
            close(pool->connections[i].socket_fd);
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    
    free(pool->connections);
    free(pool);
    
    printf("[POOL] Connection pool destroyed\n");
}

// PHASE 2: Thread pool function declarations
void* worker_thread(void* arg);
thread_pool_t* thread_pool_create();
int thread_pool_add_task(thread_pool_t* pool, int client_socket);
void thread_pool_destroy(thread_pool_t* pool);
void *thread_fn(void *newSocket);

// PHASE 4: Optimized cache function declarations
optimized_cache_t* cache_create();
void cache_destroy(optimized_cache_t* cache);
cache_node_t* cache_find(optimized_cache_t* cache, const char* url);
int cache_add(optimized_cache_t* cache, const char* url, const char* data, int size);
void cache_remove_expired(optimized_cache_t* cache);
unsigned int hash_function(const char* str);

// PHASE 5: Connection pool function declarations
connection_pool_t* connection_pool_create(int max_connections);
void connection_pool_destroy(connection_pool_t* pool);
int connection_pool_get(connection_pool_t* pool, char* host, int port);
void connection_pool_return(connection_pool_t* pool, int socket_fd, char* host, int port, int keep_alive);
void connection_pool_cleanup(connection_pool_t* pool);
int create_persistent_connection(char* host, int port);

typedef struct cache_element cache_element;

// lru cache (one element with linked list)
struct cache_element{
    char *data;            // response data
    int len;               // length of data
    char *url;             // reuested url
    time_t lru_time_track; // tracks time for lru cache elemtns
    cache_element *next;
};

// to find the specific cache in memory (DEPRECATED - use cache_find)
cache_element *find(char *url);

// to add elements in cache (DEPRECATED - use cache_add)
int add_cache_element(char *data, int size, char *url);

// to remove cache elements
void remove_cache_elements();

int port_number = 8080;
int proxy_socketID; // socket id of the proxy server

// PHASE 2: Thread pool global variable
thread_pool_t* thread_pool;

// PHASE 4: Optimized cache global variable
optimized_cache_t* optimized_cache;

// PHASE 5: Connection pool global variable
connection_pool_t* connection_pool;

// cause this is a multithreaaded we need to create multiple threads for multiple clients with unique sockets.
// REMOVED: pthread_t tid[MAX_CLIENTS]; - replaced with thread pool

// semaphores for the cache
// will help to lock the cache when a thread is reading or writing to it
sem_t semaphore;
// mutex lock for the cache (DEPRECATED: replaced by cache_mutex in optimized_cache)
pthread_mutex_t lock;

//defining the cache head (DEPRECATED: replaced by optimized cache)
cache_element *cache_head = NULL;
int cache_size;//replace with the size of the cache


int connectRemoteServer(char* host_address,int port_number){
    // PHASE 5: Try to get connection from pool first
    int remoteSocketID = connection_pool_get(connection_pool, host_address, port_number);
    
    if(remoteSocketID > 0) {
        printf("[REUSE] Reusing pooled connection to %s:%d (socket %d)\n", 
               host_address, port_number, remoteSocketID);
        return remoteSocketID;
    }
    
    // Create new connection if not available in pool
    printf("[NEW] Creating new connection to %s:%d\n", host_address, port_number);
    remoteSocketID = create_persistent_connection(host_address, port_number);
    
    return remoteSocketID;
}

int handle_request(int clientSocketID,struct ParsedRequest *request,char* tempReq){
    char* buff = (char*)malloc(sizeof(char)*MAX_BYTES);
    if(buff == NULL) { // FIXED: Check malloc return value
        printf("Failed to allocate memory for request buffer\n");
        return -1;
    }
    strcpy(buff,"GET");
    strcat(buff,request->path);
    strcat(buff," ");
    strcat(buff,request->version);
    strcat(buff,"\r\n");
    size_t len = strlen(buff);

    // PHASE 5: Support keep-alive connections instead of forcing close
    const char* connection_header = ParsedHeader_get(request, "Connection") ? 
                                   ParsedHeader_get(request, "Connection")->value : "keep-alive";
    
    if(ParsedHeader_set(request,"Connection", connection_header) < 0){
        printf("Failed to set the connection header\n");
        free(buff); // FIXED: Free allocated memory before returning
        return -1;
    }
    
    printf("[CONN] Using connection mode: %s\n", connection_header);

    if(ParsedHeader_get(request,"Host") != NULL){
        if(ParsedHeader_set(request,"Host",request->host) < 0){
            printf("Failed to set the host\n");
            free(buff); // FIXED: Free allocated memory before returning
            return -1;
        }
    }

    if(ParsedRequest_unparse_headers(request,buff+len,(size_t)(MAX_BYTES)-len) < 0){
        printf("Failed to unparse the headers\n");
        printf(" or unparse headers is not working\n");
        free(buff); // FIXED: Free allocated memory before returning
        return -1;
    }

    int server_port = 80;//http always runs on port 80
    if(request->port != NULL){
        server_port = atoi(request->port);
    }
    
    // PHASE 5: Use connection pool instead of direct connection
    int remoteSocketID = connectRemoteServer(request->host,server_port);

    //for safety check
    if(remoteSocketID < 0){
        printf("Failed to connect to the remote server\n");
        free(buff); // FIXED: Free allocated memory before returning
        return -1;
    }


    int bytes_to_send = send(remoteSocketID,buff,strlen(buff),0);
    bzero(buff,MAX_BYTES);

    if(bytes_to_send < 0){
        printf("Failed to send the request to the remote server\n");
        free(buff); // FIXED: Free allocated memory before returning
        return -1;
    }

    int bytes_received = recv(remoteSocketID,buff,MAX_BYTES-1,0);//-1 because to not read the last byte that is "/0"

    if(bytes_received < 0){
        printf("Failed to receive the response from the remote server\n");
        free(buff); // FIXED: Free allocated memory before returning
        return -1;
    }


    char* tempBuff = (char *)malloc(sizeof(char)*MAX_BYTES);
    if(tempBuff == NULL) { // FIXED: Check malloc return value
        printf("Failed to allocate memory for tempBuff\n");
        free(buff);
        return -1;
    }

    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;
    int server_keep_alive = 0; // PHASE 5: Track server keep-alive support

    while(bytes_received > 0){
        bytes_received = send(clientSocketID,buff,bytes_received,0);
        //if the data is not sent then we need to receive the data again
        for(int i=0;i<bytes_received/sizeof(char);i++){
            // FIXED: Check bounds to prevent buffer overflow
            if(temp_buffer_index < temp_buffer_size - 1) {
                tempBuff[temp_buffer_index] = buff[i];
                temp_buffer_index++;
            }
        }
        
        // PHASE 5: Check for keep-alive in server response
        if(strstr(buff, "Connection: keep-alive") != NULL) {
            server_keep_alive = 1;
            printf("[KEEPALIVE] Server supports keep-alive\n");
        }
        
        temp_buffer_size += bytes_received;
        char* new_buff = (char*)realloc(tempBuff, temp_buffer_size); // FIXED: Check realloc return
        if(new_buff == NULL) {
            printf("Failed to reallocate memory\n");
            free(tempBuff);
            free(buff);
            return -1;
        }
        tempBuff = new_buff;

        if(bytes_received < 0){
            printf("Failed to send the response to the client\n");
            break;
        }

        bzero(buff,MAX_BYTES);
        bytes_received = recv(remoteSocketID,buff,MAX_BYTES-1,0);

    }
    
    tempBuff[temp_buffer_index] = '\0';//null terminating the buffer
    free(buff);
    
    // PHASE 4: Add to both caches (transitional period)
    add_cache_element(tempBuff,strlen(tempBuff),tempReq); // Legacy cache
    cache_add(optimized_cache, tempReq, tempBuff, strlen(tempBuff)); // Optimized cache
    
    free(tempBuff);
    
    // PHASE 5: Return connection to pool instead of closing
    connection_pool_return(connection_pool, remoteSocketID, request->host, server_port, server_keep_alive);
    
    return 0;
}


int checkHTTPversion(char *version){
    if(version == NULL){
        return 0;
    }
    if(strcmp(version,"HTTP/1.0") == 0){
        return 1;
    }
    if(strcmp(version,"HTTP/1.1") == 0){
        return 1;
    }
    return 0;
}


void sendErrorMessages(int socket, int error_code){
    char *error_message = (char*)malloc(sizeof(char)*MAX_BYTES);
    bzero(error_message,MAX_BYTES);
    if(error_code == 400){
        strcpy(error_message,"HTTP/1.1 400 Bad Request\r\n\r\n");
    }else if(error_code == 500){
        strcpy(error_message,"HTTP/1.1 500 Internal Server Error\r\n\r\n");
    }else if(error_code == 501){
        strcpy(error_message,"HTTP/1.1 501 Not Implemented\r\n\r\n");
    }else if(error_code == 502){
        strcpy(error_message,"HTTP/1.1 502 Bad Gateway\r\n\r\n");
    }else if(error_code == 503){
        strcpy(error_message,"HTTP/1.1 503 Service Unavailable\r\n\r\n");
    }else if(error_code == 504){
        strcpy(error_message,"HTTP/1.1 504 Gateway Timeout\r\n\r\n");
    }else if(error_code == 505){
        strcpy(error_message,"HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n");
    }else{
        strcpy(error_message,"HTTP/1.1 400 Bad Request\r\n\r\n");
    }

    printf("%s", error_message);
    send(socket, error_message, strlen(error_message), 0); // FIXED: Actually send the error message
    free(error_message);

    return; // FIXED: void function should not return a value
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
    if(tempReq == NULL) { // FIXED: Check malloc return value
        printf("Failed to allocate memory for tempReq\n");
        free(buffer);
        sem_post(&semaphore);
        return NULL;
    }
    for(int i=0;i<strlen(buffer);i++){
        tempReq[i] = buffer[i];
    }
    tempReq[strlen(buffer)] = '\0'; // FIXED: Proper null termination


    struct cache_element *temp = find(tempReq);//search the cache for the request
    
    // PHASE 4: Also try optimized cache (transitional - both caches active)
    cache_node_t *optimized_temp = cache_find(optimized_cache, tempReq);
    
    if(temp != NULL || optimized_temp != NULL){
        //if the request is found in the cache
        //then we can send the data to the client
        
        // PHASE 4: Use optimized cache if available, fallback to old cache
        char* cache_data;
        int cache_len;
        
        if(optimized_temp != NULL) {
            printf("✅ OPTIMIZED CACHE HIT!\n");
            cache_data = optimized_temp->data;
            cache_len = optimized_temp->len;
        } else {
            printf("📁 Legacy cache hit\n");
            cache_data = temp->data;
            cache_len = temp->len;
        }
        
        int size = cache_len/sizeof(char); //size of the data
        //exaample: if the data is "hello" then size(actual) will be 20  and if char is of 4 bytes then size will be 5(characters we can say)
        int pos = 0;
        char response[MAX_BYTES];
        while(pos<size){
            bzero(response,MAX_BYTES);
            for(int i=0;i<MAX_BYTES && pos<size;i++){
                response[i] = cache_data[pos];
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
        }
    }

    // FIXED: Cleanup and return properly
    shutdown(socket, SHUT_RDWR);  // PHASE 3: Use cross-platform define
    close(socket);
    free(buffer);
    free(tempReq);
    sem_post(&semaphore);//sem_signal
    int sem_val; // FIXED: Different variable name to avoid redeclaration
    sem_getvalue(&semaphore, &sem_val);
    printf("Semaphore post value is: %d\n", sem_val);
    return NULL;

}


int main(int argc, char *argv[]){
    int client_socketdID, client_length; // client socket id and length
    struct sockaddr_in server_address, client_address; // server and client address

    // PHASE 3: Initialize platform-specific networking
    init_winsock();

    //initializing the semaphore (FIXED: correct parameters)
    // sem_init(semaphore, pshared, value)
    // pshared=0 for threads in same process, value=MAX_CLIENTS for available slots
    sem_init(&semaphore, 0, MAX_CLIENTS);
    
    //initialize the mutex lock
    pthread_mutex_init(&lock, NULL);

    // PHASE 2: Initialize thread pool
    thread_pool = thread_pool_create();
    if(thread_pool == NULL) {
        printf("Failed to create thread pool\n");
        exit(1);
    }

    // PHASE 4: Initialize optimized cache
    printf("DEBUG: Creating optimized cache...\n");
    optimized_cache = cache_create();
    if(optimized_cache == NULL) {
        printf("Failed to create optimized cache\n");
        exit(1);
    }
    printf("DEBUG: Optimized cache created successfully\n");

    // PHASE 5: Initialize connection pool
    printf("DEBUG: Creating connection pool...\n");
    connection_pool = connection_pool_create(MAX_POOL_SIZE);
    if(connection_pool == NULL) {
        printf("Failed to create connection pool\n");
        exit(1);
    }
    printf("DEBUG: Connection pool created successfully\n");

    //check if the port number is provided
    if(argc == 2){ // FIXED: Use argc instead of argv for argument count
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
        print_socket_error("Failed to create a socket"); // PHASE 3: Cross-platform error
        exit(1);
    }

    //if created then we can reuse it..
    int resuse = 1;
    if(setsockopt(proxy_socketID, SOL_SOCKET, SO_REUSEADDR, (const char*)&resuse, sizeof(resuse)) < 0){
        print_socket_error("Failed to set socket options : (setsockopt-failed)"); // PHASE 3: Cross-platform error
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
    // REMOVED: int Connected_socketID[MAX_CLIENTS]; - no longer needed with thread pool

    while(1){
        bzero((char*)&client_address, sizeof(client_address));//cleaned the client address and removed the garbage values
        client_length = sizeof(client_address);//storing the final length of the client address

        client_socketdID = accept(proxy_socketID,(struct sockaddr*)&client_address, (socklen_t*)&client_length);//accepting the client connection

        if(client_socketdID < 0){
            print_socket_error("Failed to accept the client connection"); // PHASE 3: Cross-platform error
            continue; // PHASE 2: Continue instead of exit for better resilience
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

        // PHASE 2: Use thread pool instead of creating new thread per connection
        if(thread_pool_add_task(thread_pool, client_socketdID) != 0) {
            printf("Failed to add task to thread pool - closing connection\n");
            close(client_socketdID);
        }
        
        client_count++;
    }

    // PHASE 2: Cleanup thread pool (this code won't normally be reached due to infinite loop)
    thread_pool_destroy(thread_pool);
    
    // PHASE 4: Cleanup optimized cache
    cache_destroy(optimized_cache);
    
    // PHASE 5: Cleanup connection pool
    connection_pool_destroy(connection_pool);
    
    close(proxy_socketID);
    
    // PHASE 3: Cleanup platform-specific networking
    cleanup_winsock();
    return 0;
} 

cache_element *find(char *url){
    cache_element *site = NULL;
    int temp_lock_status = pthread_mutex_lock(&lock);//lock the cache
    if(temp_lock_status != 0){
        perror("Failed to lock the cache\n");
        return NULL;
    }

    printf("Remove cache lock acquired %d\n",temp_lock_status);
    
    if(cache_head != NULL){
        site = cache_head;
        while(site != NULL){
            if(strcmp(site->url,url) == 0){
                printf("LRU Time Track before: %lld\n",(long long)site->lru_time_track); // FIXED: Correct format specifier
                printf("Cache hit or Cache Found!\n");
                site->lru_time_track = time(NULL);
                printf("LRU Time Track after: %lld\n",(long long)site->lru_time_track); // FIXED: Correct format specifier
                break;
            }
            site = site->next;
        }
    }else{
        printf("Cache is empty or URL not found\n");
    }

    temp_lock_status = pthread_mutex_unlock(&lock);//unlock the cache
    if(temp_lock_status != 0){
        perror("Failed to unlock the cache\n");
        return NULL;
    }
    printf("Remove cache lock released %d\n",temp_lock_status);
    return site;
}

//cache is full so we have to remove elements from cache.
void remove_cache_element(){
    //two pointers to keep track of the cache
    cache_element *p;
    cache_element *q;

    //to keep track of the least recently used element and delete some node
    cache_element *temp;

    int temp_lock_val = pthread_mutex_lock(&lock);//lock the cache
    if(temp_lock_val != 0){
        perror("Failed to lock the cache\n");
        return;
    }
    printf("Remove cache lock acquired %d\n",temp_lock_val);

    if(cache_head!=NULL){
        // FIXED: Initialize temp properly
        temp = cache_head; // Start with first element as the least recently used
        
        for(p=cache_head,q=cache_head; q->next!=NULL; q=q->next){
            if((q->next)->lru_time_track < temp->lru_time_track){
                temp = q->next;
                p=q;
            }
        }

        if(temp == cache_head){
            cache_head = cache_head->next;
        }else{
            p->next = temp->next;
        }
        //if cache is not empty then searches for th node which has the least lru_time_track and deletes it

        cache_size = cache_size - (temp->len + strlen(temp->url) + sizeof(cache_element));
        free(temp->data);
        free(temp->url);
        free(temp);
    }

    temp_lock_val = pthread_mutex_unlock(&lock);
    if(temp_lock_val != 0){
        perror("Failed to unlock the cache\n");
        return;
    }
    printf("Remove cache lock released %d\n",temp_lock_val);
    return;
}



// //self, check once again if error is there
// int add_cache_element(char *data, int size, char *url){
//     cache_element *new_element = (cache_element*)malloc(sizeof(cache_element));
//     new_element->data = (char*)malloc(sizeof(char)*size);
//     new_element->url = (char*)malloc(sizeof(char)*strlen(url));
//     new_element->len = size;
//     new_element->lru_time_track = time(NULL);
//     new_element->next = NULL;

//     for(int i=0;i<size;i++){
//         new_element->data[i] = data[i];
//     }

//     for(int i=0;i<strlen(url);i++){
//         new_element->url[i] = url[i];
//     }

//     int temp_lock_status = pthread_mutex_lock(&lock);//lock the cache
//     if(temp_lock_status != 0){
//         perror("Failed to lock the cache\n");
//         return -1;
//     }

//     printf("Add cache lock acquired %d\n",temp_lock_status);

//     if(cache_head == NULL){
//         cache_head = new_element;
//     }else{
//         cache_element *temp = cache_head;
//         while(temp->next != NULL){
//             temp = temp->next;
//         }
//         temp->next = new_element;
//     }

//     temp_lock_status = pthread_mutex_unlock(&lock);//unlock the cache
//     if(temp_lock_status != 0){
//         perror("Failed to unlock the cache\n");
//         return -1;
//     }
//     printf("Add cache lock released %d\n",temp_lock_status);
//     return 0;
// }


int add_cache_element(char *data,int size,char *url){
    int temp_lock_status = pthread_mutex_lock(&lock);
    printf("Add cache lock acquired %d\n",temp_lock_status);
    int element_size = size+1+strlen(url)+sizeof(cache_element);

    // FIXED: Logic error - should cache if element is SMALLER than max size
    if(element_size > MAX_ELEMENT_SIZE){
        temp_lock_status = pthread_mutex_unlock(&lock);
        printf("Element too large to cache - Add cache lock released/unlocked %d\n",temp_lock_status);
        return 0;
    }else{
        while(cache_size + element_size > MAX_CACHE_SIZE){
            remove_cache_element(); // FIXED: Correct function name
        }

        cache_element *new_element = (cache_element*)malloc(sizeof(cache_element));
        new_element->data = (char*)malloc(sizeof(char)*(size+1));
        strcpy(new_element->data,data);
        new_element->url = (char*)malloc(1+(strlen(url)*sizeof(char)));
        strcpy(new_element->url,url);
        new_element->lru_time_track = time(NULL);
        new_element->next = cache_head;
        new_element->len = size;
        cache_head = new_element;

        cache_size += element_size;

        temp_lock_status = pthread_mutex_unlock(&lock);//unlock the cache
        printf("Add cache lock released/unlocked %d\n",temp_lock_status);
    }
    return 0;
}

// PHASE 2: Thread Pool Implementation Functions

// Worker thread function - continuously processes tasks from queue
void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    
    while(1) {
        pthread_mutex_lock(&pool->queue_mutex);
        
        // Wait for tasks or shutdown signal
        while(pool->task_queue_head == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }
        
        // Check for shutdown
        if(pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        // Get task from queue
        task_t* task = pool->task_queue_head;
        pool->task_queue_head = task->next;
        if(pool->task_queue_head == NULL) {
            pool->task_queue_tail = NULL;
        }
        pool->queue_size--;
        
        pthread_mutex_unlock(&pool->queue_mutex);
        
        // Process the task (handle client connection)
        thread_fn((void*)&task->client_socket);
        
        // PHASE 5: Periodic connection pool cleanup (every 50 requests)
        static int cleanup_counter = 0;
        cleanup_counter++;
        if (cleanup_counter % 50 == 0) {
            connection_pool_cleanup(connection_pool);
            cleanup_counter = 0;
        }
        
        // Clean up task
        free(task);
    }
    
    return NULL;
}

// Initialize thread pool
thread_pool_t* thread_pool_create() {
    thread_pool_t* pool = (thread_pool_t*)malloc(sizeof(thread_pool_t));
    if(pool == NULL) {
        printf("Failed to allocate thread pool\n");
        return NULL;
    }
    
    // Initialize pool
    pool->task_queue_head = NULL;
    pool->task_queue_tail = NULL;
    pool->queue_size = 0;
    pool->shutdown = 0;
    
    // Initialize synchronization primitives
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_cond, NULL);
    
    // Create worker threads
    for(int i = 0; i < THREAD_POOL_SIZE; i++) {
        if(pthread_create(&pool->workers[i], NULL, worker_thread, pool) != 0) {
            printf("Failed to create worker thread %d\n", i);
            // TODO: Cleanup on failure
            return NULL;
        }
    }
    
    printf("[POOL] Thread pool created with %d worker threads\n", THREAD_POOL_SIZE);
    return pool;
}

// Add task to thread pool queue
int thread_pool_add_task(thread_pool_t* pool, int client_socket) {
    if(pool == NULL) return -1;
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    // Check if queue is full
    if(pool->queue_size >= TASK_QUEUE_SIZE) {
        pthread_mutex_unlock(&pool->queue_mutex);
        printf("❌ Task queue full - rejecting client\n");
        return -1;
    }
    
    // Create new task
    task_t* task = (task_t*)malloc(sizeof(task_t));
    if(task == NULL) {
        pthread_mutex_unlock(&pool->queue_mutex);
        return -1;
    }
    
    task->client_socket = client_socket;
    task->next = NULL;
    
    // Add to queue
    if(pool->task_queue_tail == NULL) {
        pool->task_queue_head = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }
    
    pool->queue_size++;
    
    // Signal worker threads
    pthread_cond_signal(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    printf("📋 Task added to queue (queue size: %d)\n", pool->queue_size);
    return 0;
}

// Shutdown thread pool
void thread_pool_destroy(thread_pool_t* pool) {
    if(pool == NULL) return;
    
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Wait for all threads to finish
    for(int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(pool->workers[i], NULL);
    }
    
    // Cleanup remaining tasks
    while(pool->task_queue_head != NULL) {
        task_t* task = pool->task_queue_head;
        pool->task_queue_head = task->next;
        close(task->client_socket);
        free(task);
    }
    
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    free(pool);
    
    printf("🛑 Thread pool destroyed\n");
}

// PHASE 4: Optimized Cache Implementation with Hash Table + Doubly-Linked List

// Simple hash function for URL strings
unsigned int hash_function(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % HASH_TABLE_SIZE;
}

// Create optimized cache
optimized_cache_t* cache_create() {
    optimized_cache_t* cache = (optimized_cache_t*)calloc(1, sizeof(optimized_cache_t));
    if (cache == NULL) {
        printf("Failed to allocate optimized cache\n");
        return NULL;
    }
    
    // Allocate hash table
    cache->hash_table = (cache_node_t**)calloc(HASH_TABLE_SIZE, sizeof(cache_node_t*));
    if (cache->hash_table == NULL) {
        printf("Failed to allocate cache hash table\n");
        free(cache);
        return NULL;
    }
    
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->current_size = 0;
    cache->entry_count = 0;
    cache->max_cache_size = MAX_CACHE_SIZE;
    
    if (pthread_mutex_init(&cache->cache_mutex, NULL) != 0) {
        free(cache->hash_table);
        free(cache);
        return NULL;
    }
    
    printf("[CACHE] Optimized cache created with hash table (O(1) lookups)\n");
    return cache;
}

// Remove node from LRU list
static void remove_from_lru(optimized_cache_t* cache, cache_node_t* node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        cache->lru_head = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        cache->lru_tail = node->prev;
    }
}

// Add node to front of LRU list (most recently used)
static void add_to_lru_front(optimized_cache_t* cache, cache_node_t* node) {
    node->prev = NULL;
    node->next = cache->lru_head;
    
    if (cache->lru_head) {
        cache->lru_head->prev = node;
    } else {
        cache->lru_tail = node;
    }
    
    cache->lru_head = node;
}

// Move node to front (mark as most recently used)
static void move_to_front(optimized_cache_t* cache, cache_node_t* node) {
    if (cache->lru_head == node) return; // Already at front
    
    remove_from_lru(cache, node);
    add_to_lru_front(cache, node);
}

// Find cache entry - O(1) average case
cache_node_t* cache_find(optimized_cache_t* cache, const char* url) {
    if (cache == NULL || url == NULL) return NULL;
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    unsigned int hash = hash_function(url);
    cache_node_t* node = cache->hash_table[hash];
    
    // Search hash chain
    while (node != NULL) {
        if (strcmp(node->url, url) == 0) {
            // Check if expired
            time_t now = time(NULL);
            if (now > node->created_time + node->ttl) {
                printf("🕐 Cache entry expired for URL: %s\n", url);
                pthread_mutex_unlock(&cache->cache_mutex);
                return NULL;
            }
            
            // Update access time and move to front (LRU)
            node->last_access = now;
            move_to_front(cache, node);
            
            printf("🎯 OPTIMIZED CACHE HIT! O(1) lookup for: %s\n", url);
            pthread_mutex_unlock(&cache->cache_mutex);
            return node;
        }
        node = node->hash_next;
    }
    
    printf("❌ Cache miss for URL: %s\n", url);
    pthread_mutex_unlock(&cache->cache_mutex);
    return NULL;
}

// Remove LRU entries when cache is full
static void evict_lru_entries(optimized_cache_t* cache, int bytes_needed) {
    while (cache->current_size + bytes_needed > MAX_CACHE_SIZE && cache->lru_tail != NULL) {
        cache_node_t* victim = cache->lru_tail;
        
        printf("🗑️ Evicting LRU entry: %s\n", victim->url);
        
        // Remove from LRU list
        remove_from_lru(cache, victim);
        
        // Remove from hash table
        unsigned int hash = hash_function(victim->url);
        cache_node_t** current = &cache->hash_table[hash];
        while (*current != NULL) {
            if (*current == victim) {
                *current = victim->hash_next;
                break;
            }
            current = &(*current)->hash_next;
        }
        
        // Update cache stats
        cache->current_size -= (victim->len + strlen(victim->url) + sizeof(cache_node_t));
        cache->entry_count--;
        
        // Free memory
        free(victim->url);
        free(victim->data);
        free(victim);
    }
}

// Add entry to cache - O(1) average case
int cache_add(optimized_cache_t* cache, const char* url, const char* data, int size) {
    if (cache == NULL || url == NULL || data == NULL) return -1;
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    // Check if entry already exists
    if (cache_find(cache, url) != NULL) {
        pthread_mutex_unlock(&cache->cache_mutex);
        return 0; // Already exists
    }
    
    int total_size = size + strlen(url) + sizeof(cache_node_t);
    
    // Check if entry is too large
    if (total_size > MAX_ELEMENT_SIZE) {
        printf("❌ Entry too large to cache: %s (%d bytes)\n", url, total_size);
        pthread_mutex_unlock(&cache->cache_mutex);
        return -1;
    }
    
    // Evict LRU entries if needed
    evict_lru_entries(cache, total_size);
    
    // Create new node
    cache_node_t* node = (cache_node_t*)malloc(sizeof(cache_node_t));
    if (node == NULL) {
        pthread_mutex_unlock(&cache->cache_mutex);
        return -1;
    }
    
    // Allocate and copy data
    node->url = strdup(url);
    node->data = malloc(size + 1);
    memcpy(node->data, data, size);
    node->data[size] = '\0';
    node->len = size;
    node->created_time = time(NULL);
    node->last_access = node->created_time;
    node->ttl = DEFAULT_TTL;
    
    // Add to hash table
    unsigned int hash = hash_function(url);
    node->hash_next = cache->hash_table[hash];
    cache->hash_table[hash] = node;
    
    // Add to front of LRU list
    add_to_lru_front(cache, node);
    
    // Update cache stats
    cache->current_size += total_size;
    cache->entry_count++;
    
    printf("✅ Added to optimized cache: %s (%d bytes, %d total entries)\n", 
           url, total_size, cache->entry_count);
    
    pthread_mutex_unlock(&cache->cache_mutex);
    return 0;
}

// Remove expired entries
void cache_remove_expired(optimized_cache_t* cache) {
    if (cache == NULL) return;
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    time_t now = time(NULL);
    cache_node_t* current = cache->lru_head;
    int removed_count = 0;
    
    while (current != NULL) {
        cache_node_t* next = current->next;
        
        if (now > current->created_time + current->ttl) {
            // Remove expired entry
            remove_from_lru(cache, current);
            
            // Remove from hash table
            unsigned int hash = hash_function(current->url);
            cache_node_t** hash_current = &cache->hash_table[hash];
            while (*hash_current != NULL) {
                if (*hash_current == current) {
                    *hash_current = current->hash_next;
                    break;
                }
                hash_current = &(*hash_current)->hash_next;
            }
            
            cache->current_size -= (current->len + strlen(current->url) + sizeof(cache_node_t));
            cache->entry_count--;
            
            free(current->url);
            free(current->data);
            free(current);
            removed_count++;
        }
        
        current = next;
    }
    
    if (removed_count > 0) {
        printf("🧹 Removed %d expired cache entries\n", removed_count);
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
}

// Destroy cache
void cache_destroy(optimized_cache_t* cache) {
    if (cache == NULL) return;
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    // Free all nodes
    cache_node_t* current = cache->lru_head;
    while (current != NULL) {
        cache_node_t* next = current->next;
        free(current->url);
        free(current->data);
        free(current);
        current = next;
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
    pthread_mutex_destroy(&cache->cache_mutex);
    free(cache);
    
    printf("🛑 Optimized cache destroyed\n");
}