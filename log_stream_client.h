#pragma once

#include "esp_err.h"

struct logstream_client_config_s {
    const char *host;
    int port;
};

#define LOGSTREAM_CLIENT_DEFAULTS { .port = 1514 }

typedef struct logstream_client_config_s logstream_client_config_t;

esp_err_t logstream_client_init(const logstream_client_config_t * config);
