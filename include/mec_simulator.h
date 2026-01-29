#ifndef MEC_SIMULATOR_H
#define MEC_SIMULATOR_H

#include "mec_common.h"
#include "mec_thread.h"

typedef struct {
    char data_path[256];
    double playback_speed;
    int loop;
} simulator_config_t;

typedef struct {
    simulator_config_t config;
    thread_context_t thread_ctx;
    track_list_t *video_tracks;
    track_list_t *radar_tracks;
} mec_simulator_t;

mec_simulator_t* simulator_create(const simulator_config_t *config);
void simulator_destroy(mec_simulator_t *sim);
int simulator_start(mec_simulator_t *sim);
void simulator_stop(mec_simulator_t *sim);

// Get the current simulated tracks
track_list_t* simulator_get_video_tracks(mec_simulator_t *sim);
track_list_t* simulator_get_radar_tracks(mec_simulator_t *sim);

// Internal thread function
void* simulator_thread(void *arg);

#endif // MEC_SIMULATOR_H
