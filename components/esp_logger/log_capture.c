

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/time.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "log_capture.h"
#include "log_common.h"

// Override original vprint handler, and prefix log line with thread name.
static vprintf_like_t original_handler;

#define MAX_LOG_HANDLERS 10
#define LOCAL_STORAGE_INDEX 1

static void (*handlers[MAX_LOG_HANDLERS])(struct log_entry_s *e);

const char *log_level_names[6] = {"none", "error", "warn", "info", "debug", "verbose"};

static uint8_t log_level_from_char(char c)
{
    switch (c) {
    case 'N':
        return ESP_LOG_NONE;
    case 'E':
        return ESP_LOG_ERROR;
    case 'W':
        return ESP_LOG_WARN;
    case 'I':
        return ESP_LOG_INFO;
    case 'D':
        return ESP_LOG_DEBUG;
    case 'V':
        return ESP_LOG_VERBOSE;
    default:
        return ESP_LOG_VERBOSE;
    }
}

static uint64_t current_timestamp_ms()
{
    struct timeval te;
    gettimeofday(&te, NULL);                                        // get current time
    uint64_t milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}

static int vprintf_handler(const char *fmt, va_list args)
{
    int ret = 0;
    bool tls_entry = false;

    /*
     *  99% of all logs, is one log line, with an ending newline for every call to this handler.
     *  There is at least two exceptions, and thats in the wifi and network task whe logs is sent in multiple parts.
     *  To handle this, we need to allocate a log_entry in thread local storage, build on that and
     *  then when done, we will commit that log to the pipe.
     *  The entry will then reside in TLS for the remaning of the time.
     *
     *  Try to look up if there is a TLS storage log_entry for this task.
     */
    struct log_entry_s *e = (struct log_entry_s *)pvTaskGetThreadLocalStoragePointer(NULL, LOCAL_STORAGE_INDEX);
    if (e) {
        tls_entry = true;
        // printf("Found log_entry in thread local heap for task: '%s'\n", pcTaskGetName(NULL));
    }

    /*
     * If there is not a log_entry in TLS, just create one on the heap for us to use.
     */
    if (!e) {
        e = alloca(sizeof(struct log_entry_s));
        if (!e) {
            // printf("OOM trying to log.\n");
            return 0;
        }
        memset(e, 0, sizeof(struct log_entry_s));
    }

    // This format, always have one log per printf call.
    if (fmt[0] && strncmp(fmt + 1, " (%lu) %s: ", 11) == 0) {
        e->level = log_level_from_char(fmt[0]);
        e->uptime = va_arg(args, long unsigned);
        e->tag = va_arg(args, char *);
        e->data_len = 0;
        fmt += 12;
    }
    // Look for the header in fmt, if found take that appart.
    else if (strncmp(fmt, "%c (%lu) %s:", 12) == 0) {
        e->level = log_level_from_char(va_arg(args, int));
        e->uptime = va_arg(args, long unsigned);
        e->tag = va_arg(args, char *);
        e->data_len = 0;
        fmt += 12;
    } else if (strncmp(fmt, "%c (%d) %s:", 11) == 0) {
        e->level = log_level_from_char(va_arg(args, int));
        e->uptime = va_arg(args, uint32_t);
        e->tag = va_arg(args, char *);
        e->data_len = 0;
        fmt += 11;
    }

    // Add some extra stuff
    memset(e->task, 0, sizeof(e->task));
    if (pcTaskGetName(NULL)) {
        strcpy(e->task, pcTaskGetName(NULL));
    }
    e->core = xPortGetCoreID();
    e->timestamp = current_timestamp_ms();

    /*
     *  int vsnprintf(char str[size], size_t size, const char *format, va_list ap);
     *
     *  The function vsnprintf() write at most size bytes (including the terminating null byte ('\0')) to str.
     *
     *  Upon successful return, these function return the number of characters printed (excluding the null byte used to end output to strings).
     *  The  functions snprintf() and vsnprintf() do not write more than size bytes (including the terminating null byte ('\0')).
     *  If the output was truncated due to this limit, then the return value is the number of characters (excluding the terminating null byte)
     *  which would have been written to the final string if enough space had been available.
     *  Thus, a return value of size or more means that the output was truncated.
     *
     */

    // Append the data to the log entry.
    size_t free_bytes = sizeof(e->data) - e->data_len;
    if (*fmt && free_bytes > 0) {
        ret = vsnprintf(e->data + e->data_len, free_bytes, fmt, args); // Returns bytes excluding the newline
        if (ret > 0 && ret <= free_bytes) {
            e->data_len = e->data_len + ret;
        } else if (ret > 0) { // Concatinated
            e->data_len += free_bytes;

            // Add some marker showing that the log line was cut.
            memcpy(e->data + sizeof(e->data) - 2, "||", 2);
        }
    }
    if (e->data_len > sizeof(e->data))
        abort();

    // On newline, commit to log buffer
    if (e->data_len > 0 && (e->data[e->data_len - 1] == '\n' || e->data_len >= sizeof(e->data) - 2)) {
        // Trim ending newlines.
        while (e->data_len > 0 && e->data[e->data_len - 1] == '\n')
            e->data_len--; // Skip ending newline

        if (e->data_len > 0) {
            if (e->data_len > sizeof(e->data))
                abort();

            // Replace unprintable characters with '.'
            for (size_t i = 0; i < e->data_len; i++) {
                if (!isprint((unsigned char)e->data[i])) {
                    e->data[i] = '.';
                }
            }

            log_capture_send_log(e);
            e->data_len = 0;
        }
    } else {

        /*
         * In case there was not a complete log row on this callback, we need to copy the log_entry into the
         * heap, and save it in TLS.
         */
        if (!tls_entry) {
            // printf("Moving log struct to heap in task: '%s' fmt: '%s'\n", e->task, fmt);
            // We need to copy the struct from stack to heap.
            struct log_entry_s *heap_entry = pvPortMalloc(sizeof(struct log_entry_s));
            if (!heap_entry)
                return 0;
            memcpy(heap_entry, e, sizeof(struct log_entry_s));
            vTaskSetThreadLocalStoragePointer(NULL, LOCAL_STORAGE_INDEX, (void *)heap_entry);
        }
    }
    return ret;
}

