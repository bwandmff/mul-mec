#ifndef MEC_VIDEO_H
#define MEC_VIDEO_H

#include "mec_common.h"
#include "mec_queue.h"
#include "mec_thread.h"

// Video stream configuration
typedef struct {
    char rtsp_url[256];
    int width;
    int height;
    int fps;
    int camera_id;
    mec_queue_t *target_queue; // 目标消息队列
} video_config_t;

// Perspective transformation parameters
typedef struct {
    double matrix[9];  // 3x3 transformation matrix
    int calibrated;
} perspective_transform_t;

// Detection region
typedef struct {
    int enabled;
    int point_count;
    image_coord_t points[10];  // Max 10 points for polygon
} detection_region_t;

// Video processing context
typedef struct {
    video_config_t config;
    perspective_transform_t transform;
    detection_region_t regions[4];  // Max 4 regions
    int region_count;
    thread_context_t thread_ctx;
    track_list_t *output_tracks;
} video_processor_t;

// Video module functions
video_processor_t* video_processor_create(const video_config_t *config);
void video_processor_destroy(video_processor_t *processor);
int video_processor_start(video_processor_t *processor);
void video_processor_stop(video_processor_t *processor);
int video_processor_set_transform(video_processor_t *processor, const perspective_transform_t *transform);
int video_processor_add_region(video_processor_t *processor, const detection_region_t *region);
track_list_t* video_processor_get_tracks(video_processor_t *processor);

// Coordinate transformation
int transform_image_to_wgs84(const perspective_transform_t *transform, 
                           const image_coord_t *image_coord, 
                           wgs84_coord_t *wgs84_coord);

// Internal processing functions
void* video_processing_thread(void *arg);
int process_video_frame(video_processor_t *processor, const void *frame_data);
int detect_targets(const void *frame_data, int width, int height, track_list_t *tracks);
int track_targets(track_list_t *previous_tracks, track_list_t *current_tracks);

#endif // MEC_VIDEO_H