#include "mec_monitor.h"
#include "mec_logging.h"
#include "mec_metrics.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * @file monitor.c
 * @brief 极轻量级的 Unix Domain Socket 监控实现
 */

mec_monitor_t* monitor_start_service(const monitor_config_t *config) {
    if (!config) return NULL;
    
    mec_monitor_t *mon = mec_malloc(sizeof(mec_monitor_t));
    if (!mon) return NULL;
    
    // 初始化整个结构体为0，确保所有字段都被初始化
    memset(mon, 0, sizeof(mec_monitor_t));
    mon->config = *config;
    mon->server_fd = -1;  // 初始化为-1，表示无效的文件描述符
    
    // 启动独立的服务线程
    if (thread_create(&mon->thread_ctx, monitor_server_thread, mon) != 0) {
        LOG_ERROR("Monitor: Failed to create server thread");
        // 清理已分配的资源
        mec_free(mon);
        return NULL;
    }
    
    return mon;
}

void monitor_stop_service(mec_monitor_t *mon) {
    if (!mon) return;
    
    // 先停止线程，然后才清理资源
    if (mon->thread_ctx.thread != 0) {
        // 设置运行标志为false，让线程自行退出
        thread_context_t *ctx = &mon->thread_ctx;
        ctx->running = false;
        
        // 发送信号唤醒等待的线程
        pthread_mutex_lock(&ctx->mutex);
        pthread_cond_broadcast(&ctx->cond);
        pthread_mutex_unlock(&ctx->mutex);
        
        // 等待线程结束
        if (pthread_join(ctx->thread, NULL) != 0) {
            LOG_WARN("Monitor: Failed to join monitor thread");
        }
    }
    
    // 清理资源
    if (mon->server_fd >= 0) {
        close(mon->server_fd);
        mon->server_fd = -1;  // 避免重复关闭
    }
    
    unlink(mon->config.socket_path);
    mec_free(mon);
}

void* monitor_server_thread(void *arg) {
    mec_monitor_t *mon = (mec_monitor_t*)arg;
    struct sockaddr_un addr;
    
    // 1. 创建 Socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG_ERROR("Monitor: Socket creation failed");
        return NULL;
    }
    mon->server_fd = server_fd;

    // 2. 绑定路径
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, mon->config.socket_path, sizeof(addr.sun_path)-1);
    addr.sun_path[sizeof(addr.sun_path)-1] = '\0';
    unlink(mon->config.socket_path); // 确保旧路径被清理

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Monitor: Bind failed for path %s", mon->config.socket_path);
        close(server_fd);
        return NULL;
    }

    // 3. 开始监听
    if (listen(server_fd, 5) < 0) {
        LOG_ERROR("Monitor: Listen failed");
        close(server_fd);
        return NULL;
    }

    LOG_INFO("Monitor: Service listening on %s", mon->config.socket_path);

    // 4. 服务循环
    while (mon->thread_ctx.running) {
        fd_set fds;
        struct timeval tv = {1, 0}; // 1秒超时，方便检查线程退出状态
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        
        int ret = select(server_fd + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) continue;

        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        // 获取当前性能指标数据
        // 注意：这里我们直接生成一个简单的报文
        char buffer[512];
        
        // 此处需要获取 metrics。由于 metrics 是静态全局的，直接获取上一次的报文数据。
        // 为了演示，我们生成一个状态 JSON
        int active_tracks = (mon->config.fusion_proc) ? mon->config.fusion_proc->track_count : 0;
        
        int len = snprintf(buffer, sizeof(buffer), 
            "{\n"
            "  \"status\": \"running\",\n"
            "  \"tracks\": %d,\n"
            "  \"uptime_s\": %ld\n"
            "}\n", 
            active_tracks, time(NULL)); // 实际项目中可加入更多 metrics 接口数据

        if (len > 0 && (size_t)len < sizeof(buffer)) {
            send(client_fd, buffer, len, 0);
        }
        close(client_fd);
    }

    return NULL;
}