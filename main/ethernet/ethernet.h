#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t eth_init(void);
bool      eth_is_connected(void);
