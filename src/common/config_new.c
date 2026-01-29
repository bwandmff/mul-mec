#include "mec_config.h"
#include "mec_logging.h"
#include <stdlib.h>
#include <ctype.h>

mec_error_code_t config_load(config_t **config_out, const char *filename) {
    if (!config_out || !filename) {
        LOG_ERROR("Invalid parameters for config_load");
        return MEC_ERROR_INVALID_PARAM;
    }

    config_t *config = malloc(sizeof(config_t));
    if (!config) {
        LOG_ERROR("Failed to allocate memory for config object");
        return MEC_ERROR_OUT_OF_MEMORY;
    }

    // 初始化配置对象
    memset(config, 0, sizeof(config_t));
    strncpy(config->filename, filename, sizeof(config->filename) - 1);
    config->filename[sizeof(config->filename) - 1] = '\0';

    if (pthread_rwlock_init(&config->lock, NULL) != 0) {
        LOG_ERROR("Failed to initialize config lock");
        free(config);
        return MEC_ERROR_INIT_FAILED;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("Failed to open config file: %s", filename);
        pthread_rwlock_destroy(&config->lock);
        free(config);
        return MEC_ERROR_IO_ERROR;
    }

    char line[1024];
    int line_num = 0;

    pthread_rwlock_wrlock(&config->lock);

    while (fgets(line, sizeof(line), file) && config->count < MEC_MAX_CONFIGS) {
        line_num++;
        
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || 
            (line[0] == ' ' && (line[1] == '\n' || line[1] == '\r')) ||
            (line[0] == '\t' && (line[1] == '\n' || line[1] == '\r'))) {
            continue;
        }

        // 查找等号分隔符
        char *delimiter = strchr(line, '=');
        if (!delimiter) {
            LOG_WARN("Invalid config format at line %d: %s", line_num, line);
            continue;
        }

        *delimiter = '\0';
        char *key = line;
        char *value = delimiter + 1;

        // 去除键和值前后的空白字符
        while (*key == ' ' || *key == '\t') key++;
        char *key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t' || 
                                *key_end == '\n' || *key_end == '\r')) {
            *key_end-- = '\0';
        }

        while (*value == ' ' || *value == '\t') value++;
        char *value_end = value + strlen(value) - 1;
        while (value_end > value && (*value_end == ' ' || *value_end == '\t' || 
                                    *value_end == '\n' || *value_end == '\r')) {
            *value_end-- = '\0';
        }

        // 验证键的有效性
        if (strlen(key) >= MEC_CONFIG_KEY_LEN) {
            LOG_WARN("Config key too long at line %d: %s", line_num, key);
            continue;
        }
        
        if (strlen(value) >= MEC_CONFIG_VALUE_LEN) {
            LOG_WARN("Config value too long at line %d: %s", line_num, value);
            continue;
        }

        // 存储配置项
        strncpy(config->entries[config->count].key, key, MEC_CONFIG_KEY_LEN - 1);
        config->entries[config->count].key[MEC_CONFIG_KEY_LEN - 1] = '\0';
        strncpy(config->entries[config->count].value, value, MEC_CONFIG_VALUE_LEN - 1);
        config->entries[config->count].value[MEC_CONFIG_VALUE_LEN - 1] = '\0';
        config->entries[config->count].type = CFG_TYPE_UNKNOWN; // 类型将在获取时确定
        config->entries[config->count].dirty = 0;
        config->count++;
    }

    pthread_rwlock_unlock(&config->lock);
    fclose(file);

    *config_out = config;
    LOG_INFO("Loaded %d configuration entries from %s", config->count, filename);
    
    return MEC_OK;
}

