#include "mec_error.h"
#include "mec_logging.h"
#include <stdarg.h>

const char* mec_error_string(mec_error_code_t code) {
    switch(code) {
        case MEC_OK:
            return "Success";
        case MEC_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case MEC_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case MEC_ERROR_INIT_FAILED:
            return "Initialization failed";
        case MEC_ERROR_START_FAILED:
            return "Start failed";
        case MEC_ERROR_STOP_FAILED:
            return "Stop failed";
        case MEC_ERROR_NOT_READY:
            return "Not ready";
        case MEC_ERROR_TIMEOUT:
            return "Timeout";
        case MEC_ERROR_RESOURCE_BUSY:
            return "Resource busy";
        case MEC_ERROR_IO_ERROR:
            return "IO error";
        case MEC_ERROR_NOT_FOUND:
            return "Not found";
        case MEC_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case MEC_ERROR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

void mec_error_log(mec_error_code_t code, const char* file, int line, 
                   const char* func, const char* format, ...) {
    if (code == MEC_OK) {
        return; // 不记录成功的返回值
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 根据错误严重程度选择日志级别
    if (code == MEC_ERROR_INVALID_PARAM || code == MEC_ERROR_OUT_OF_MEMORY) {
        LOG_ERROR("%s:%d in %s(): %s (Code: %d, %s)", 
                  file, line, func, buffer, code, mec_error_string(code));
    } else if (code == MEC_ERROR_TIMEOUT || code == MEC_ERROR_RESOURCE_BUSY) {
        LOG_WARN("%s:%d in %s(): %s (Code: %d, %s)", 
                 file, line, func, buffer, code, mec_error_string(code));
    } else {
        LOG_ERROR("%s:%d in %s(): %s (Code: %d, %s)", 
                  file, line, func, buffer, code, mec_error_string(code));
    }
}