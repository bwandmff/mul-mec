#ifndef MEC_MONITOR_H
#define MEC_MONITOR_H

#include "mec_common.h"
#include "mec_fusion.h"
#include "mec_thread.h"

/**
 * @brief 实时监控服务模块
 * 
 * 通过 Unix Domain Socket 提供一个轻量级的查询接口，
 * 外部工具连接后可实时获取系统运行状态。
 */

typedef struct {
    char socket_path[128];
    fusion_processor_t *fusion_proc; // 需要监控的算法句柄
} monitor_config_t;

typedef struct {
    monitor_config_t config;
    thread_context_t thread_ctx;
    int server_fd;
} mec_monitor_t;

/**
 * @brief 启动监控服务
 * @param config 配置（包含 Socket 路径）
 * @return 句柄
 */
mec_monitor_t* monitor_start_service(const monitor_config_t *config);

/**
 * @brief 停止监控服务
 */
void monitor_stop_service(mec_monitor_t *mon);

// 内部服务线程
void* monitor_server_thread(void *arg);

#endif // MEC_MONITOR_H
