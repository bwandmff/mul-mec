#include "mec_config.h"
#include "mec_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

config_t* config_load(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("Failed to open config file: %s", filename);
        return NULL;
    }

    config_t *config = malloc(sizeof(config_t));
    if (!config) {
        fclose(file);
        return NULL;
    }

    config->count = 0;
    char line[512];

    while (fgets(line, sizeof(line), file) && config->count < MAX_CONFIGS) {
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

            strncpy(config->entries[config->count].key, key, sizeof(config->entries[config->count].key) - 1);
            strncpy(config->entries[config->count].value, value, sizeof(config->entries[config->count].value) - 1);
            config->count++;
        }
    }

    fclose(file);
    LOG_INFO("Loaded %d configuration entries from %s", config->count, filename);
    return config;
}

void config_free(config_t *config) {
    if (config) {
        free(config);
    }
}

const char* config_get_string(config_t *config, const char *key, const char *default_value) {
    if (!config) return default_value;
    
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            return config->entries[i].value;
        }
    }
    return default_value;
}

int config_get_int(config_t *config, const char *key, int default_value) {
    if (!config) return default_value;
    
    const char* value = config_get_string(config, key, NULL);
    if (value) {
        return atoi(value);
    }
    return default_value;
}

double config_get_double(config_t *config, const char *key, double default_value) {
    if (!config) return default_value;
    
    const char* value = config_get_string(config, key, NULL);
    if (value) {
        return atof(value);
    }
    return default_value;
}