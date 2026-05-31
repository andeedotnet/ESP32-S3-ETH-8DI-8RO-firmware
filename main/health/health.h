#pragma once

#include "esp_err.h"

/*
 * Background health monitor. Runs a low-priority task that:
 *   - is subscribed to the Task Watchdog, so a starved scheduler trips the WDT
 *     (and, with CONFIG_ESP_TASK_WDT_PANIC=y, reboots the board);
 *   - periodically logs free / minimum-ever heap for trend analysis;
 *   - reboots deliberately if free heap falls below a critical floor, before
 *     allocations start failing across the HTTP server and network stack.
 */
esp_err_t health_task_start(void);
