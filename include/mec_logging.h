#ifndef MEC_LOGGING_H
#define MEC_LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

// 日志级别枚举
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
} log_level_t;

// 日志目标类型
typedef enum {
    LOG_TARGET_CONSOLE = 0,
    LOG_TARGET_FILE = 1,
    LOG_TARGET_BOTH = 2
} log_target_t;

// 日志管理器结构
typedef struct {
    FILE *log_file;
    log_level_t min_level;
    log_target_t target;
    pthread_mutex_t mutex;
    char filepath[256];
    size_t max_file_size;
    int max_files;
    int current_file_index;
} log_manager_t;

// 日志相关函数声明
int log_init(const char *filename, log_level_t level);
void log_cleanup(void);
void log_set_level(log_level_t level);
void log_set_target(log_target_t target);
void log_message(log_level_t level, const char *format, ...);
void log_message_with_context(const char* file, int line, const char* func, 
                             log_level_t level, const char *format, ...);

// 宏定义方便使用
#define LOG_DEBUG(fmt, ...) log_message_with_context(__FILE__, __LINE__, __func__, LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_message_with_context(__FILE__, __LINE__, __func__, LOG_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_message_with_context(__FILE__, __LINE__, __func__, LOG_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_message_with_context(__FILE__, __LINE__, __func__, LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) log_message_with_context(__FILE__, __LINE__, __func__, LOG_CRITICAL, fmt, ##__VA_ARGS__)

#endif // MEC_LOGGING_H