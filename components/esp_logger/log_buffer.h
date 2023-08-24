#pragma once

#include "esp_log.h"
#include "esp_system.h"

#include "freertos/FreeRTOSConfig.h"

#include "log_capture.h"

esp_err_t log_buffer_init(void);
esp_err_t log_buffer_early_init(void);

bool log_pull_entry(struct log_entry_s *entry);
bool log_peek_entry(struct log_entry_s *entry, uint32_t *index);
