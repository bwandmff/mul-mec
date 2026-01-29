#ifndef MEC_THREAD_H
#define MEC_THREAD_H

#include <pthread.h>
#include <stdbool.h>

// 线程上下文结构定义
typedef struct {
    pthread_t thread;
    bool running;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} thread_context_t;

// 线程相关函数声明
int thread_create(thread_context_t *ctx, void *(*start_routine)(void*), void *arg);
void thread_destroy(thread_context_t *ctx);
void thread_lock(thread_context_t *ctx);
void thread_unlock(thread_context_t *ctx);
void thread_wait(thread_context_t *ctx);
void thread_signal(thread_context_t *ctx);

#endif // MEC_THREAD_H