#include "mec_common.h"
#include "mec_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define POOL_BLOCK_SIZE 1024
#define POOL_NUM_BLOCKS 16

typedef struct {
    uint8_t data[POOL_BLOCK_SIZE];
    int used;
} pool_block_t;

static pool_block_t memory_pool[POOL_NUM_BLOCKS];
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

void mec_pool_init() {
    for (int i = 0; i < POOL_NUM_BLOCKS; i++) {
        memory_pool[i].used = 0;
    }
    LOG_INFO("Memory Pool: Initialized with %d blocks (Block size: %zu bytes)", 
             POOL_NUM_BLOCKS, POOL_BLOCK_SIZE);
}

void* mec_malloc(size_t size) {
    // Try to allocate from the memory pool first
    if (size <= POOL_BLOCK_SIZE) {
        pthread_mutex_lock(&pool_mutex);
        for (int i = 0; i < POOL_NUM_BLOCKS; i++) {
            if (!memory_pool[i].used) {
                memory_pool[i].used = 1;
                pthread_mutex_unlock(&pool_mutex);
                return (void*)memory_pool[i].data;
            }
        }
        pthread_mutex_unlock(&pool_mutex);
        
        // If pool is full, fall back to system malloc
        LOG_WARN("Memory Pool: Pool exhausted, falling back to heap");
    }
    
    // Use system malloc for larger allocations or when pool is full
    void *ptr = malloc(size);
    if (!ptr) {
        LOG_ERROR("Memory allocation failed for %zu bytes", size);
    }
    return ptr;
}

void* mec_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void *ptr = mec_malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* mec_realloc(void *ptr, size_t size) {
    // Check if the pointer is from our pool
    int from_pool = 0;
    for (int i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (ptr == (void*)memory_pool[i].data) {
            from_pool = 1;
            break;
        }
    }
    
    if (from_pool) {
        // If it's from our pool and new size fits, reuse it
        if (size <= POOL_BLOCK_SIZE) {
            return ptr; // Already allocated from pool
        } else {
            // Size no longer fits in pool, allocate from heap and copy
            void *new_ptr = malloc(size);
            if (new_ptr && ptr) {
                memcpy(new_ptr, ptr, POOL_BLOCK_SIZE); // Copy old data
                // Note: we don't free the pool block here as it would require tracking
            }
            return new_ptr;
        }
    } else {
        // From system malloc, use regular realloc
        return realloc(ptr, size);
    }
}

void mec_free(void *ptr) {
    // Check if the pointer is from our pool
    for (int i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (ptr == (void*)memory_pool[i].data) {
            pthread_mutex_lock(&pool_mutex);
            memory_pool[i].used = 0;  // Mark as free
            pthread_mutex_unlock(&pool_mutex);
            return;  // Return early, don't call free()
        }
    }
    
    // If not from our pool, it must be from malloc, so call free
    if (ptr) {
        free(ptr);
    }
}