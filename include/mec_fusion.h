#ifndef MEC_FUSION_H
#define MEC_FUSION_H

#include "mec_common.h"
#include "mec_thread.h"

// Fusion configuration
typedef struct {
    double association_threshold;
    double position_weight;
    double velocity_weight;
    double confidence_threshold;
    int max_track_age;
} fusion_config_t;

// Kalman filter state
typedef struct {
    double state[6];      // [x, y, vx, vy, ax, ay]
    double covariance[36]; // 6x6 covariance matrix
    struct timeval last_update;
    int initialized;
} kalman_state_t;

// Fused track
typedef struct {
    int global_id;
    target_type_t type;
    kalman_state_t filter_state;
    double confidence;
    int age;
    int sensor_mask;  // Bitmask of active sensors
    struct timeval last_update;
} fused_track_t;

// Fusion processor context
typedef struct {
    fusion_config_t config;
    thread_context_t thread_ctx;
    fused_track_t *tracks;
    int track_count;
    int track_capacity;
    int next_global_id;
    track_list_t *output_tracks;
} fusion_processor_t;

// Fusion module functions
fusion_processor_t* fusion_processor_create(const fusion_config_t *config);
void fusion_processor_destroy(fusion_processor_t *processor);
int fusion_processor_start(fusion_processor_t *processor);
void fusion_processor_stop(fusion_processor_t *processor);
int fusion_processor_add_tracks(fusion_processor_t *processor, 
                               const track_list_t *tracks, 
                               int sensor_id);
track_list_t* fusion_processor_get_tracks(fusion_processor_t *processor);

// Internal fusion functions
void* fusion_processing_thread(void *arg);
int associate_tracks(const track_list_t *sensor_tracks, 
                    fused_track_t *fused_tracks, 
                    int fused_count,
                    double threshold);
int update_fused_track(fused_track_t *fused_track, 
                      const target_track_t *sensor_track);
int predict_track_state(fused_track_t *track, double dt);
int initialize_kalman_filter(kalman_state_t *state, const target_track_t *track);
int update_kalman_filter(kalman_state_t *state, const target_track_t *measurement);
double calculate_track_distance(const fused_track_t *track1, const target_track_t *track2);

#endif // MEC_FUSION_H