void log_capture_send_log(log_entry_t *log_entry)
{
    for (size_t i = 0; i < MAX_LOG_HANDLERS; i++)
        if (handlers[i] != NULL)
            handlers[i](log_entry);
}

esp_err_t log_capture_early_init()
{
    original_handler = esp_log_set_vprintf(vprintf_handler);
    return ESP_OK;
}

esp_err_t log_capture_register_handler(log_entry_cb_t cb)
{
    for (size_t i = 0; i < MAX_LOG_HANDLERS; i++) {
        if (handlers[i] == NULL) {
            handlers[i] = cb;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

char *log_printable_char(char c)
{
    static char buf[5];
    switch (c) {
    case 0x02:
        sprintf(buf, "STX");
        break;
    case 0x03:
        sprintf(buf, "ETX");
        break;
    case 0x0A:
        sprintf(buf, "LF ");
        break;
    case 0x0D:
        sprintf(buf, "CR ");
        break;
    case 0x06:
        sprintf(buf, "ACK");
        break;
    case 0x0:
        sprintf(buf, "NUL");
        break;
    default:
        if (isprint(c)) {
            sprintf(buf, "'%c'", c);
        } else {
            sprintf(buf, "%2X ", c);
        }
        break;
    }
    return buf;
}

char *log_printable_char2(char c)
{
    static char buf[6];
    switch (c) {
    case 0x02:
        sprintf(buf, "<STX>");
        break;
    case 0x03:
        sprintf(buf, "<ETX>");
        break;
    case 0x0A:
        sprintf(buf, "<LF>");
        break;
    case 0x0D:
        sprintf(buf, "<CR>");
        break;
    case 0x06:
        sprintf(buf, "<ACK>");
        break;
    case 0x0:
        sprintf(buf, "<NUL>");
        break;
    default:
        if (isprint(c)) {
            sprintf(buf, "%c", c);
        } else {
            sprintf(buf, "<%2X>", c);
        }
        break;
    }
    return buf;
}

int log_array(esp_log_level_t log_level, const char *tag, const char *prefix, const uint8_t *data, size_t data_size)
{
    for (uint32_t i = 0; i < data_size; i += 8) {
        char buffer[CONFIG_LOGGER_LOG_MAX_LOG_LINE_SIZE];
        size_t buffer_pos = sprintf(buffer, "%04lx: ", (long unsigned int)i);
        for (uint32_t j = i; j < data_size && j < i + 8; j++)
            buffer_pos += sprintf(&buffer[buffer_pos], "%02x ", data[j]);
        while (buffer_pos < ((8 * 3) + 10))
            buffer[buffer_pos++] = ' ';
        for (uint32_t j = i; j < data_size && j < i + 8; j++)
            buffer_pos += sprintf(&buffer[buffer_pos], "%s ", log_printable_char(data[j]));
        ESP_LOG_LEVEL_LOCAL(log_level, tag, "%s %s", prefix, buffer);
    }
    return 0;
}

int log_int16_array(esp_log_level_t log_level, const char *tag, const char *prefix, const char *data, size_t data_size)
{
    uint32_t i = 0;
    while (i < data_size) {
        char buffer[CONFIG_LOGGER_LOG_MAX_LOG_LINE_SIZE];
        size_t buffer_pos = 0;
        while (i < data_size && buffer_pos < 110) {
            int16_t *temp = (int16_t *)&data[i += 2];
            buffer_pos += snprintf(&buffer[buffer_pos], sizeof(buffer) - buffer_pos, "%d,", *temp);
            if (data[i - 1] == '\n')
                break;
        }

        ESP_LOG_LEVEL_LOCAL(log_level, tag, "%s = \"%s\"", prefix, buffer);
    }
    return 0;
}

int log_string(esp_log_level_t log_level, const char *tag, const char *prefix, const char *data, size_t data_size)
{
    uint32_t i = 0;
    while (i < data_size) {
        char buffer[CONFIG_LOGGER_LOG_MAX_LOG_LINE_SIZE];
        size_t buffer_pos = 0;
        while (i < data_size && buffer_pos < 100) {
            buffer_pos += sprintf(&buffer[buffer_pos], "%s", log_printable_char2(data[i++]));
            if (data[i - 1] == '\n')
                break;
        }

        ESP_LOG_LEVEL_LOCAL(log_level, tag, "%s = \"%s\"", prefix, buffer);
    }
    return 0;
}
