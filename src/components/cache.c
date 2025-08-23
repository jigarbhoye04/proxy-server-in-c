#include "../../include/proxy/cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Cache Implementation

// Hash function for URLs
unsigned int cache_hash(const char* url) {
    unsigned int hash = 5381;
    int c;
    
    while ((c = *url++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % HASH_TABLE_SIZE;
}

optimized_cache_t* cache_create(void) {
    optimized_cache_t* cache = malloc(sizeof(optimized_cache_t));
    if (!cache) {
        printf("[CACHE] Failed to allocate memory for cache\n");
        return NULL;
    }
    
    // Initialize hash table
    cache->hash_table = malloc(sizeof(cache_node_t*) * HASH_TABLE_SIZE);
    if (!cache->hash_table) {
        printf("[CACHE] Failed to allocate memory for hash table\n");
        free(cache);
        return NULL;
    }
    
    // Initialize hash table slots
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        cache->hash_table[i] = NULL;
    }
    
    // Initialize LRU list
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->current_size = 0;
    cache->max_size = CACHE_SIZE;
    
    // Initialize mutex
    if (pthread_mutex_init(&cache->cache_mutex, NULL) != 0) {
        printf("[CACHE] Failed to initialize cache mutex\n");
        free(cache->hash_table);
        free(cache);
        return NULL;
    }
    
    printf("[CACHE] Optimized cache created with hash table (O(1) lookups)\n");
    return cache;
}

cache_node_t* cache_get(optimized_cache_t* cache, const char* url) {
    if (!cache || !url) {
        return NULL;
    }
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    unsigned int hash = cache_hash(url);
    cache_node_t* node = cache->hash_table[hash];
    
    // Search in hash chain
    while (node) {
        if (strcmp(node->url, url) == 0) {
            // Check if not expired
            time_t current_time = time(NULL);
            if (current_time - node->timestamp < CACHE_EXPIRY_TIME) {
                // Move to front of LRU list
                cache_move_to_front(cache, node);
                node->access_count++;
                
                printf("[CACHE] Cache hit for URL: %.50s...\n", url);
                pthread_mutex_unlock(&cache->cache_mutex);
                return node;
            } else {
                // Entry expired, will be removed
                printf("[CACHE] Cache entry expired for URL: %.50s...\n", url);
                break;
            }
        }
        node = node->next;
    }
    
    printf("[CACHE] Cache miss for URL: %.50s...\n", url);
    pthread_mutex_unlock(&cache->cache_mutex);
    return NULL;
}

int cache_add(optimized_cache_t* cache, const char* url, const char* data, int size) {
    if (!cache || !url || !data || size <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    // Check if we need to make space
    if (cache->current_size >= cache->max_size) {
        cache_remove_lru(cache);
    }
    
    // Create new cache node
    cache_node_t* node = malloc(sizeof(cache_node_t));
    if (!node) {
        printf("[CACHE] Failed to allocate memory for cache node\n");
        pthread_mutex_unlock(&cache->cache_mutex);
        return -1;
    }
    
    // Allocate and copy URL
    node->url = malloc(strlen(url) + 1);
    if (!node->url) {
        printf("[CACHE] Failed to allocate memory for URL\n");
        free(node);
        pthread_mutex_unlock(&cache->cache_mutex);
        return -1;
    }
    strcpy(node->url, url);
    
    // Allocate and copy data
    node->data = malloc(size);
    if (!node->data) {
        printf("[CACHE] Failed to allocate memory for data\n");
        free(node->url);
        free(node);
        pthread_mutex_unlock(&cache->cache_mutex);
        return -1;
    }
    memcpy(node->data, data, size);
    
    node->data_size = size;
    node->timestamp = time(NULL);
    node->access_count = 1;
    
    // Add to hash table
    unsigned int hash = cache_hash(url);
    node->next = cache->hash_table[hash];
    cache->hash_table[hash] = node;
    
    // Add to front of LRU list
    node->lru_prev = NULL;
    node->lru_next = cache->lru_head;
    
    if (cache->lru_head) {
        cache->lru_head->lru_prev = node;
    } else {
        cache->lru_tail = node;
    }
    cache->lru_head = node;
    
    cache->current_size++;
    
    printf("[CACHE] Added entry for URL: %.50s... (size: %d bytes)\n", url, size);
    pthread_mutex_unlock(&cache->cache_mutex);
    return 0;
}

void cache_move_to_front(optimized_cache_t* cache, cache_node_t* node) {
    if (!cache || !node || node == cache->lru_head) {
        return; // Already at front or invalid
    }
    
    // Remove from current position
    if (node->lru_prev) {
        node->lru_prev->lru_next = node->lru_next;
    }
    if (node->lru_next) {
        node->lru_next->lru_prev = node->lru_prev;
    } else {
        cache->lru_tail = node->lru_prev;
    }
    
    // Move to front
    node->lru_prev = NULL;
    node->lru_next = cache->lru_head;
    
    if (cache->lru_head) {
        cache->lru_head->lru_prev = node;
    } else {
        cache->lru_tail = node;
    }
    cache->lru_head = node;
}

void cache_remove_lru(optimized_cache_t* cache) {
    if (!cache || !cache->lru_tail) {
        return;
    }
    
    cache_node_t* lru_node = cache->lru_tail;
    
    // Remove from LRU list
    if (lru_node->lru_prev) {
        lru_node->lru_prev->lru_next = NULL;
        cache->lru_tail = lru_node->lru_prev;
    } else {
        cache->lru_head = NULL;
        cache->lru_tail = NULL;
    }
    
    // Remove from hash table
    unsigned int hash = cache_hash(lru_node->url);
    cache_node_t* current = cache->hash_table[hash];
    cache_node_t* prev = NULL;
    
    while (current) {
        if (current == lru_node) {
            if (prev) {
                prev->next = current->next;
            } else {
                cache->hash_table[hash] = current->next;
            }
            break;
        }
        prev = current;
        current = current->next;
    }
    
    printf("[CACHE] Removed LRU entry for URL: %.50s...\n", lru_node->url);
    
    // Free memory
    free(lru_node->url);
    free(lru_node->data);
    free(lru_node);
    
    cache->current_size--;
}

void cache_remove_expired(optimized_cache_t* cache) {
    if (!cache) return;
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    time_t current_time = time(NULL);
    int removed = 0;
    
    // Check all hash table entries
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        cache_node_t* current = cache->hash_table[i];
        cache_node_t* prev = NULL;
        
        while (current) {
            if (current_time - current->timestamp >= CACHE_EXPIRY_TIME) {
                // Remove expired entry
                cache_node_t* expired = current;
                
                // Remove from hash chain
                if (prev) {
                    prev->next = current->next;
                } else {
                    cache->hash_table[i] = current->next;
                }
                
                // Remove from LRU list
                if (expired->lru_prev) {
                    expired->lru_prev->lru_next = expired->lru_next;
                } else {
                    cache->lru_head = expired->lru_next;
                }
                
                if (expired->lru_next) {
                    expired->lru_next->lru_prev = expired->lru_prev;
                } else {
                    cache->lru_tail = expired->lru_prev;
                }
                
                // Free memory
                free(expired->url);
                free(expired->data);
                free(expired);
                
                cache->current_size--;
                removed++;
                
                current = prev ? prev->next : cache->hash_table[i];
            } else {
                prev = current;
                current = current->next;
            }
        }
    }
    
    if (removed > 0) {
        printf("[CACHE] Removed %d expired entries\n", removed);
    }
    
    pthread_mutex_unlock(&cache->cache_mutex);
}

void cache_destroy(optimized_cache_t* cache) {
    if (!cache) return;
    
    printf("[CACHE] Destroying cache...\n");
    
    pthread_mutex_lock(&cache->cache_mutex);
    
    // Free all cache entries
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        cache_node_t* current = cache->hash_table[i];
        while (current) {
            cache_node_t* next = current->next;
            free(current->url);
            free(current->data);
            free(current);
            current = next;
        }
    }
    
    // Free hash table
    free(cache->hash_table);
    
    pthread_mutex_unlock(&cache->cache_mutex);
    
    // Destroy mutex
    pthread_mutex_destroy(&cache->cache_mutex);
    
    // Free cache structure
    free(cache);
    
    printf("[CACHE] Cache destroyed\n");
}
