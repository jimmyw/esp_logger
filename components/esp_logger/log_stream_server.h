#pragma once

#include "esp_err.h"

struct logstream_server_config_s {
    int port;
};

#define LOGSTREAM_SERVER_DEFAULTS { .port = 1514 }

typedef struct logstream_server_config_s logstream_server_config_t;

esp_err_t logstream_server_init(const logstream_server_config_t * config);
