#include "mec_memory.h"
#include "mec_logging.h"
#include <sys/mman.h>

// 内存池结构定义
typedef struct {
    void* blocks;
    int* free_list;
    int free_count;
    int total_count;
    size_t block_size;
    pthread_mutex_t lock;
} mem_pool_t;

// 内存池实例
static mem_pool_t small_pool = {0};
static mem_pool_t medium_pool = {0};
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

// 统计信息
static size_t current_allocated = 0;
static size_t peak_allocated = 0;
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

// 初始化内存池
static int init_pool(mem_pool_t* pool, int count, size_t block_size) {
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        return -1;
    }
    
    pool->blocks = calloc(count, block_size);
    if (!pool->blocks) {
        pthread_mutex_destroy(&pool->lock);
        return -1;
    }
    
    pool->free_list = malloc(count * sizeof(int));
    if (!pool->free_list) {
        free(pool->blocks);
        pthread_mutex_destroy(&pool->lock);
        return -1;
    }
    
    // 初始化空闲列表
    for (int i = 0; i < count; i++) {
        pool->free_list[i] = i;
    }
    
    pool->free_count = count;
    pool->total_count = count;
    pool->block_size = block_size;
    
    return 0;
}

void mec_memory_init(void) {
    LOG_INFO("Initializing MEC memory management system");
    
    if (init_pool(&small_pool, MEC_MEM_POOL_SMALL_COUNT, MEC_MEM_POOL_SMALL_SIZE) != 0) {
        LOG_ERROR("Failed to initialize small memory pool");
    }
    
    if (init_pool(&medium_pool, MEC_MEM_POOL_MEDIUM_COUNT, MEC_MEM_POOL_MEDIUM_SIZE) != 0) {
        LOG_ERROR("Failed to initialize medium memory pool");
    }
    
    LOG_INFO("MEC memory management initialized successfully");
}

void mec_memory_cleanup(void) {
    LOG_INFO("Cleaning up MEC memory management system");
    
    if (small_pool.blocks) {
        free(small_pool.blocks);
        small_pool.blocks = NULL;
    }
    if (small_pool.free_list) {
        free(small_pool.free_list);
        small_pool.free_list = NULL;
    }
    pthread_mutex_destroy(&small_pool.lock);
    
    if (medium_pool.blocks) {
        free(medium_pool.blocks);
        medium_pool.blocks = NULL;
    }
    if (medium_pool.free_list) {
        free(medium_pool.free_list);
        medium_pool.free_list = NULL;
    }
    pthread_mutex_destroy(&medium_pool.lock);
    
    LOG_INFO("MEC memory management cleaned up successfully");
}

void* mec_malloc(size_t size) {
    void* ptr = NULL;
    
    // 尝试从小内存池分配
    if (size <= MEC_MEM_POOL_SMALL_SIZE) {
        pthread_mutex_lock(&small_pool.lock);
        if (small_pool.free_count > 0) {
            int idx = small_pool.free_list[--small_pool.free_count];
            ptr = (char*)small_pool.blocks + idx * small_pool.block_size;
            // 标记为已分配（存储下一个空闲索引）
            *((int*)ptr) = -1;
        }
        pthread_mutex_unlock(&small_pool.lock);
    }
    // 尝试从中内存池分配
    else if (size <= MEC_MEM_POOL_MEDIUM_SIZE) {
        pthread_mutex_lock(&medium_pool.lock);
        if (medium_pool.free_count > 0) {
            int idx = medium_pool.free_list[--medium_pool.free_count];
            ptr = (char*)medium_pool.blocks + idx * medium_pool.block_size;
            // 标记为已分配
            *((int*)ptr) = -1;
        }
        pthread_mutex_unlock(&medium_pool.lock);
    }
    
    // 如果内存池分配失败，使用系统malloc
    if (!ptr) {
        ptr = malloc(size);
        if (ptr) {
            LOG_WARN("Memory pool exhausted, falling back to system malloc for size %zu", size);
        }
    }
    
    // 更新统计信息
    if (ptr) {
        pthread_mutex_lock(&stats_lock);
        current_allocated += size;
        if (current_allocated > peak_allocated) {
            peak_allocated = current_allocated;
        }
        pthread_mutex_unlock(&stats_lock);
    }
    
    return ptr;
}

