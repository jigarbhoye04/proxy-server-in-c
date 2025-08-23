#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <time.h>

// PHASE 6: Cache Module
// Optimized O(1) hash table cache with LRU eviction

#define CACHE_SIZE 1024
#define HASH_TABLE_SIZE 1024
#define CACHE_EXPIRY_TIME 300  // 5 minutes

// Cache node structure for hash table + LRU
typedef struct cache_node {
    char* url;                    // Request URL (key)
    char* data;                   // Cached response data
    int data_size;                // Size of cached data
    time_t timestamp;             // When cached
    int access_count;             // Access frequency
    
    struct cache_node* next;      // For hash collision chaining
    struct cache_node* lru_prev;  // For LRU doubly-linked list
    struct cache_node* lru_next;  // For LRU doubly-linked list
} cache_node_t;

// Optimized cache structure
typedef struct {
    cache_node_t* hash_table[HASH_TABLE_SIZE];  // Hash table for O(1) lookup
    cache_node_t* lru_head;                     // Most recently used
    cache_node_t* lru_tail;                     // Least recently used
    pthread_mutex_t cache_mutex;
    int current_size;
    int max_size;
} optimized_cache_t;

// Cache management functions
optimized_cache_t* cache_create(void);
cache_node_t* cache_get(optimized_cache_t* cache, const char* url);
int cache_add(optimized_cache_t* cache, const char* url, const char* data, int size);
void cache_remove_expired(optimized_cache_t* cache);
void cache_destroy(optimized_cache_t* cache);

// Cache utilities
unsigned int cache_hash(const char* url);
void cache_move_to_front(optimized_cache_t* cache, cache_node_t* node);
void cache_remove_lru(optimized_cache_t* cache);

#endif // CACHE_H
