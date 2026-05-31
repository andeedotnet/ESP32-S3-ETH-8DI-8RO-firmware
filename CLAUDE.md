# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Custom ESP-IDF v6.0.1 firmware for the **ESP32-S3-ETH-8DI-8RO** industrial board: 8 relay outputs (TCA9554PWR I2C expander), 8 optocoupler-isolated digital inputs (GPIO4–11), W5500 SPI Ethernet, WiFi station mode, WS2812 RGB LED, buzzer. Exposes a REST API and a single-page web UI (relay control, input monitoring, WiFi settings, webhook config, input mode selection, API docs).

## Build & Flash

Requires ESP-IDF v6.0.1 at `~/.espressif/v6.0.1/esp-idf`. The `eim_config.toml` holds the installer config.

```bash
# Activate ESP-IDF environment (required before any idf.py command)
source ~/.espressif/v6.0.1/esp-idf/export.sh

# Set target (first time only)
idf.py set-target esp32s3

# Build
idf.py build

# Flash (find port: ls /dev/cu.*)
idf.py -p /dev/cu.usbmodem* flash

# Flash + serial monitor
idf.py -p /dev/cu.usbmodem* flash monitor
# Exit with Ctrl+]

# Monitor only
idf.py -p /dev/cu.usbmodem* monitor

# Factory reset
idf.py -p /dev/cu.usbmodem* erase-flash
```

## Code Architecture

All source lives in `main/` as a **single ESP-IDF component**. Sub-directories are plain source folders — not separate components. `main/CMakeLists.txt` registers everything in one `idf_component_register()`. External dependencies (`espressif/cjson`, `espressif/ethernet_init`) are declared in `main/idf_component.yml`.

| Module | Path | Purpose |
|--------|------|---------|
| `app_state` | `main/` | Shared runtime state struct + FreeRTOS mutex (`g_state`, `g_state_mutex`) |
| `board_config.h` | `main/` | All GPIO/I2C/SPI pin and address constants — single source of truth |
| `i2c_bus` | `main/i2c_bus/` | Shared `i2c_master_bus_handle_t`; both TCA9554 and PCF85063 attach to it |
| `relay` | `main/relay/` | TCA9554PWR driver; shadow byte for read-modify-write without I2C readback |
| `digital_input` | `main/digital_input/` | GPIO ISR → 20 ms debounce task → `g_di_change_queue` |
| `ethernet` | `main/ethernet/` | W5500 via `espressif/ethernet_init ~1.3.0`; sets `LED_CONNECTED` on IP event |
| `wifi_manager` | `main/wifi/` | STA mode, always reconnects on disconnect (no give-up limit) with exponential backoff 1→30 s via `esp_timer`, credentials from NVS |
| `ntp` | `main/ntp/` | SNTP init from NVS server + timezone; restartable via `ntp_init()` |
| `nvs_config` | `main/nvs_config/` | NVS namespace `"app_cfg"`: WiFi creds, 8 webhook configs, 8 input modes + relay targets, NTP server/tz |
| `http_server` | `main/http_server/` | REST endpoints + serves embedded web UI assets; wildcard URI matching |
| `webhook` | `main/webhook/` | Consumes `g_di_change_queue`; relay dispatch (momentary/latching) + webhook POST (independent of relay mode) |
| `led` | `main/led/` | WS2812 via RMT copy encoder; pattern queue (depth 1, `xQueueOverwrite`) |
| `buzzer` | `main/buzzer/` | GPIO46 simple on/off |
| `health` | `main/health/` | Low-prio monitor task subscribed to the Task WDT; logs heap trend, reboots on critical heap floor (`HEALTH_HEAP_FLOOR`) |

**LED patterns** (`main/led/led.h`):
- `LED_BOOTING` — orange blink (500 ms on/off), active from boot until network
- `LED_CONNECTED` — solid blue, set by ethernet and wifi event handlers on IP assignment
- `LED_NO_NETWORK` — fast red blink
- `LED_OFF` — off

**Web UI** (`main/web_ui/`): single-page HTML embedded via `target_add_binary_data()`. JS extracted to `main/web_ui/static/app.js` (also embedded). Bootstrap 5.3 CSS+JS embedded from `main/web_ui/static/bootstrap/`. No filesystem partition needed. Navigation links are in the navbar (Bootstrap `navbar-nav flex-row` with `data-bs-toggle="tab"`). Tabs: Relays, Inputs, Network, Webhooks, NTP, API. Relay/input states polled every 500 ms via `GET /api/v1/state`.

**W5500 in ESP-IDF v6.0**: `esp_eth_mac_new_w5500()` was removed. Use `espressif/ethernet_init ~1.3.0` — it provides `ethernet_init_all()` and is configured via `sdkconfig.defaults` Kconfig entries.

**NVS first boot**: `nvs_open()` with `NVS_READONLY` returns `ESP_ERR_NVS_NOT_FOUND` when the namespace has never been written. Do not use `ESP_ERROR_CHECK` on it — handle gracefully as "no credentials stored".

**GPIO ISR service**: `ethernet_init` installs it first; `gpio_install_isr_service()` in `di_init()` returns `ESP_ERR_INVALID_STATE` which is expected and ignored.

