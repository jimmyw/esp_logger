

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
#include "log_print.h"

static EXT_RAM_BSS_ATTR char log_data[CONFIG_LOGGER_LOG_BUFFER_SIZE];
static circ_buf_t log_buf;
static uint32_t last_index = 0;
static struct {
    uint32_t offset;
    uint32_t entry;
} peek_cache = {};

static SemaphoreHandle_t xSemaphore = NULL;
static StaticSemaphore_t xSemaphoreBuffer;

struct log_header_s {
    uint32_t index;
    uint8_t core;
    uint8_t level;
    uint16_t data_len;
    uint64_t timestamp;
    char task[configMAX_TASK_NAME_LEN];
    char tag[CONFIG_LOGGER_LOG_MAX_TAG_SIZE];
} __attribute__((packed));

static void purge_entry()
{
    struct log_header_s header = {};
    if (!circ_peek(&log_buf, (char *)&header, sizeof(struct log_header_s)))
        return;
    circ_pull_ptr_pulled(&log_buf, sizeof(struct log_header_s) + header.data_len);
    memset(&peek_cache, 0, sizeof(peek_cache));
}

static void log_buffer_push_entry(struct log_entry_s *e)
{
    struct log_header_s header = {
        .index = last_index++,
        .core = e->core,
        .level = e->level,
        .timestamp = e->timestamp,
        .data_len = e->data_len,
    };
    strncpy(header.task, e->task, sizeof(header.task));
    strncpy(header.tag, e->tag, sizeof(header.tag));
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) != pdTRUE) {
        return;
    }

    // Free space in log header
    while (circ_get_free_bytes(&log_buf) < (sizeof(struct log_header_s) + e->data_len)) {
        purge_entry();
    }

    if (circ_push(&log_buf, (char *)&header, sizeof(struct log_header_s)) != sizeof(struct log_header_s))
        abort();
    if (circ_push(&log_buf, (char *)e->data, e->data_len) != e->data_len)
        abort();
    xSemaphoreGive(xSemaphore);
}

bool log_pull_entry(struct log_entry_s *entry)
{

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    struct log_header_s header = {};
    // Peek entry, without pulling it
    if (!circ_pull(&log_buf, (char *)&header, sizeof(header))) {
        xSemaphoreGive(xSemaphore);
        return false;
    }

    if (header.data_len == 0)
        abort();

    entry->core = header.core;
    entry->level = header.level;
    strncpy(entry->task, header.task, sizeof(entry->task));
    strncpy(entry->tag, header.tag, sizeof(entry->tag));
    entry->timestamp = header.timestamp;
    entry->data_len = header.data_len;

    if (header.data_len > sizeof(entry->data))
        abort();

    if (circ_pull(&log_buf, (char *)entry->data, header.data_len) != header.data_len)
        abort();
    memset(&peek_cache, 0, sizeof(peek_cache));
    xSemaphoreGive(xSemaphore);
    return true;
}

bool log_peek_entry(struct log_entry_s *entry, uint32_t *index)
{
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) != pdTRUE)
        return false;

    size_t offset = 0;

    if (peek_cache.offset > 0 && peek_cache.entry > 0 && peek_cache.entry == *index) {
        offset = peek_cache.offset;
    }

    // Lets search of an entry, that is greater than timestamp.
    while (1) {
        struct log_header_s header = {};
        // Peek entry, without pulling it
        if (circ_peek_offset(&log_buf, (char *)&header, sizeof(header), offset) != sizeof(header)) {
            xSemaphoreGive(xSemaphore);
            return false;
        }
        // Store offset in a local cache to speed up peeking.
        peek_cache.entry = header.index;
        peek_cache.offset = offset;

        offset += sizeof(header);
        if (header.index > *index) {
            *index = header.index;
            entry->core = header.core;
            entry->level = header.level;
            strcpy(entry->task, header.task);
            strcpy(entry->tag, header.tag);
            entry->timestamp = header.timestamp;
            entry->data_len = header.data_len;

            if (header.data_len < 1)
                abort();
            if (header.data_len > sizeof(entry->data))
                abort();

            if (circ_peek_offset(&log_buf, (char *)entry->data, header.data_len, offset) != header.data_len)
                abort();
            break;
        }
        offset += header.data_len;
    };

    xSemaphoreGive(xSemaphore);
    return true;
}

