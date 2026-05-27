# ESP32-S3-ETH-8DI-8RO Custom Firmware

Custom ESP-IDF firmware for the ESP32-S3-ETH-8DI-8RO industrial relay board.

## Features

- **REST API** — control relays, read relay/input states, configure webhooks, input modes, and NTP
- **Single-page web UI** — relay control, digital input monitoring, WiFi/network settings, webhook configuration, input mode selection, NTP settings, API docs
- **Input Modes** — per-input: no relay action / Momentary (rising edge toggles target relay(s)) / Latching (relay mirrors input)
- **Multi-relay target** — each digital input can target any combination of the 8 relays (bitmask)
- **Webhooks** — HTTP POST to a configurable URL on digital input state change (rising, falling, or any edge); independent of relay mode; can be enabled/disabled per input
- **NTP** — configurable NTP server and POSIX timezone; current time shown in web UI
- **Dual network** — W5500 SPI Ethernet (DHCP) and WiFi station mode (DHCP); credentials persisted in NVS flash; WiFi reconnects indefinitely after disconnect
- **Status LED** — orange blinking while waiting for network, solid blue once connected
- **Robust by design** — `configASSERT` on all FreeRTOS allocations, bounds checks on all NVS keys, HTTP 500 on OOM, 3 s HTTP timeout vs 15 s WDT

## REST API

Base URL: `http://<device-ip>`

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/relays` | Read all 8 relay states |
| POST | `/api/v1/relays/{1-8}` | Set a relay — body: `{"state": 1}` |
| GET | `/api/v1/inputs` | Read all 8 digital input states |
| GET | `/api/v1/state` | Combined relay + input snapshot (used for live polling) |
| GET | `/api/v1/input_modes` | Read all 8 input modes + relay targets |
| POST | `/api/v1/input_modes/{1-8}` | Set input mode — body: `{"mode": 1, "relay_mask": 3}` |
| GET | `/api/v1/network` | Network status (IP addresses, connection state) |
| POST | `/api/v1/network/wifi` | Save WiFi credentials — body: `{"ssid":"…","password":"…"}` |
| GET | `/api/v1/webhooks` | Read all webhook configurations |
| POST | `/api/v1/webhooks/{1-8}` | Set webhook — body: `{"url":"http://…","trigger":"rising","enabled":true}` |
| GET | `/api/v1/ntp` | Read NTP config and current device time |
| POST | `/api/v1/ntp` | Set NTP server + timezone — body: `{"server":"pool.ntp.org","tz":"CET-1CEST,M3.5.0,M10.5.0/3"}` |

Full API documentation is also available in the web UI under the **API** tab.

### Input modes

| Value | Name | Behaviour |
|-------|------|-----------|
| `0` | Off | No relay action |
| `1` | Momentary | Rising edge toggles the target relay(s) |
| `2` | Latching | Target relay(s) ON while input is LOW, OFF while HIGH |

The relay target is a bitmask — bit 0 = Relay 1, bit 7 = Relay 8. Multiple relays can be targeted simultaneously. Webhooks fire **independently** of the relay mode when a URL and matching trigger are configured.

### Webhook payload

```json
POST <configured-url>
Content-Type: application/json

{"input": 1, "state": 1, "trigger": "rising", "timestamp_ms": 12345}
```

`trigger` values: `rising`, `falling`, `change`

---

## Build & Flash (macOS)

### Prerequisites

ESP-IDF **v6.0.1** — install via the [ESP-IDF installer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html#installation) or use `eim_config.toml`:

```bash
# Run once to set up the Python environment
~/.espressif/v6.0.1/esp-idf/install.sh esp32s3
```

### Build & Flash

```bash
# Activate ESP-IDF environment (required in every new shell)
source ~/.espressif/v6.0.1/esp-idf/export.sh

# First time only
idf.py set-target esp32s3

# Build
idf.py build

# Flash  (find port: ls /dev/cu.*)
idf.py -p /dev/cu.usbmodem* flash

# Flash + open serial monitor
idf.py -p /dev/cu.usbmodem* flash monitor
# Exit monitor with Ctrl+]

# Monitor only (no flash)
idf.py -p /dev/cu.usbmodem* monitor

# Factory reset (erase NVS + app)
idf.py -p /dev/cu.usbmodem* erase-flash
```

---

## Board Overview

[ESP32-S3-ETH-8DI-8RO](https://www.waveshare.com/wiki/ESP32-S3-ETH-8DI-8RO) is an industrial-grade board with 8 relay outputs and 8 optocoupler-isolated digital inputs, built around the ESP32-S3 microcontroller.

**Specifications**
- Supply: 7–36 V DC (screw terminal) or 5 V USB-C
- Relay channels: 8 × 1NO 1NC, ≤10 A / 250 V AC
- Digital input channels: 8 (optocoupler isolated)
- Communication: WiFi 2.4 GHz, 10/100 Ethernet (W5500), RS485, USB-C

**Onboard ICs**

| IC | Interface | Purpose |
|----|-----------|---------|
| ESP32-S3 | — | Main MCU, WiFi, BLE |
| TCA9554PWR | I2C (SCL=41, SDA=42) | IO expander — relay control |
| PCF85063ATL | I2C (SCL=41, SDA=42) | RTC |
| W5500 | SPI (MOSI=13, MISO=14, SCLK=15, CS=16, INT=12) | Ethernet |
| WS2812 | GPIO38 | RGB status LED |
| Buzzer | GPIO46 | Audible indicator |

**GPIO Map**

| GPIO | Function |
|------|----------|
| 4–11 | Digital inputs 1–8 |
| 12 | ETH_INT |
| 13 | ETH_MOSI |
| 14 | ETH_MISO |
| 15 | ETH_SCLK |
| 16 | ETH_CS |
| 17 | RS485 TX |
| 18 | RS485 RX |
| 38 | WS2812 RGB LED |
| 41 | I2C SCL |
| 42 | I2C SDA |
| 46 | Buzzer |
| EXIO1–8 | Relay 1–8 (via TCA9554) |
