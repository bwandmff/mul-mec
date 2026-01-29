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
        ctx->running = false;
        pthread_cond_broadcast(&ctx->cond); // Wake up any waiting threads
        if (pthread_join(ctx->thread, NULL) != 0) {
            // Handle join error if needed
        }
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