mec_error_code_t config_save(config_t *config) {
    if (!config) {
        LOG_ERROR("Invalid config parameter for save");
        return MEC_ERROR_INVALID_PARAM;
    }

    FILE *file = fopen(config->filename, "w");
    if (!file) {
        LOG_ERROR("Failed to open config file for writing: %s", config->filename);
        return MEC_ERROR_IO_ERROR;
    }

    pthread_rwlock_rdlock(&config->lock);

    for (int i = 0; i < config->count; i++) {
        fprintf(file, "%s=%s\n", config->entries[i].key, config->entries[i].value);
    }

    pthread_rwlock_unlock(&config->lock);
    fclose(file);

    LOG_INFO("Saved %d configuration entries to %s", config->count, config->filename);
    return MEC_OK;
}

void config_free(config_t *config) {
    if (!config) {
        return;
    }

    pthread_rwlock_wrlock(&config->lock);
    pthread_rwlock_unlock(&config->lock);
    pthread_rwlock_destroy(&config->lock);
    free(config);
}

static int config_find_key(config_t *config, const char *key) {
    if (!config || !key) {
        return -1;
    }

    pthread_rwlock_rdlock(&config->lock);
    
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            pthread_rwlock_unlock(&config->lock);
            return i;
        }
    }
    
    pthread_rwlock_unlock(&config->lock);
    return -1;
}

mec_error_code_t config_get_string(config_t *config, const char *key, char *out_value, size_t out_size, const char *default_value) {
    if (!config || !key || !out_value || out_size == 0) {
        LOG_ERROR("Invalid parameters for config_get_string");
        return MEC_ERROR_INVALID_PARAM;
    }

    int idx = config_find_key(config, key);
    if (idx >= 0) {
        pthread_rwlock_rdlock(&config->lock);
        strncpy(out_value, config->entries[idx].value, out_size - 1);
        out_value[out_size - 1] = '\0';
        pthread_rwlock_unlock(&config->lock);
    } else {
        strncpy(out_value, default_value ? default_value : "", out_size - 1);
        out_value[out_size - 1] = '\0';
    }

    return MEC_OK;
}

mec_error_code_t config_get_int(config_t *config, const char *key, int *out_value, int default_value) {
    if (!config || !key || !out_value) {
        LOG_ERROR("Invalid parameters for config_get_int");
        return MEC_ERROR_INVALID_PARAM;
    }

    char str_value[64];
    mec_error_code_t ret = config_get_string(config, key, str_value, sizeof(str_value), NULL);
    if (ret != MEC_OK) {
        *out_value = default_value;
        return ret;
    }

    if (str_value[0] == '\0') {
        *out_value = default_value;
        return MEC_OK;
    }

    char *endptr;
    long val = strtol(str_value, &endptr, 10);
    
    if (*endptr != '\0' && *endptr != '\n' && *endptr != '\r') {
        LOG_WARN("Invalid integer value for key '%s': %s", key, str_value);
        *out_value = default_value;
        return MEC_ERROR_INVALID_PARAM;
    }

    *out_value = (int)val;
    return MEC_OK;
}

mec_error_code_t config_get_double(config_t *config, const char *key, double *out_value, double default_value) {
    if (!config || !key || !out_value) {
        LOG_ERROR("Invalid parameters for config_get_double");
        return MEC_ERROR_INVALID_PARAM;
    }

    char str_value[64];
    mec_error_code_t ret = config_get_string(config, key, str_value, sizeof(str_value), NULL);
    if (ret != MEC_OK) {
        *out_value = default_value;
        return ret;
    }

    if (str_value[0] == '\0') {
        *out_value = default_value;
        return MEC_OK;
    }

    char *endptr;
    double val = strtod(str_value, &endptr);
    
    if (*endptr != '\0' && *endptr != '\n' && *endptr != '\r') {
        LOG_WARN("Invalid double value for key '%s': %s", key, str_value);
        *out_value = default_value;
        return MEC_ERROR_INVALID_PARAM;
    }

    *out_value = val;
    return MEC_OK;
}

