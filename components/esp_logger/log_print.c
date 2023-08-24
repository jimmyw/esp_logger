

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/queue.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"

#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "linenoise/linenoise.h"

#include "circ_buf.h"
#include "log_common.h"
#include "log_buffer.h"

static SemaphoreHandle_t xSemaphore = NULL;
static StaticSemaphore_t xSemaphoreBuffer;

#define ANSI_COLOR_E ANSI_COLOR(ANSI_COLOR_RED)
#define ANSI_COLOR_W ANSI_COLOR(ANSI_COLOR_BROWN)
#define ANSI_COLOR_I ANSI_COLOR(ANSI_COLOR_GREEN)
#define ANSI_COLOR_D
#define ANSI_COLOR_V
#define ANSI_FORMAT(letter) ANSI_COLOR_##letter #letter " %u (%-6" PRIu64 ") %15s%24s: "
#define ANSI_FORMAT_END " " ANSI_RESET_COLOR "\n"
void print_log_entry_color(struct log_entry_s *entry, FILE *output)
{
    if (xSemaphoreTakeRecursive(xSemaphore, MS_TO_TICKS(250)) != pdTRUE) {
        return;
    }
    const uint64_t timestamp = entry->timestamp > 100000000 ? entry->timestamp / 1000 : entry->timestamp;

    if (entry->level == ESP_LOG_ERROR) {
        fprintf(output, ANSI_FORMAT(E), entry->core, timestamp, entry->task ? entry->task : "null", entry->tag ? entry->tag : "null");
    } else if (entry->level == ESP_LOG_WARN) {
        fprintf(output, ANSI_FORMAT(W), entry->core, timestamp, entry->task ? entry->task : "null", entry->tag ? entry->tag : "null");
    } else if (entry->level == ESP_LOG_DEBUG) {
        fprintf(output, ANSI_FORMAT(D), entry->core, timestamp, entry->task ? entry->task : "null", entry->tag ? entry->tag : "null");
    } else if (entry->level == ESP_LOG_VERBOSE) {
        fprintf(output, ANSI_FORMAT(V), entry->core, timestamp, entry->task ? entry->task : "null", entry->tag ? entry->tag : "null");
    } else {
        fprintf(output, ANSI_FORMAT(I), entry->core, timestamp, entry->task ? entry->task : "null", entry->tag ? entry->tag : "null");
    }
    if (entry->data_len > 0)
        fwrite(entry->data, entry->data_len, 1, output);
    fwrite(ANSI_FORMAT_END, sizeof(ANSI_FORMAT_END), 1, output);
    fflush(output);
    xSemaphoreGiveRecursive(xSemaphore);
}

SemaphoreHandle_t print_log_get_mutex()
{
    return xSemaphore;
}

void print_log_entry(struct log_entry_s *entry, FILE *output)
{
    if (entry->data_len < 1)
        return;
    if (xSemaphoreTakeRecursive(xSemaphore, MS_TO_TICKS(250)) != pdTRUE) {
        return;
    }
    const uint64_t timestamp = entry->timestamp > 10000000 ? entry->timestamp / 1000 : entry->timestamp;
    fprintf(output, "%c %u (%-6" PRIu64 ") %15s%20s: %.*s\n", entry->level < 6 ? toupper(log_level_names[entry->level][0]) : 'X', entry->core, timestamp,
            entry->task ? entry->task : "null", entry->tag ? entry->tag : "null", entry->data_len, entry->data);
    fflush(output);
    xSemaphoreGiveRecursive(xSemaphore);
}

static void print_log_stdout(struct log_entry_s *entry)
{
    return print_log_entry_color(entry, stdout);
    // return print_log_entry(entry, stdout);
}

esp_err_t log_print_init(void)
{

    xSemaphore = xSemaphoreCreateRecursiveMutexStatic(&xSemaphoreBuffer);

    // Register this as a output in the capture pipe.
    log_capture_register_handler(&print_log_stdout);
    xSemaphoreGiveRecursive(xSemaphore);
    return ESP_OK;
}