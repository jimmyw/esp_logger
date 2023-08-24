

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/queue.h>

#include "esp_console.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"

static struct {
    struct arg_str *tag;
    struct arg_int *level;
    struct arg_str *log;
    struct arg_end *end;
} log_test_args;

static int cmd_log_test(int argc, char **argv)
{

    int nerrors = arg_parse(argc, argv, (void **)&log_test_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, log_test_args.end, argv[0]);
        return 1;
    }

    ESP_LOG_LEVEL_LOCAL(log_test_args.level->ival[0],log_test_args.tag->sval[0], "%s", log_test_args.log->sval[0]);
    return 0;
}



esp_err_t log_test_init(void)
{
    log_test_args.tag = arg_str1(NULL, NULL, "log tag", "");
    log_test_args.level = arg_int1(NULL, NULL, "log level", "");
    log_test_args.log = arg_str1(NULL, NULL,  "log message", "");
    log_test_args.end = arg_end(3);

    const esp_console_cmd_t log_test_cmd = {
        .command = "log",
        .help = "Print log",
        .hint = NULL,
        .func = &cmd_log_test,
        .argtable = &log_test_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&log_test_cmd));

    return ESP_OK;
}