mec_error_code_t config_get_bool(config_t *config, const char *key, int *out_value, int default_value) {
    if (!config || !key || !out_value) {
        LOG_ERROR("Invalid parameters for config_get_bool");
        return MEC_ERROR_INVALID_PARAM;
    }

    char str_value[64];
    mec_error_code_t ret = config_get_string(config, key, str_value, sizeof(str_value), NULL);
    if (ret != MEC_OK) {
        *out_value = default_value;
        return ret;
    }

    if (str_value[0] == '\0') {
        *out_value = default_value;
        return MEC_OK;
    }

    // 转换为小写并比较
    for (char *p = str_value; *p; p++) {
        *p = tolower(*p);
    }

    if (strcmp(str_value, "true") == 0 || strcmp(str_value, "1") == 0 || 
        strcmp(str_value, "yes") == 0 || strcmp(str_value, "on") == 0) {
        *out_value = 1;
    } else if (strcmp(str_value, "false") == 0 || strcmp(str_value, "0") == 0 || 
               strcmp(str_value, "no") == 0 || strcmp(str_value, "off") == 0) {
        *out_value = 0;
    } else {
        LOG_WARN("Invalid boolean value for key '%s': %s", key, str_value);
        *out_value = default_value;
    }

    return MEC_OK;
}

mec_error_code_t config_set_string(config_t *config, const char *key, const char *value) {
    if (!config || !key || !value) {
        LOG_ERROR("Invalid parameters for config_set_string");
        return MEC_ERROR_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&config->lock);

    // 查找是否已存在该键
    int idx = -1;
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        // 更新现有键
        strncpy(config->entries[idx].value, value, MEC_CONFIG_VALUE_LEN - 1);
        config->entries[idx].value[MEC_CONFIG_VALUE_LEN - 1] = '\0';
        config->entries[idx].type = CFG_TYPE_STRING;
        config->entries[idx].dirty = 1;
    } else {
        // 添加新键（如果还有空间）
        if (config->count >= MEC_MAX_CONFIGS) {
            pthread_rwlock_unlock(&config->lock);
            LOG_ERROR("Config storage full");
            return MEC_ERROR_OUT_OF_MEMORY;
        }

        strncpy(config->entries[config->count].key, key, MEC_CONFIG_KEY_LEN - 1);
        config->entries[config->count].key[MEC_CONFIG_KEY_LEN - 1] = '\0';
        strncpy(config->entries[config->count].value, value, MEC_CONFIG_VALUE_LEN - 1);
        config->entries[config->count].value[MEC_CONFIG_VALUE_LEN - 1] = '\0';
        config->entries[config->count].type = CFG_TYPE_STRING;
        config->entries[config->count].dirty = 1;
        config->count++;
    }

    pthread_rwlock_unlock(&config->lock);
    return MEC_OK;
}

mec_error_code_t config_set_int(config_t *config, const char *key, int value) {
    char str_value[32];
    snprintf(str_value, sizeof(str_value), "%d", value);
    return config_set_string(config, key, str_value);
}

mec_error_code_t config_set_double(config_t *config, const char *key, double value) {
    char str_value[64];
    snprintf(str_value, sizeof(str_value), "%.6f", value);
    return config_set_string(config, key, str_value);
}

mec_error_code_t config_set_bool(config_t *config, const char *key, int value) {
    return config_set_string(config, key, value ? "true" : "false");
}

mec_error_code_t config_reload(config_t **config, const char *filename) {
    if (!config || !filename) {
        LOG_ERROR("Invalid parameters for config_reload");
        return MEC_ERROR_INVALID_PARAM;
    }

    config_t *old_config = *config;
    config_t *new_config = NULL;
    
    mec_error_code_t ret = config_load(&new_config, filename);
    if (ret != MEC_OK) {
        LOG_ERROR("Failed to reload config from %s", filename);
        return ret;
    }

    // 替换旧配置
    *config = new_config;
    if (old_config) {
        config_free(old_config);
    }

    LOG_INFO("Configuration reloaded successfully from %s", filename);
    return MEC_OK;
}