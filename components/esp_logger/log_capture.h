#pragma once

#include "esp_log.h"
#include "esp_system.h"

#include "freertos/FreeRTOSConfig.h"

struct log_entry_s {
    uint8_t core;
    uint8_t level;
    uint16_t uptime;
    uint64_t timestamp;
    char task[configMAX_TASK_NAME_LEN];
    const char *tag;
    size_t data_len;
    char data[256];
};

extern const char *log_level_names[6];
typedef void log_entry_cb_t(struct log_entry_s *e);
esp_err_t log_capture_init(void);
esp_err_t log_capture_register_handler(log_entry_cb_t cb);

int log_array(esp_log_level_t log_level, const char *tag, const char *prefix, const uint8_t *data, size_t data_size);
int log_string(esp_log_level_t log_level, const char *tag, const char *prefix, const char *data, size_t data_size);
int log_int16_array(esp_log_level_t log_level, const char *tag, const char *prefix, const char *data, size_t data_size);

char *log_printable_char(char c);
char *log_printable_char2(char c);
