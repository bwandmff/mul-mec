#ifndef MEC_ERROR_H
#define MEC_ERROR_H

#include <stdio.h>

/**
 * @brief MEC系统错误代码定义
 */
typedef enum {
    MEC_OK = 0,                    /**< 成功 */
    MEC_ERROR_INVALID_PARAM = -1,   /**< 无效参数 */
    MEC_ERROR_OUT_OF_MEMORY = -2,   /**< 内存不足 */
    MEC_ERROR_INIT_FAILED = -3,     /**< 初始化失败 */
    MEC_ERROR_START_FAILED = -4,    /**< 启动失败 */
    MEC_ERROR_STOP_FAILED = -5,     /**< 停止失败 */
    MEC_ERROR_NOT_READY = -6,       /**< 未就绪 */
    MEC_ERROR_TIMEOUT = -7,         /**< 超时 */
    MEC_ERROR_RESOURCE_BUSY = -8,   /**< 资源忙 */
    MEC_ERROR_IO_ERROR = -9,        /**< IO错误 */
    MEC_ERROR_NOT_FOUND = -10,      /**< 未找到 */
    MEC_ERROR_PERMISSION_DENIED = -11, /**< 权限拒绝 */
    MEC_ERROR_INTERNAL = -99        /**< 内部错误 */
} mec_error_code_t;

/**
 * @brief 获取错误代码对应的描述字符串
 * @param code 错误代码
 * @return 错误描述字符串
 */
const char* mec_error_string(mec_error_code_t code);

/**
 * @brief 打印错误信息到日志
 * @param code 错误代码
 * @param file 文件名
 * @param line 行号
 * @param func 函数名
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void mec_error_log(mec_error_code_t code, const char* file, int line, 
                   const char* func, const char* format, ...);

/* 宏定义便于使用 */
#define MEC_CHECK(call) \
    do { \
        mec_error_code_t ret = call; \
        if (ret != MEC_OK) { \
            mec_error_log(ret, __FILE__, __LINE__, __func__, \
                         "Function failed: %s", #call); \
            return ret; \
        } \
    } while(0)

#define MEC_RETURN_IF_ERROR(expr) \
    do { \
        mec_error_code_t ret = expr; \
        if (ret != MEC_OK) { \
            return ret; \
        } \
    } while(0)

#define MEC_LOG_ERROR_IF_ERROR(expr) \
    do { \
        mec_error_code_t ret = expr; \
        if (ret != MEC_OK) { \
            mec_error_log(ret, __FILE__, __LINE__, __func__, \
                         "Error occurred: %s", mec_error_string(ret)); \
        } \
    } while(0)

#endif // MEC_ERROR_H