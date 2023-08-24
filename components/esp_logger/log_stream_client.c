

#include <ctype.h>
#include <lwip/netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/queue.h>

#include "esp_log.h"

#include "log_capture.h"
#include "log_common.h"
#include "log_stream_client.h"
#include "log_stream_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

static struct sockaddr_in dest_addr;

static const char *TAG = "logstream_client";
#define MAX_PACKET_SIZE 1400 // mtu minus some overhead

static void send_logstream(log_entry_t *entry)
{
    log_stream_entry_t tx_entry;
    tx_entry.log_stream_version = 1;
    tx_entry.core = entry->core;
    tx_entry.level = entry->level;
    strncpy(tx_entry.task, entry->task, sizeof(tx_entry.task));
    strncpy(tx_entry.tag, entry->tag, sizeof(tx_entry.tag));
    tx_entry.timestamp = entry->timestamp;
    tx_entry.data_len = entry->data_len;
    memcpy(tx_entry.data, entry->data, tx_entry.data_len);

    int log_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    size_t packet_size = MIN(offsetof(log_entry_t, data) + tx_entry.data_len, MAX_PACKET_SIZE);

    if (log_socket > 0)
        sendto(log_socket, entry, packet_size, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    close(log_socket);
}

esp_err_t logstream_client_init(const logstream_client_config_t *config)
{

    dest_addr.sin_addr.s_addr = inet_addr(config->host);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config->port);

    ESP_LOGD(TAG, "Sending logs to logstream server %s:%d", config->host, config->port);

    log_capture_register_handler(&send_logstream);

    return ESP_OK;
}
