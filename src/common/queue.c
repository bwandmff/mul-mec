#include "mec_queue.h"
#include "mec_logging.h"
#include <errno.h>

/**
 * @brief 循环队列的内部实现结构
 */
struct mec_queue_t {
    mec_msg_t *buffer;      // 动态分配的消息缓冲区数组
    int capacity;           // 缓冲区最大容量
    int head;               // 弹出索引（头）
    int tail;               // 插入索引（尾）
    int count;              // 当前有效消息计数
    
    pthread_mutex_t mutex;    // 互斥锁：保护整个结构体的并发访问
    pthread_cond_t not_empty; // 消费者同步信号：队列非空时触发
    pthread_cond_t not_full;  // 生产者同步信号：队列有空间时触发
};

mec_queue_t* mec_queue_create(int capacity) {
    if (capacity <= 0) return NULL;

    mec_queue_t *queue = (mec_queue_t*)mec_malloc(sizeof(mec_queue_t));
    if (!queue) return NULL;

    queue->buffer = (mec_msg_t*)mec_calloc(capacity, sizeof(mec_msg_t));
    if (!queue->buffer) {
        mec_free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    // 初始化同步原语
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    LOG_INFO("MEC Queue: Initialized with capacity %d", capacity);
    return queue;
}

void mec_queue_destroy(mec_queue_t *queue) {
    if (!queue) return;

    pthread_mutex_lock(&queue->mutex);
    // 清理缓冲区中积压的动态内存
    for (int i = 0; i < queue->count; i++) {
        int idx = (queue->head + i) % queue->capacity;
        if (queue->buffer[idx].tracks) {
            track_list_release(queue->buffer[idx].tracks);
        }
    }
    pthread_mutex_unlock(&queue->mutex);

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    mec_free(queue->buffer);
    mec_free(queue);
    
    LOG_INFO("MEC Queue: Destroyed");
}

int mec_queue_push(mec_queue_t *queue, const mec_msg_t *msg) {
    if (!queue || !msg || !msg->tracks) return -1;

    pthread_mutex_lock(&queue->mutex);

    // 检查队列是否已满
    if (queue->count >= queue->capacity) {
        pthread_mutex_unlock(&queue->mutex);
        LOG_WARN("MEC Queue: Push failed - buffer overflow!");
        return -1; 
    }

    // --- 零拷贝改进：增加引用计数而非深拷贝 ---
    track_list_retain(msg->tracks);

    // 存入环形缓冲区
    queue->buffer[queue->tail].sensor_id = msg->sensor_id;
    queue->buffer[queue->tail].timestamp = msg->timestamp;
    queue->buffer[queue->tail].tracks = msg->tracks;

    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    // 通知正在等待的消费者
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int mec_queue_pop(mec_queue_t *queue, mec_msg_t *out_msg, int timeout_ms) {
    if (!queue || !out_msg) return -1;

    pthread_mutex_lock(&queue->mutex);

    // 若队列为空，根据超时配置进行等待
    while (queue->count == 0) {
        if (timeout_ms < 0) {
            // 无限等待
            pthread_cond_wait(&queue->not_empty, &queue->mutex);
        } else if (timeout_ms == 0) {
            // 不等待，直接退出
            pthread_mutex_unlock(&queue->mutex);
            return -1;
        } else {
            // 计时等待
            struct timespec ts;
            struct timeval now;
            gettimeofday(&now, NULL);
            
            long nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
            ts.tv_sec = now.tv_sec + (timeout_ms / 1000) + (nsec / 1000000000);
            ts.tv_nsec = nsec % 1000000000;

            if (pthread_cond_timedwait(&queue->not_empty, &queue->mutex, &ts) != 0) {
                pthread_mutex_unlock(&queue->mutex);
                return -1; // 超时触发
            }
        }
    }

    // 弹出数据并转移所有权（零拷贝转移）
    *out_msg = queue->buffer[queue->head];
    queue->buffer[queue->head].tracks = NULL; // 清除原引用

    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    // 通知生产者（如有由于队列满而阻塞的生产者）
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int mec_queue_size(mec_queue_t *queue) {
    if (!queue) return 0;
    pthread_mutex_lock(&queue->mutex);
    int size = queue->count;
    pthread_mutex_unlock(&queue->mutex);
    return size;
}
