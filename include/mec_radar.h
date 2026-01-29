#ifndef MEC_RADAR_H
#define MEC_RADAR_H

#include "mec_common.h"
#include "mec_queue.h"
#include "mec_thread.h"

// Radar configuration
typedef struct {
    char device_path[256];
    int baud_rate;
    int radar_id;
    double range_resolution;
    double angle_resolution;
    double max_range;
    mec_queue_t *target_queue; // 目标消息队列
} radar_config_t;

// Raw radar data structure
typedef struct {
    int target_id;
    double range;
    double angle;
    double velocity;
    double rcs;  // Radar Cross Section
    struct timeval timestamp;
} radar_detection_t;

// Radar processing context
typedef struct {
    radar_config_t config;
    thread_context_t thread_ctx;
    track_list_t *output_tracks;
    int fd;  // File descriptor for radar device
} radar_processor_t;

// Radar module functions
radar_processor_t* radar_processor_create(const radar_config_t *config);
void radar_processor_destroy(radar_processor_t *processor);
int radar_processor_start(radar_processor_t *processor);
void radar_processor_stop(radar_processor_t *processor);
track_list_t* radar_processor_get_tracks(radar_processor_t *processor);

// Internal processing functions
void* radar_processing_thread(void *arg);
int radar_read_data(radar_processor_t *processor, radar_detection_t *detection);
int radar_convert_to_track(const radar_detection_t *detection, 
                          const radar_config_t *config, 
                          target_track_t *track);
int radar_polar_to_cartesian(double range, double angle, double *x, double *y);

#endif // MEC_RADAR_H