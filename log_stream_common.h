
#pragma once

struct log_stream_entry_s {
    uint8_t log_stream_version;
    uint8_t core;
    uint8_t level;
    uint16_t uptime;
    uint64_t timestamp;
    char task[configMAX_TASK_NAME_LEN];
    char tag[CONFIG_LOGGER_LOG_MAX_TAG_SIZE];
    size_t data_len;
    char data[CONFIG_LOGGER_LOG_MAX_LOG_LINE_SIZE];
}  __attribute__((packed));

typedef struct log_stream_entry_s log_stream_entry_t;
