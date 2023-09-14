

#include <ctype.h>
#include <lwip/netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/queue.h>

#include "esp_log.h"

#include "log_common.h"
#include "log_capture.h"
#include "log_syslog_client.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

static struct sockaddr_in dest_addr;

static const char *TAG = "log_syslog_client";

static void send_syslog(log_entry_t *entry) {

    int log_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    char buffer[256];
    int msglen = snprintf(buffer, sizeof(buffer), "<%d>%.*s", entry->level, entry->data_len, entry->data);

    if (log_socket > 0)
        sendto(log_socket, buffer, msglen, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    close(log_socket);
}

esp_err_t log_syslog_client_init(const log_syslog_client_config_t *config)
{

    dest_addr.sin_addr.s_addr = inet_addr(config->host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config->port);

    ESP_LOGD(TAG, "Sending logs to syslog %s:%d", config->host, config->port);

    log_capture_register_handler(&send_syslog);

    return ESP_OK;
}