**`-Werror=format-truncation`**: Use static key arrays instead of `snprintf` for NVS key construction (see `s_mode_keys[]` in `nvs_config.c`).

## Input Modes

Each digital input has an independently configurable mode stored in NVS (`di1_mode`…`di8_mode`):

| Value | Name | Behaviour |
|-------|------|-----------|
| `0` | Off | No relay action |
| `1` | Momentary | Rising edge toggles target relay(s) |
| `2` | Latching | Target relay(s) ON while input LOW, OFF while HIGH |

Mode dispatch happens in `webhook.c`'s task. Relay target is a **bitmask** (`uint8_t`, bit N = relay N+1) stored in NVS (`di1_rly`…`di8_rly`); default is `1 << input_idx`. Webhooks fire **independently** of relay mode — if a URL is configured and the trigger matches, the webhook fires regardless of which relay mode is active. Latching inputs apply their relay state at boot via `webhook_apply_boot_states()`.

## Hardware Architecture

| Component | Interface | GPIOs | Purpose |
|-----------|-----------|-------|---------|
| TCA9554PWR | I2C | SCL=41, SDA=42 | IO expander — all 8 relays via EXIO1–EXIO8 |
| PCF85063ATL | I2C | SCL=41, SDA=42 | RTC (shares I2C bus) |
| W5500 | SPI | INT=12, MOSI=13, MISO=14, SCLK=15, CS=16 | 10/100 Ethernet |
| WS2812 | GPIO38 | — | RGB status LED |
| Buzzer | GPIO46 | — | Audible indicator |
| Digital inputs | GPIO4–11 | — | Optocoupler-isolated, active HIGH |

Relay control goes through the TCA9554 expander — not directly via ESP32 GPIOs.

## REST API

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Web UI (HTML) |
| GET | `/static/*` | Embedded static assets (JS, CSS) |
| GET | `/api/v1/relays` | All relay states `{"relays":[0,1,...]}` |
| POST | `/api/v1/relays/{1-8}` | Set relay `{"state":1}` |
| GET | `/api/v1/inputs` | All input states `{"inputs":[1,0,...]}` |
| GET | `/api/v1/state` | Combined snapshot `{"relays":[...],"inputs":[...]}` — used for 500 ms polling |
| GET | `/api/v1/input_modes` | All input modes `{"modes":[{"mode":0,"relay_mask":1},...]}` |
| POST | `/api/v1/input_modes/{1-8}` | Set input mode `{"mode":1,"relay_mask":3}` |
| GET | `/api/v1/network` | Network status + stored SSID |
| POST | `/api/v1/network/wifi` | Save WiFi creds `{"ssid":"…","password":"…"}` |
| GET | `/api/v1/webhooks` | All webhook configs (includes `enabled` field) |
| POST | `/api/v1/webhooks/{1-8}` | Set webhook `{"url":"…","trigger":"rising","enabled":true}` |
| GET | `/api/v1/ntp` | NTP config + current time `{"server":"…","tz":"…","time":"…","synced":true}` |
| POST | `/api/v1/ntp` | Set NTP `{"server":"pool.ntp.org","tz":"CET-1CEST,M3.5.0,M10.5.0/3"}` |

**Robustness notes:**
- All JSON handlers NULL-check `cJSON_PrintUnformatted()` and return HTTP 500 on heap exhaustion.
- WiFi always retries on disconnect — no give-up limit — but with exponential backoff (1→30 s) so a down/flapping AP can't spin a tight reconnect loop. Backoff resets on `GOT_IP` and on user-initiated reconnect.
- Webhook HTTP timeout is 3 s; task WDT is 15 s (`CONFIG_ESP_TASK_WDT_TIMEOUT_S=15`) with `CONFIG_ESP_TASK_WDT_PANIC=y` — a genuinely hung task reboots the board.
- `webhook_task`, `di_task` and the `health` task are subscribed to the Task WDT (`esp_task_wdt_add`) and feed it each loop; their queue receives use a 1 s timeout so a blocked task still resets the dog rather than waiting on `portMAX_DELAY`.
- `health` task reboots the board if free heap drops below `HEALTH_HEAP_FLOOR` (24 KB) — pre-empts allocation failures across HTTP/TCP-IP.
- Panics write a core dump to the `coredump` flash partition (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`); read it back with `idf.py -p PORT coredump-info` / `coredump-debug`.
- Bluetooth is fully disabled (`CONFIG_BT_ENABLED=n` in `sdkconfig.defaults`) — Ethernet/WiFi-only device, no BT controller in the image.
- `tca9554_write_output()` retries up to 3× so a transient I2C glitch doesn't silently drop a relay command.
- `webhook_post()` checks `g_state.eth_connected || g_state.wifi_connected` before attempting HTTP.
- All FreeRTOS object allocations (mutex, queues, tasks) guarded with `configASSERT()`.

**Config regeneration:** `sdkconfig.defaults` is the source of truth and is only applied when `sdkconfig` is absent. After editing defaults, regenerate with `rm sdkconfig && idf.py reconfigure` (or `idf.py fullclean` if the build dir was moved between absolute paths — stale toolchain spec paths otherwise break the build).
