#include "mec_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

static log_manager_t g_log_manager = {
    .log_file = NULL,
    .min_level = LOG_INFO,
    .target = LOG_TARGET_CONSOLE,
    .filepath = "/tmp/mec_system.log",
    .max_file_size = 10 * 1024 * 1024, // 10MB
    .max_files = 5,
    .current_file_index = 0
};

// 初始化互斥锁
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int log_init(const char *filename, log_level_t level) {
    pthread_mutex_lock(&log_mutex);
    
    // 关闭现有日志文件
    if (g_log_manager.log_file && g_log_manager.log_file != stdout && g_log_manager.log_file != stderr) {
        fclose(g_log_manager.log_file);
        g_log_manager.log_file = NULL;
    }
    
    // 设置日志级别
    g_log_manager.min_level = level;
    
    // 设置文件路径
    if (filename) {
        strncpy(g_log_manager.filepath, filename, sizeof(g_log_manager.filepath) - 1);
        g_log_manager.filepath[sizeof(g_log_manager.filepath) - 1] = '\0';
        
        // 尝试打开日志文件
        g_log_manager.log_file = fopen(g_log_manager.filepath, "a");
        if (!g_log_manager.log_file) {
            // 如果打开失败，使用stderr作为备用
            g_log_manager.log_file = stderr;
            g_log_manager.target = LOG_TARGET_CONSOLE;
        } else {
            g_log_manager.target = LOG_TARGET_FILE;
        }
    } else {
        g_log_manager.log_file = stdout;
        g_log_manager.target = LOG_TARGET_CONSOLE;
    }
    
    pthread_mutex_unlock(&log_mutex);
    
    LOG_INFO("Logging system initialized (Level: %d, Target: %d)", level, g_log_manager.target);
    
    return 0;
}

void log_cleanup(void) {
    pthread_mutex_lock(&log_mutex);
    
    if (g_log_manager.log_file && 
        g_log_manager.log_file != stdout && 
        g_log_manager.log_file != stderr) {
        fclose(g_log_manager.log_file);
        g_log_manager.log_file = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void log_set_level(log_level_t level) {
    pthread_mutex_lock(&log_mutex);
    g_log_manager.min_level = level;
    pthread_mutex_unlock(&log_mutex);
}

void log_set_target(log_target_t target) {
    pthread_mutex_lock(&log_mutex);
    g_log_manager.target = target;
    pthread_mutex_unlock(&log_mutex);
}

static void log_write_message(FILE *output, log_level_t level, const char *timestamp, 
                            const char *level_str, const char *module_info, 
                            const char *formatted_msg) {
    fprintf(output, "[%s] [%s] %s%s\n", timestamp, level_str, module_info, formatted_msg);
    fflush(output);
}

void log_message_with_context(const char* file, int line, const char* func, 
                             log_level_t level, const char *format, ...) {
    if (level < g_log_manager.min_level) {
        return;
    }

    pthread_mutex_lock(&log_mutex);
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // 格式化时间戳
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d", 
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    // 获取日志级别字符串
    const char *level_str;
    switch(level) {
        case LOG_DEBUG:   level_str = "DEBUG  "; break;
        case LOG_INFO:    level_str = "INFO   "; break;
        case LOG_WARN:    level_str = "WARN  "; break;
        case LOG_ERROR:   level_str = "ERROR "; break;
        case LOG_CRITICAL: level_str = "CRITICAL"; break;
        default:          level_str = "UNKNOWN"; break;
    }
    
    // 格式化模块信息
    const char* filename = strrchr(file, '/');
    if (filename) {
        filename++; // 跳过'/'
    } else {
        filename = file; // 如果没有路径分隔符，使用整个路径
    }
    
    char module_info[256];
    snprintf(module_info, sizeof(module_info), "%s:%d in %s(): ", filename, line, func);
    
    // 格式化消息
    char formatted_msg[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(formatted_msg, sizeof(formatted_msg), format, args);
    va_end(args);
    
    // 输出到相应的目标
    if (g_log_manager.target == LOG_TARGET_CONSOLE || g_log_manager.target == LOG_TARGET_BOTH) {
        log_write_message(stdout, level, timestamp, level_str, module_info, formatted_msg);
    }
    
    if (g_log_manager.target == LOG_TARGET_FILE || g_log_manager.target == LOG_TARGET_BOTH) {
        if (g_log_manager.log_file) {
            log_write_message(g_log_manager.log_file, level, timestamp, level_str, 
                             module_info, formatted_msg);
        }
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void log_message(log_level_t level, const char *format, ...) {
    if (level < g_log_manager.min_level) {
        return;
    }

    pthread_mutex_lock(&log_mutex);
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // 格式化时间戳
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d", 
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    // 获取日志级别字符串
    const char *level_str;
    switch(level) {
        case LOG_DEBUG:   level_str = "DEBUG  "; break;
        case LOG_INFO:    level_str = "INFO   "; break;
        case LOG_WARN:    level_str = "WARN  "; break;
        case LOG_ERROR:   level_str = "ERROR "; break;
        case LOG_CRITICAL: level_str = "CRITICAL"; break;
        default:          level_str = "UNKNOWN"; break;
    }
    
    // 格式化消息
    char formatted_msg[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(formatted_msg, sizeof(formatted_msg), format, args);
    va_end(args);
    
    // 输出到相应的目标
    if (g_log_manager.target == LOG_TARGET_CONSOLE || g_log_manager.target == LOG_TARGET_BOTH) {
        fprintf(stdout, "[%s] [%s] %s\n", timestamp, level_str, formatted_msg);
        fflush(stdout);
    }
    
    if (g_log_manager.target == LOG_TARGET_FILE || g_log_manager.target == LOG_TARGET_BOTH) {
        if (g_log_manager.log_file) {
            fprintf(g_log_manager.log_file, "[%s] [%s] %s\n", timestamp, level_str, formatted_msg);
            fflush(g_log_manager.log_file);
        }
    }
    
    pthread_mutex_unlock(&log_mutex);
}