#include "mec_video.h"
#include <time.h>

/**
 * @file video_processor_mock.c
 * @brief 纯 C 语言实现的视频处理 Mock 模块
 * 
 * 用于在没有 OpenCV 环境的情况下进行系统集成测试。
 */

video_processor_t* video_processor_create(const video_config_t *config) {
    if (!config) return NULL;
    
    video_processor_t *processor = (video_processor_t*)mec_malloc(sizeof(video_processor_t));
    if (!processor) return NULL;
    
    processor->config = *config;
    processor->transform.calibrated = 0;
    processor->region_count = 0;
    // 使用零拷贝引用计数模型
    processor->output_tracks = track_list_create(10);
    
    LOG_INFO("MOCK Video: Created (No OpenCV dependency)");
    return processor;
}

void video_processor_destroy(video_processor_t *processor) {
    if (!processor) return;
    video_processor_stop(processor);
    track_list_release(processor->output_tracks);
    mec_free(processor);
}

int video_processor_start(video_processor_t *processor) {
    if (!processor) return -1;
    if (thread_create(&processor->thread_ctx, video_processing_thread, processor) != 0) {
        LOG_ERROR("MOCK Video: Thread start failed");
        return -1;
    }
    return 0;
}

void video_processor_stop(video_processor_t *processor) {
    if (processor) thread_destroy(&processor->thread_ctx);
}

// 获取航迹列表
track_list_t* video_processor_get_tracks(video_processor_t *processor) {
    return (processor) ? processor->output_tracks : NULL;
}

/**
 * @brief Mock 视频处理线程：模拟每秒产生 10 帧检测数据
 */
void* video_processing_thread(void *arg) {
    video_processor_t *proc = (video_processor_t*)arg;
    int target_id_seed = 1000;

    while (proc->thread_ctx.running) {
        thread_lock(&proc->thread_ctx);
        
        track_list_clear(proc->output_tracks);
        
        // 模拟检测到一个匀速移动的目标
        target_track_t t;
        t.id = target_id_seed;
        t.type = TARGET_VEHICLE;
        t.position.latitude = 39.9087;
        t.position.longitude = 116.3975;
        t.velocity = 15.0;
        t.heading = 90.0;
        t.confidence = 0.95;
        gettimeofday(&t.timestamp, NULL);
        
        track_list_add(proc->output_tracks, &t);

        // 如果设置了目标队列，自动推送
        if (proc->config.target_queue) {
            mec_msg_t msg;
            msg.sensor_id = proc->config.camera_id;
            msg.tracks = proc->output_tracks; // 引用计数模型下只需传递指针
            msg.timestamp = t.timestamp;
            mec_queue_push(proc->config.target_queue, &msg);
        }

        thread_unlock(&proc->thread_ctx);
        
        usleep(100000); // 10Hz
    }
    return NULL;
}

// 屏蔽其他 OpenCV 相关的具体实现函数，仅保留符号定义
int transform_image_to_wgs84(const perspective_transform_t *t, const image_coord_t *i, wgs84_coord_t *w) { return 0; }
int detect_targets(const void *f, int w, int h, track_list_t *tr) { return 0; }
int track_targets(track_list_t *p, track_list_t *c) { return 0; }
int video_processor_set_transform(video_processor_t *p, const perspective_transform_t *t) { return 0; }
int video_processor_add_region(video_processor_t *p, const detection_region_t *r) { return 0; }
