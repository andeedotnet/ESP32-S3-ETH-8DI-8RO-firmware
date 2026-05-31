#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t relay_init(void);
esp_err_t relay_set(uint8_t relay_num, bool state);   /* relay_num: 1-based */
uint8_t   relay_get_mask(void);                        /* bit 0 = relay 1 */

/* Atomic multi-relay operations — the read-modify-write happens under the same
 * lock as the I2C write so a concurrent relay_set() can't be lost (TOCTOU-safe).
 * Both take a bitmask where bit 0 = relay 1 … bit 7 = relay 8. */
esp_err_t relay_set_masked(uint8_t affect_mask, bool on); /* set selected relays on/off */
esp_err_t relay_toggle_mask(uint8_t toggle_mask);         /* flip selected relays */
