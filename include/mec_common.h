#ifndef MEC_COMMON_H
#define MEC_COMMON_H

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

// 包含MEC系统自定义头文件
#include "mec_memory.h"
#include "mec_error.h"
#include "mec_config.h"
#include "mec_logging.h"

// Common data structures
typedef struct {
    double latitude;
    double longitude;
    double altitude;
} wgs84_coord_t;

typedef struct {
    int x;
    int y;
} image_coord_t;

typedef enum {
    TARGET_VEHICLE = 0,
    TARGET_NON_VEHICLE = 1,
    TARGET_PEDESTRIAN = 2,
    TARGET_OBSTACLE = 3
} target_type_t;

typedef struct {
    int id;
    target_type_t type;
    wgs84_coord_t position;
    double velocity;
    double heading;
    double confidence;
    struct timeval timestamp;
    int sensor_id;
} target_track_t;

typedef struct {
    target_track_t *tracks;
    int count;
    int capacity;
    int ref_count;            // 引用计数
    pthread_mutex_t ref_lock; // 保護計数のロック
} track_list_t;

// 性能監視統計
typedef struct {
    double fps;               // 現在の処理フレームレート
    double latency_ms;        // 平均処理遅延
    size_t mem_used;          // メモリ使用量
} mec_metrics_t;

// メモリ管理
void* mec_malloc(size_t size);
void* mec_calloc(size_t nmemb, size_t size);
void* mec_realloc(void *ptr, size_t size);
void mec_free(void *ptr);

// ... (config and logging)

// トラックリストユーティリティ
track_list_t* track_list_create(int initial_capacity);
void track_list_retain(track_list_t *list);  // 引用カウンタを増やす
void track_list_release(track_list_t *list); // 引用カウンタを減らす（0になったら解放）
int track_list_add(track_list_t *list, const target_track_t *track);
void track_list_clear(track_list_t *list);

#endif // MEC_COMMON_H