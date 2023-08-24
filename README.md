# ESP Logger example

This is my log chain project for esp-idf.

## How to use example

These needs to be added in this order, early to start log buffering
```
    // This initialize a capture, that takes over esp-idf log output, and provides an interface
    // for log modules to hook up, using log_capture_register_handler()
    ESP_ERROR_CHECK(log_capture_early_init());

    // This module registers a buffer handler, that saves logs into RAM, for later retrival
    ESP_ERROR_CHECK(log_buffer_early_init());

    // This module initializes a colorful log print output, that we are used to.
    ESP_ERROR_CHECK(log_print_early_init());
```

This can be run to add commands:
```
    // These are less critical initiazions that adds console commands.
    ESP_ERROR_CHECK(log_buffer_init());
    ESP_ERROR_CHECK(log_test_init());
```


Make sure CONFIG_LOG_COLORS is NOT enabled in your sdk config, colors will be added anyway from our own printer.

Use dmesg to print your old logs.

Use log cmd to test log.

### Configure the project

## Example Output

## Troubleshooting
