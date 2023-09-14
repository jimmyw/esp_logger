#pragma once

#include "esp_err.h"

struct log_syslog_client_config_s {
    const char *host;
    int port;
};

#define SYSLOG_CLIENT_DEFAULTS { .port = 514 }

typedef struct log_syslog_client_config_s log_syslog_client_config_t;

esp_err_t log_syslog_client_init(const log_syslog_client_config_t * config);
