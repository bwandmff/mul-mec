#include "mec_common.h"
#include "mec_video.h"
#include "mec_radar.h"
#include "mec_fusion.h"
#include "mec_simulator.h"
#include "mec_queue.h"
#include "mec_v2x.h"
#include "mec_metrics.h"
#include "mec_monitor.h"
#include <signal.h>
#include <stdint.h>

static int running = 1;
static int reload_config = 0;

// 信号处理函数：确保系统能够安全退出
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        LOG_INFO("Received shutdown signal, exiting...");
    } else if (sig == SIGHUP) {
        reload_config = 1;
        LOG_INFO("Received SIGHUP, reloading configuration...");
    }
}

int main(int argc, char *argv[]) {
    int sim_mode = 0;
    char *config_path = "/etc/mec/mec.conf";

    // 1. 命令行参数解析
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sim") == 0 || strcmp(argv[i], "-s") == 0) {
            sim_mode = 1;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    // 2. 初始化内存管理系统
    mec_memory_init();
    
    // 3. 初始化日志系统与性能监控
    log_init("/tmp/mec_system.log", LOG_INFO);
    metrics_init();
    LOG_INFO("MEC System starting... (Mode: %s)", sim_mode ? "Simulation" : "Real Sensors");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 4. 加载配置文件
    config_t *config = NULL;
    mec_error_code_t ret = config_load(&config, config_path);
    if (ret != MEC_OK) {
        LOG_WARN("Failed to load configuration from %s: %s", config_path, mec_error_string(ret));
        if (!sim_mode) {
            LOG_ERROR("Cannot run without valid configuration in non-sim mode");
            ret = MEC_ERROR_INIT_FAILED;
            goto cleanup;
        }
    }
    
    // 5. 创建全局异步消息队列 (容量设为 50)
    mec_queue_t *msg_queue = mec_queue_create(50);
    if (!msg_queue) {
        LOG_ERROR("Failed to create message queue");
        ret = MEC_ERROR_INIT_FAILED;
        goto cleanup;
    }

    // 6. 初始化融合引擎配置
    fusion_config_t fusion_cfg = {0};
    int temp_int;
    double temp_double;
    
    if (config) {
        MEC_LOG_ERROR_IF_ERROR(config_get_double(config, "fusion.association_threshold", &temp_double, 5.0));
        fusion_cfg.association_threshold = temp_double;
        
        MEC_LOG_ERROR_IF_ERROR(config_get_double(config, "fusion.position_weight", &temp_double, 1.0));
        fusion_cfg.position_weight = temp_double;
        
        MEC_LOG_ERROR_IF_ERROR(config_get_double(config, "fusion.velocity_weight", &temp_double, 0.1));
        fusion_cfg.velocity_weight = temp_double;
        
        MEC_LOG_ERROR_IF_ERROR(config_get_double(config, "fusion.confidence_threshold", &temp_double, 0.3));
        fusion_cfg.confidence_threshold = temp_double;
        
        MEC_LOG_ERROR_IF_ERROR(config_get_int(config, "fusion.max_track_age", &temp_int, 50));
        fusion_cfg.max_track_age = temp_int;
    } else {
        fusion_cfg.association_threshold = 5.0;
        fusion_cfg.confidence_threshold = 0.3;
        fusion_cfg.max_track_age = 50;
    }

    fusion_processor_t *fusion_proc = fusion_processor_create(&fusion_cfg);
    if (!fusion_proc) {
        LOG_ERROR("Failed to create fusion processor");
        ret = MEC_ERROR_INIT_FAILED;
        goto cleanup;
    }
    
    video_processor_t *video_proc = NULL;
    radar_processor_t *radar_proc = NULL;
    mec_simulator_t *simulator = NULL;
    mec_monitor_t *monitor_service = NULL;  // 确保初始化为NULL

    // 7. 启动数据源（模拟器或真实传感器）
    if (sim_mode) {
        simulator_config_t sim_cfg = {
            .playback_speed = 1.0,
            .loop = 1
        };
        
        char sim_data[256];
        MEC_LOG_ERROR_IF_ERROR(config_get_string(config, "sim.data_path", sim_data, sizeof(sim_data), "config/scenario_test.txt"));
        strncpy(sim_cfg.data_path, sim_data, sizeof(sim_cfg.data_path) - 1);
        
        simulator = simulator_create(&sim_cfg);
        if (!simulator) {
            LOG_ERROR("Failed to create simulator");
            ret = MEC_ERROR_INIT_FAILED;
            goto cleanup;
        }
        
        if (simulator_start(simulator) != 0) {
            LOG_ERROR("Failed to start simulator");
            ret = MEC_ERROR_START_FAILED;
            goto cleanup;
        }
    } else {
        // 真实传感器模式：将队列句柄传入配置，实现生产者模式
        video_config_t video_cfg = {0};
        
        char rtsp_url[256];
        MEC_LOG_ERROR_IF_ERROR(config_get_string(config, "video.rtsp_url", rtsp_url, sizeof(rtsp_url), "rtsp://192.168.1.100:554/stream"));
        strncpy(video_cfg.rtsp_url, rtsp_url, sizeof(video_cfg.rtsp_url) - 1);
        video_cfg.camera_id = 1;
        video_cfg.target_queue = msg_queue; // 绑定异步队列
        
        radar_config_t radar_cfg = {0};
        
        char device_path[256];
        MEC_LOG_ERROR_IF_ERROR(config_get_string(config, "radar.device_path", device_path, sizeof(device_path), "/dev/ttyUSB0"));
        strncpy(radar_cfg.device_path, device_path, sizeof(radar_cfg.device_path) - 1);
        radar_cfg.radar_id = 2;
        radar_cfg.target_queue = msg_queue; // 绑定异步队列
        
        video_proc = video_processor_create(&video_cfg);
        if (!video_proc) {
            LOG_ERROR("Failed to create video processor");
            ret = MEC_ERROR_INIT_FAILED;
            goto cleanup;
        }
        
        radar_proc = radar_processor_create(&radar_cfg);
        if (!radar_proc) {
            LOG_ERROR("Failed to create radar processor");
            ret = MEC_ERROR_INIT_FAILED;
            goto cleanup;
        }
        
        if (video_processor_start(video_proc) != 0) {
            LOG_ERROR("Failed to start video processor");
            ret = MEC_ERROR_START_FAILED;
            goto cleanup;
        }
        
        if (radar_processor_start(radar_proc) != 0) {
            LOG_ERROR("Failed to start radar processor");
            ret = MEC_ERROR_START_FAILED;
            goto cleanup;
        }
    }

    // 8. 启动融合处理器线程
    if (fusion_processor_start(fusion_proc) != 0) {
        LOG_ERROR("Failed to start fusion processor");
        ret = MEC_ERROR_START_FAILED;
        goto cleanup;
    }

    // --- 新增：启动监控服务 ---
    monitor_config_t mon_cfg = {0};
    strncpy(mon_cfg.socket_path, "/tmp/mec_system.sock", sizeof(mon_cfg.socket_path)-1);
    mon_cfg.fusion_proc = fusion_proc;
    monitor_service = monitor_start_service(&mon_cfg);
    if (!monitor_service) {
        LOG_WARN("Failed to start monitor service, continuing without monitoring");
    }
    
    LOG_INFO("MEC System Running in Asynchronous Mode (Queue: %d msgs limit)", 50);
    
    // 9. 核心消息循环 (消费者模式)
    while (running) {
        // --- 检查并处理配置重载 ---
        if (reload_config) {
            config_t *new_cfg = NULL;
            if (config_reload(&new_cfg, config_path) == MEC_OK) {
                // 更新融合参数（示例）
                MEC_LOG_ERROR_IF_ERROR(config_get_double(new_cfg, "fusion.association_threshold", &temp_double, 5.0));
                fusion_cfg.association_threshold = temp_double;
                
                MEC_LOG_ERROR_IF_ERROR(config_get_double(new_cfg, "fusion.confidence_threshold", &temp_double, 0.3));
                fusion_cfg.confidence_threshold = temp_double;
                
                LOG_INFO("Configuration reloaded (New Association Threshold: %.2f)", fusion_cfg.association_threshold);
                
                // 释放旧配置，使用新配置
                if (config) {
                    config_free(config);
                }
                config = new_cfg;
            } else {
                LOG_ERROR("Failed to reload configuration");
            }
            reload_config = 0;
        }

        mec_msg_t incoming_msg;
        
        // 从队列中弹出数据，设置 500ms 超时，避免死等
        if (mec_queue_pop(msg_queue, &incoming_msg, 500) == 0) {
            struct timeval t1, t2;
            gettimeofday(&t1, NULL);

            // 拿到数据，立刻投喂给融合引擎
            MEC_LOG_ERROR_IF_ERROR(fusion_processor_add_tracks(fusion_proc, incoming_msg.tracks, incoming_msg.sensor_id));
            
            // 重要：队列 pop 出来的 tracks 所有权转移给了主循环，处理完需释放引用
            track_list_release(incoming_msg.tracks);
            
            gettimeofday(&t2, NULL);
            double lat = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec) / 1000.0;
            metrics_record_frame(lat);

            // 实时输出结果
            track_list_t *fused = fusion_processor_get_tracks(fusion_proc);
            if (fused && fused->count > 0) {
                printf("\r[LIVE] Fused Targets: %d | Last Source: %d   ", fused->count, incoming_msg.sensor_id);
                fflush(stdout);

                // --- 新增：V2X 标准消息编码 ---
                uint8_t v2x_buffer[2048];
                int v2x_len = sizeof(v2x_buffer);
                if (v2x_encode_rsm(fused, 0xABCD, v2x_buffer, &v2x_len) == 0) {
                    LOG_DEBUG("V2X: Encoded RSM packet (%d bytes) ready for broadcast", v2x_len);
                }
            }
        } else {
            // 队列空，打印心跳状态
            static time_t last_hb = 0;
            time_t now = time(NULL);
            if (now - last_hb >= 5) {
                LOG_INFO("System Heartbeat: [Queue Size: %d] [Active Tracks: %d]", 
                         mec_queue_size(msg_queue), fusion_proc->track_count);
                metrics_report();
                last_hb = now;
            }

            if (sim_mode) {
                // 模拟器特殊处理：主动拉取
                track_list_t *vt = simulator_get_video_tracks(simulator);
                if (vt && vt->count > 0) {
                    MEC_LOG_ERROR_IF_ERROR(fusion_processor_add_tracks(fusion_proc, vt, 1));
                    track_list_clear(vt);
                }
            }
        }
    }
    
    ret = MEC_OK; // 正常退出
    
cleanup:
    LOG_INFO("MEC System shutting down...");
    if (monitor_service) {
        monitor_stop_service(monitor_service);
        monitor_service = NULL;
    }
    if (simulator) {
        simulator_stop(simulator);
        simulator_destroy(simulator);
    }
    if (video_proc) { 
        video_processor_stop(video_proc); 
        video_processor_destroy(video_proc); 
    }
    if (radar_proc) { 
        radar_processor_stop(radar_proc); 
        radar_processor_destroy(radar_proc); 
    }
    if (fusion_proc) { 
        fusion_processor_stop(fusion_proc); 
        fusion_processor_destroy(fusion_proc); 
    }
    if (msg_queue) mec_queue_destroy(msg_queue);
    if (config) config_free(config);
    log_cleanup();
    mec_memory_cleanup();
    
    LOG_INFO("MEC System shutdown complete. Peak memory usage: %zu bytes", mec_memory_get_peak_usage());
    
    return (ret == MEC_OK) ? 0 : 1;
}
