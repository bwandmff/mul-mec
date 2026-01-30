#include "mec_thread.h"
#include <stdio.h>
#include <stdlib.h>

int thread_create(thread_context_t *ctx, void *(*start_routine)(void*), void *arg) {
    ctx->running = true;
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&ctx->cond, NULL) != 0) {
        pthread_mutex_destroy(&ctx->mutex);
        return -1;
    }
    
    return pthread_create(&ctx->thread, NULL, start_routine, arg);
}

void thread_destroy(thread_context_t *ctx) {
    if (ctx) {
        // 确保线程处于非运行状态
        ctx->running = false;
        
        // 唤醒任何等待的线程
        pthread_mutex_lock(&ctx->mutex);
        pthread_cond_broadcast(&ctx->cond);
        pthread_mutex_unlock(&ctx->mutex);
        
        // 等待线程结束
        if (pthread_join(ctx->thread, NULL) != 0) {
            // LOG_WARN("Thread: Failed to join thread");
        }
        
        // 销毁同步原语
        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->cond);
    }
}

void thread_lock(thread_context_t *ctx) {
    pthread_mutex_lock(&ctx->mutex);
}

void thread_unlock(thread_context_t *ctx) {
    pthread_mutex_unlock(&ctx->mutex);
}

void thread_wait(thread_context_t *ctx) {
    pthread_cond_wait(&ctx->cond, &ctx->mutex);
}

void thread_signal(thread_context_t *ctx) {
    pthread_cond_signal(&ctx->cond);
}