#pragma once

#include "esp_log.h"
#include "esp_system.h"

#include "freertos/FreeRTOSConfig.h"

#include "log_capture.h"

esp_err_t log_print_early_init(void);
void print_log_entry(struct log_entry_s *entry, FILE *output);
void print_log_entry_color(struct log_entry_s *entry, FILE *output);
SemaphoreHandle_t print_log_get_mutex();