#pragma once
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define ANSI_COLOR_RED "31"
#define ANSI_COLOR_BLACK "30"
#define ANSI_COLOR_GREEN "32"
#define ANSI_COLOR_BROWN "33"
#define ANSI_COLOR_BLUE "34"
#define ANSI_COLOR_PURPLE "35"
#define ANSI_COLOR_CYAN "36"
#define ANSI_COLOR(COLOR) "\033[0;" COLOR "m"
#define ANSI_BOLD(COLOR) "\033[1;" COLOR "m"
#define ANSI_RESET_COLOR "\033[0m"

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ARRAY_INDEX(x, y) ((uint32_t)((char *)(x) - (char *)(y)) / sizeof((y)[0]))

#undef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define STRLEN(str) (sizeof(str) - 1)

#define RET_RETURN(x)                                                     \
    {                                                                     \
        esp_err_t RETX = (x);                                             \
        if (RETX != ESP_OK) {                                             \
            ESP_LOGE(TAG, "Error %x in %s:%d", RETX, __FILE__, __LINE__); \
            return RETX;                                                  \
        }                                                                 \
    }

#define TICKS_TO_MS(ticks) ((ticks)*portTICK_PERIOD_MS)
#define MS_TO_TICKS(ms) ((ms) / portTICK_PERIOD_MS)

#define TICKS_TO_S(ticks) (TICKS_TO_MS(ticks) / 1000)
#define S_TO_TICKS(ms) (MS_TO_TICKS(ms * 1000))

#define US_PER_MS (1000)

#define MS_PER_SEC (1000)
#define MS_PER_MIN (60 * 1000)
#define MS_PER_HOUR (60 * MS_PER_MIN)
#define MS_PER_DAY (24 * MS_PER_HOUR)

#define SEC_PER_MIN (60)
#define SEC_PER_HOUR (60 * SEC_PER_MIN)
#define SEC_PER_DAY (24 * SEC_PER_HOUR)



