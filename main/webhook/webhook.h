#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t webhook_task_start(void);
void      webhook_apply_boot_states(void);
