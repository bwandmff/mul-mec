#include "mec_metrics.h"
#include "mec_logging.h"

static mec_perf_stats_t g_stats;

void metrics_init() {
    pthread_mutex_init(&g_stats.lock, NULL);
    g_stats.frame_count = 0;
    g_stats.total_latency_ms = 0;
    gettimeofday(&g_stats.start_time, NULL);
}

void metrics_record_frame(double latency_ms) {
    pthread_mutex_lock(&g_stats.lock);
    g_stats.frame_count++;
    g_stats.total_latency_ms += latency_ms;
    pthread_mutex_unlock(&g_stats.lock);
}

void metrics_report() {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    pthread_mutex_lock(&g_stats.lock);
    double elapsed = (now.tv_sec - g_stats.start_time.tv_sec) + 
                    (now.tv_usec - g_stats.start_time.tv_usec) / 1000000.0;
    
    if (elapsed > 0) {
        double fps = g_stats.frame_count / elapsed;
        double avg_lat = (g_stats.frame_count > 0) ? (g_stats.total_latency_ms / g_stats.frame_count) : 0;
        
        LOG_INFO("PERF: FPS: %.2f | Avg Latency: %.3f ms | Frames: %ld", 
                 fps, avg_lat, (long)g_stats.frame_count);
    }
    pthread_mutex_unlock(&g_stats.lock);
}