#pragma once
/*
* GPIO_Defs.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#include "ESPixelStick.h"

// #define ESP32_CAM

#ifdef ARDUINO_ARCH_ESP32
#   include "driver/gpio.h"
#endif

#ifdef ARDUINO_ARCH_ESP8266
typedef enum
{
    GPIO_NUM_0 = 0,
    GPIO_NUM_1,
    GPIO_NUM_2,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_8,
    GPIO_NUM_9,
    GPIO_NUM_10,
    GPIO_NUM_11,
    GPIO_NUM_12,
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_15,
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
    GPIO_NUM_19,
    GPIO_NUM_20,
    GPIO_NUM_21,
    GPIO_NUM_22,
    GPIO_NUM_23,
    GPIO_NUM_24,
    GPIO_NUM_25,
    GPIO_NUM_26,
    GPIO_NUM_27,
    GPIO_NUM_28,
    GPIO_NUM_29,
    GPIO_NUM_30,
    GPIO_NUM_31,
    GPIO_NUM_32,
    GPIO_NUM_33,
    GPIO_NUM_34,
} gpio_num_t;

typedef enum
{
    UART_NUM_0 = 0,
    UART_NUM_1,
    UART_NUM_2
} uart_port_t;
#endif // def ARDUINO_ARCH_ESP8266

#ifdef ESP32_CAM

//Output Manager definitions
#define DEFAULT_UART_1_GPIO     gpio_num_t::GPIO_NUM_0
#define DEFAULT_UART_2_GPIO     gpio_num_t::GPIO_NUM_1

#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_3
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_16

// File Manager SPI definitions
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_2
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_15
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_14
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_13

#else

//Output Manager definitions
#define DEFAULT_UART_1_GPIO     gpio_num_t::GPIO_NUM_2
#define DEFAULT_UART_2_GPIO     gpio_num_t::GPIO_NUM_13

#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_12
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_14
#define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_32
#define DEFAULT_RMT_3_GPIO      gpio_num_t::GPIO_NUM_33
// #define DEFAULT_RMT_4_GPIO      gpio_num_t::GPIO_NUM_
// #define DEFAULT_RMT_5_GPIO      gpio_num_t::GPIO_NUM_
// #define DEFAULT_RMT_6_GPIO      gpio_num_t::GPIO_NUM_
// #define DEFAULT_RMT_7_GPIO      gpio_num_t::GPIO_NUM_

// WS2801 SPI definitions
#define DEFAULT_SPI_DATA_GPIO  gpio_num_t::GPIO_NUM_15
#define DEFAULT_SPI_CLOCK_GPIO gpio_num_t::GPIO_NUM_25

// File Manager SPI definitions
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_19
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_23
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_18

#ifdef ARDUINO_ARCH_ESP32
#   define SD_CARD_CS_PIN      gpio_num_t::GPIO_NUM_4
#else
#   define SD_CARD_CS_PIN      gpio_num_t::GPIO_NUM_15
#endif
#endif // ndef ESP32_CAM
