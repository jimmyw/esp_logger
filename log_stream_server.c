

#include <ctype.h>
#include <lwip/netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/queue.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "log_capture.h"
#include "log_common.h"
#include "log_stream_common.h"
#include "log_stream_server.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

logstream_server_config_t server_config;

static const char *TAG = "logstream_server";

static void logstream_server_task(void *pvParameters)
{
    log_stream_entry_t log_stream_entry;
    struct sockaddr_in6 dest_addr;

    while (1) {

        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(server_config.port);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 3600;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);

        while (1) {

            int len = recvfrom(sock, &log_stream_entry, sizeof(log_stream_entry), 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                if (errno != 11)
                    ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else if (len > offsetof(log_entry_t, data)) {
                char addr_str[128];
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);

                log_entry_t entry = {};
                if (log_stream_entry.log_stream_version == 1) {
                    entry.core = log_stream_entry.core;
                    entry.level = log_stream_entry.level;
                    strncpy(entry.task, log_stream_entry.task, sizeof(entry.task));
                    strncpy(entry.tag, log_stream_entry.tag, sizeof(entry.tag));
                    entry.timestamp = log_stream_entry.timestamp;
                    entry.data_len = log_stream_entry.data_len;
                    memcpy(entry.data, log_stream_entry.data, log_stream_entry.data_len);
                    log_capture_send_log(&entry);
                } else {
                    ESP_LOGE(TAG, "Wrong version %d", log_stream_entry.log_stream_version);
                }

            }
        }

        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

esp_err_t logstream_server_init(const logstream_server_config_t *config)
{
    server_config = *config;
    xTaskCreate(logstream_server_task, "logstream_server", 4096, (void *)AF_INET, 5, NULL);
    return ESP_OK;
}
