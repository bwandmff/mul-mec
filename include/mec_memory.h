#ifndef MEC_MEMORY_H
#define MEC_MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>  // 添加标准整数类型定义
#include <stdbool.h> // 添加布尔类型定义

// 内存池相关定义
#define MEC_MEM_POOL_SMALL_SIZE 256
#define MEC_MEM_POOL_MEDIUM_SIZE 4096
#define MEC_MEM_POOL_SMALL_COUNT 64
#define MEC_MEM_POOL_MEDIUM_COUNT 32
#define MEC_MEM_POOL_LARGE_COUNT 8

// 内存调试选项
#ifdef DEBUG_MEMORY
#define mec_malloc(size) mec_debug_malloc(size, __FILE__, __LINE__)
#define mec_free(ptr) mec_debug_free(ptr, __FILE__, __LINE__)
void* mec_debug_malloc(size_t size, const char* file, int line);
void mec_debug_free(void* ptr, const char* file, int line);
#else
void* mec_malloc(size_t size);
void mec_free(void* ptr);
#endif

void* mec_calloc(size_t nmemb, size_t size);
void* mec_realloc(void *ptr, size_t size);

// 内存池初始化和清理
void mec_memory_init(void);
void mec_memory_cleanup(void);

// 内存使用统计
size_t mec_memory_get_used(void);
size_t mec_memory_get_peak_usage(void);

#endif // MEC_MEMORY_H