void mec_free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    // 尝试归还到内存池
    char* block_start = (char*)ptr;
    
    // 检查是否属于小内存池
    if (block_start >= (char*)small_pool.blocks && 
        block_start < (char*)small_pool.blocks + small_pool.total_count * small_pool.block_size) {
        if (((uintptr_t)(block_start - (char*)small_pool.blocks) % small_pool.block_size) == 0) {
            pthread_mutex_lock(&small_pool.lock);
            if (small_pool.free_count < small_pool.total_count) {
                int idx = (block_start - (char*)small_pool.blocks) / small_pool.block_size;
                small_pool.free_list[small_pool.free_count++] = idx;
                pthread_mutex_unlock(&small_pool.lock);
                
                pthread_mutex_lock(&stats_lock);
                current_allocated -= small_pool.block_size;
                pthread_mutex_unlock(&stats_lock);
                return;
            }
            pthread_mutex_unlock(&small_pool.lock);
        }
    }
    
    // 检查是否属于中内存池
    if (block_start >= (char*)medium_pool.blocks && 
        block_start < (char*)medium_pool.blocks + medium_pool.total_count * medium_pool.block_size) {
        if (((uintptr_t)(block_start - (char*)medium_pool.blocks) % medium_pool.block_size) == 0) {
            pthread_mutex_lock(&medium_pool.lock);
            if (medium_pool.free_count < medium_pool.total_count) {
                int idx = (block_start - (char*)medium_pool.blocks) / medium_pool.block_size;
                medium_pool.free_list[medium_pool.free_count++] = idx;
                pthread_mutex_unlock(&medium_pool.lock);
                
                pthread_mutex_lock(&stats_lock);
                current_allocated -= medium_pool.block_size;
                pthread_mutex_unlock(&stats_lock);
                return;
            }
            pthread_mutex_unlock(&medium_pool.lock);
        }
    }
    
    // 不属于内存池，使用系统free
    free(ptr);
}

void* mec_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = mec_malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* mec_realloc(void* ptr, size_t size) {
    if (!ptr) {
        return mec_malloc(size);
    }
    
    if (size == 0) {
        mec_free(ptr);
        return NULL;
    }
    
    // 对于内存池中的块，我们需要重新分配
    char* block_start = (char*)ptr;
    
    // 检查是否属于内存池
    if ((block_start >= (char*)small_pool.blocks && 
         block_start < (char*)small_pool.blocks + small_pool.total_count * small_pool.block_size &&
         ((uintptr_t)(block_start - (char*)small_pool.blocks) % small_pool.block_size) == 0) ||
        (block_start >= (char*)medium_pool.blocks && 
         block_start < (char*)medium_pool.blocks + medium_pool.total_count * medium_pool.block_size &&
         ((uintptr_t)(block_start - (char*)medium_pool.blocks) % medium_pool.block_size) == 0)) {
        // 如果是内存池中的块，需要重新分配
        void* new_ptr = mec_malloc(size);
        if (new_ptr) {
            // 计算原块大小并复制数据
            size_t copy_size = size;
            if (block_start >= (char*)small_pool.blocks && 
                block_start < (char*)small_pool.blocks + small_pool.total_count * small_pool.block_size) {
                if (copy_size > small_pool.block_size) copy_size = small_pool.block_size;
            } else if (block_start >= (char*)medium_pool.blocks) {
                if (copy_size > medium_pool.block_size) copy_size = medium_pool.block_size;
            }
            memcpy(new_ptr, ptr, copy_size);
            mec_free(ptr);
        }
        return new_ptr;
    }
    
    // 如果不是内存池中的块，使用系统realloc
    return realloc(ptr, size);
}

size_t mec_memory_get_used(void) {
    pthread_mutex_lock(&stats_lock);
    size_t used = current_allocated;
    pthread_mutex_unlock(&stats_lock);
    return used;
}

size_t mec_memory_get_peak_usage(void) {
    pthread_mutex_lock(&stats_lock);
    size_t peak = peak_allocated;
    pthread_mutex_unlock(&stats_lock);
    return peak;
}