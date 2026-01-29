#include "mec_config.h"
#include "mec_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

mec_error_code_t config_load(config_t **config, const char *filename) {
    if (!config || !filename) {
        return MEC_ERROR_INVALID_PARAM;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("Failed to open config file: %s", filename);
        return MEC_ERROR_NOT_FOUND;
    }

    *config = malloc(sizeof(config_t));
    if (!(*config)) {
        fclose(file);
        return MEC_ERROR_OUT_OF_MEMORY;
    }

    // Initialize config structure
    memset(*config, 0, sizeof(config_t));
    (*config)->count = 0;
    strcpy((*config)->filename, filename);

    // Initialize read-write lock
    if (pthread_rwlock_init(&(*config)->lock, NULL) != 0) {
        free(*config);
        fclose(file);
        return MEC_ERROR_INIT_FAILED;
    }

    char line[512];

    while (fgets(line, sizeof(line), file) && (*config)->count < MEC_MAX_CONFIGS) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // Parse key=value pairs
        char *delimiter = strchr(line, '=');
        if (delimiter) {
            *delimiter = '\0';
            char *key = line;
            char *value = delimiter + 1;

            // Trim whitespace
            while (*key == ' ' || *key == '\t') key++;
            int len = strlen(value);
            while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t' || 
                   value[len-1] == '\n' || value[len-1] == '\r')) {
                value[--len] = '\0';
            }
            while (*value == ' ' || *value == '\t') value++;

            strncpy((*config)->entries[(*config)->count].key, key, sizeof((*config)->entries[(*config)->count].key) - 1);
            strncpy((*config)->entries[(*config)->count].value, value, sizeof((*config)->entries[(*config)->count].value) - 1);
            (*config)->entries[(*config)->count].key[sizeof((*config)->entries[(*config)->count].key) - 1] = '\0';
            (*config)->entries[(*config)->count].value[sizeof((*config)->entries[(*config)->count].value) - 1] = '\0';
            (*config)->count++;
        }
    }

    fclose(file);
    LOG_INFO("Loaded %d configuration entries from %s", (*config)->count, filename);
    return MEC_OK;
}

void config_free(config_t *config) {
    if (config) {
        pthread_rwlock_destroy(&config->lock);
        free(config);
    }
}

mec_error_code_t config_get_string(config_t *config, const char *key, char *out_value, size_t out_size, const char *default_value) {
    if (!config || !key || !out_value || out_size == 0) {
        return MEC_ERROR_INVALID_PARAM;
    }
    
    // Acquire read lock
    if (pthread_rwlock_rdlock(&config->lock) != 0) {
        return MEC_ERROR_INTERNAL;
    }
    
    mec_error_code_t result = MEC_ERROR_NOT_FOUND;
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            strncpy(out_value, config->entries[i].value, out_size - 1);
            out_value[out_size - 1] = '\0';  // Ensure null termination
            result = MEC_OK;
            break;
        }
    }
    
    if (result != MEC_OK && default_value) {
        strncpy(out_value, default_value, out_size - 1);
        out_value[out_size - 1] = '\0';
        result = MEC_OK;
    }
    
    // Release read lock
    pthread_rwlock_unlock(&config->lock);
    
    return result;
}

mec_error_code_t config_get_int(config_t *config, const char *key, int *out_value, int default_value) {
    if (!config || !key || !out_value) {
        return MEC_ERROR_INVALID_PARAM;
    }
    
    char str_value[MEC_CONFIG_VALUE_LEN];
    mec_error_code_t result = config_get_string(config, key, str_value, sizeof(str_value), NULL);
    
    if (result == MEC_OK) {
        *out_value = atoi(str_value);
    } else {
        *out_value = default_value;
        result = MEC_OK;  // Default value provided
    }
    
    return result;
}

mec_error_code_t config_get_double(config_t *config, const char *key, double *out_value, double default_value) {
    if (!config || !key || !out_value) {
        return MEC_ERROR_INVALID_PARAM;
    }
    
    char str_value[MEC_CONFIG_VALUE_LEN];
    mec_error_code_t result = config_get_string(config, key, str_value, sizeof(str_value), NULL);
    
    if (result == MEC_OK) {
        *out_value = atof(str_value);
    } else {
        *out_value = default_value;
        result = MEC_OK;  // Default value provided
    }
    
    return result;
}

mec_error_code_t config_reload(config_t **config, const char *filename) {
    if (!config || !filename) {
        return MEC_ERROR_INVALID_PARAM;
    }
    
    config_free(*config);
    return config_load(config, filename);
}