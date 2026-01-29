#include "mec_simulator.h"
#include "mec_logging.h"
#include <time.h>

mec_simulator_t* simulator_create(const simulator_config_t *config) {
    if (!config) return NULL;
    
    mec_simulator_t *sim = mec_malloc(sizeof(mec_simulator_t));
    if (!sim) return NULL;
    
    sim->config = *config;
    sim->video_tracks = track_list_create(100);
    sim->radar_tracks = track_list_create(100);
    
    if (!sim->video_tracks || !sim->radar_tracks) {
        simulator_destroy(sim);
        return NULL;
    }
    
    LOG_INFO("Created simulator with data: %s", config->data_path);
    return sim;
}

void simulator_destroy(mec_simulator_t *sim) {
    if (!sim) return;
    simulator_stop(sim);
    track_list_release(sim->video_tracks);
    track_list_release(sim->radar_tracks);
    mec_free(sim);
}

int simulator_start(mec_simulator_t *sim) {
    if (!sim) return -1;
    if (thread_create(&sim->thread_ctx, simulator_thread, sim) != 0) {
        LOG_ERROR("Failed to start simulator thread");
        return -1;
    }
    return 0;
}

void simulator_stop(mec_simulator_t *sim) {
    if (!sim) return;
    thread_destroy(&sim->thread_ctx);
}

track_list_t* simulator_get_video_tracks(mec_simulator_t *sim) {
    return sim->video_tracks;
}

track_list_t* simulator_get_radar_tracks(mec_simulator_t *sim) {
    return sim->radar_tracks;
}

void* simulator_thread(void *arg) {
    mec_simulator_t *sim = (mec_simulator_t*)arg;
    FILE *fp = NULL;
    char line[512];
    
    while (sim->thread_ctx.running) {
        fp = fopen(sim->config.data_path, "r");
        if (!fp) {
            LOG_ERROR("Failed to open simulation data: %s", sim->config.data_path);
            break;
        }

        struct timeval start_time;
        gettimeofday(&start_time, NULL);
        long start_ms = start_time.tv_sec * 1000 + start_time.tv_usec / 1000;

        while (sim->thread_ctx.running && fgets(line, sizeof(line), fp)) {
            if (line[0] == '#' || line[0] == '\n') continue;

            long rel_time_ms;
            int sensor_id, id, type;
            double lat, lon, vel, heading, conf;

            if (sscanf(line, "%ld %d %d %d %lf %lf %lf %lf %lf", 
                       &rel_time_ms, &sensor_id, &id, &type, &lat, &lon, &vel, &heading, &conf) != 9) {
                continue;
            }

            // Wait until it's time to inject this data
            struct timeval now;
            while (sim->thread_ctx.running) {
                gettimeofday(&now, NULL);
                long current_ms = now.tv_sec * 1000 + now.tv_usec / 1000;
                if ((current_ms - start_ms) >= (rel_time_ms / sim->config.playback_speed)) {
                    break;
                }
                usleep(5000); 
            }

            // Inject into appropriate list
            thread_lock(&sim->thread_ctx);
            target_track_t track;
            track.id = id;
            track.type = (target_type_t)type;
            track.position.latitude = lat;
            track.position.longitude = lon;
            track.position.altitude = 0;
            track.velocity = vel;
            track.heading = heading;
            track.confidence = conf;
            track.sensor_id = sensor_id;
            track.timestamp = now;

            if (sensor_id == 1) { // Video
                // In a real simulator, we'd clear per frame, but here we just append or manage
                // Simplified: clear if this is a new "burst" of same-timestamp data
                track_list_add(sim->video_tracks, &track);
            } else if (sensor_id == 2) { // Radar
                track_list_add(sim->radar_tracks, &track);
            }
            thread_unlock(&sim->thread_ctx);
        }

        fclose(fp);
        if (!sim->config.loop) break;
        LOG_INFO("Simulation loop restart");
        track_list_clear(sim->video_tracks);
        track_list_clear(sim->radar_tracks);
    }
    
    return NULL;
}
