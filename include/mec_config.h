#ifndef MEC_CONFIG_H
#define MEC_CONFIG_H

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "mec_error.h"

#define MEC_MAX_CONFIGS 200
#define MEC_CONFIG_KEY_LEN 128
#define MEC_CONFIG_VALUE_LEN 512

// 配置类型枚举
typedef enum {
    CFG_TYPE_UNKNOWN = 0,
    CFG_TYPE_INT,
    CFG_TYPE_DOUBLE,
    CFG_TYPE_STRING,
    CFG_TYPE_BOOL
} cfg_type_t;

// 配置项结构
typedef struct {
    char key[MEC_CONFIG_KEY_LEN];
    char value[MEC_CONFIG_VALUE_LEN];
    cfg_type_t type;
    int dirty;  // 是否被修改过
} config_entry_t;

// 配置对象结构
typedef struct {
    config_entry_t entries[MEC_MAX_CONFIGS];
    int count;
    char filename[256];
    pthread_rwlock_t lock;  // 读写锁，提高并发性能
    time_t last_modified;   // 最后修改时间
} config_t;

// 配置相关函数声明
mec_error_code_t config_load(config_t **config, const char *filename);
mec_error_code_t config_save(config_t *config);
void config_free(config_t *config);
mec_error_code_t config_get_string(config_t *config, const char *key, char *out_value, size_t out_size, const char *default_value);
mec_error_code_t config_get_int(config_t *config, const char *key, int *out_value, int default_value);
mec_error_code_t config_get_double(config_t *config, const char *key, double *out_value, double default_value);
mec_error_code_t config_get_bool(config_t *config, const char *key, int *out_value, int default_value);
mec_error_code_t config_set_string(config_t *config, const char *key, const char *value);
mec_error_code_t config_set_int(config_t *config, const char *key, int value);
mec_error_code_t config_set_double(config_t *config, const char *key, double value);
mec_error_code_t config_set_bool(config_t *config, const char *key, int value);
mec_error_code_t config_reload(config_t **config, const char *filename);

#endif // MEC_CONFIG_H