struct log_buffer_stat {
    size_t buffer_max_size_bytes;
    size_t buffer_size_bytes;
    size_t buffer_size_entries;
};

void log_buffer_stats(struct log_buffer_stat *stat)
{
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) != pdTRUE)
        return;
    memset(stat, 0, sizeof(struct log_buffer_stat));
    stat->buffer_max_size_bytes = circ_total_size(&log_buf);

    // Lets search of an entry, that is greater than timestamp.
    while (1) {
        struct log_header_s header = {};
        // Peek entry, without pulling it
        if (circ_peek_offset(&log_buf, (char *)&header, sizeof(header), stat->buffer_size_bytes) != sizeof(header)) {
            xSemaphoreGive(xSemaphore);
            return;
        }
        stat->buffer_size_bytes += sizeof(header) + header.data_len;
        stat->buffer_size_entries++;
    };

    xSemaphoreGive(xSemaphore);
}

static struct {
    struct arg_lit *clear;
    struct arg_lit *color;
    struct arg_lit *purge;
    struct arg_lit *stats;
    struct arg_end *end;
} dmesg_args;

static int cmd_dmesg(int argc, char **argv)
{

    int nerrors = arg_parse(argc, argv, (void **)&dmesg_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, dmesg_args.end, argv[0]);
        return 1;
    }
    struct log_entry_s entry = {};

    bool clear = dmesg_args.clear->count > 0;
    bool color = dmesg_args.color->count > 0;

    if (dmesg_args.purge->count > 0) {
        while (log_pull_entry(&entry)) {
        }
        return 0;
    }

    if (dmesg_args.stats->count > 0) {
        struct log_buffer_stat stat;
        log_buffer_stats(&stat);
        printf("Log buffer max size: %d bytes.\n", stat.buffer_max_size_bytes);
        printf("Log buffer current size: %d bytes.\n", stat.buffer_size_bytes);
        printf("Log buffer current size: %d entries.\n", stat.buffer_size_entries);
        return 0;
    }

    if (clear) {
        while (log_pull_entry(&entry)) {
            if (color)
                print_log_entry_color(&entry, stdout);
            else
                print_log_entry(&entry, stdout);
        }
    } else {
        uint32_t index = 0;
        while (log_peek_entry(&entry, &index)) {
            if (color)
                print_log_entry_color(&entry, stdout);
            else
                print_log_entry(&entry, stdout);
        }
    }
    return 0;
}

esp_err_t log_buffer_early_init()
{
    xSemaphore = xSemaphoreCreateBinaryStatic(&xSemaphoreBuffer);
    circ_init(&log_buf, log_data, sizeof(log_data));
    log_capture_register_handler(&log_buffer_push_entry);
    xSemaphoreGive(xSemaphore);

    return ESP_OK;
}

esp_err_t log_buffer_init(void)
{
    dmesg_args.clear = arg_lit0("c", "clear", "Clear buffer on receive");
    dmesg_args.color = arg_lit0("o", "color", "Color the output");
    dmesg_args.purge = arg_lit0("p", "purge", "Purge buffer without printing");
    dmesg_args.stats = arg_lit0("s", "stats", "Print log buffer stats");
    dmesg_args.end = arg_end(2);

    const esp_console_cmd_t dmesg_cmd = {
        .command = "dmesg",
        .help = "Print log buffer",
        .hint = NULL,
        .func = &cmd_dmesg,
        .argtable = &dmesg_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&dmesg_cmd));

    return ESP_OK;
}
