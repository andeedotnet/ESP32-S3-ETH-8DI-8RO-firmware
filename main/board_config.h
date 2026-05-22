#pragma once

/* I2C bus (shared by TCA9554 relay expander and PCF85063 RTC) */
#define BOARD_I2C_SCL_GPIO      41
#define BOARD_I2C_SDA_GPIO      42
#define BOARD_I2C_FREQ_HZ       100000

/* TCA9554PWR I2C IO expander — controls relays 1-8 via EXIO1-EXIO8 */
#define BOARD_TCA9554_ADDR      0x20    /* A0=A1=A2=GND */
#define BOARD_TCA9554_REG_IN    0x00    /* input port (read-only) */
#define BOARD_TCA9554_REG_OUT   0x01    /* output port latch */
#define BOARD_TCA9554_REG_POL   0x02    /* polarity inversion */
#define BOARD_TCA9554_REG_CFG   0x03    /* direction: 0=output, 1=input */

/* PCF85063ATL RTC */
#define BOARD_PCF85063_ADDR     0x51

/* Digital inputs — optocoupler isolated, active LOW */
#define BOARD_DI_GPIO_BASE      4       /* inputs on GPIO4..GPIO11 */
#define BOARD_DI_COUNT          8

/* W5500 SPI Ethernet — configured via sdkconfig.defaults / ethernet_init component */
#define BOARD_ETH_INT_GPIO      12
#define BOARD_ETH_MOSI_GPIO     13
#define BOARD_ETH_MISO_GPIO     14
#define BOARD_ETH_SCLK_GPIO     15
#define BOARD_ETH_CS_GPIO       16

/* WS2812 RGB LED */
#define BOARD_RGB_LED_GPIO      38

/* Buzzer */
#define BOARD_BUZZER_GPIO       46

/* Relay count */
#define BOARD_RELAY_COUNT       8
