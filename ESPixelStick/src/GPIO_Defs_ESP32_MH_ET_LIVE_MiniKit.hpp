#pragma once
/*
* GPIO_Defs_ESP32_MH_ET_LIVE_MiniKit.hpp - Output Management class
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

//Output Manager
#define DEFAULT_UART_1_GPIO     gpio_num_t::GPIO_NUM_16
// #define DEFAULT_UART_2_GPIO     gpio_num_t::GPIO_NUM_21
#define UART_LAST               OutputChannelId_UART_1
// #define UART_LAST               OutputChannelId_UART_2

// #define SUPPORT_RMT_OUTPUT
// #define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_17
// #define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_22
// #define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_26
// #define RMT_LAST                OutputChannelId_RMT_3

// #define SUPPORT_RELAY_OUTPUT
// #define SUPPORT_OutputType_WS2801
// #define SUPPORT_OutputType_APA102
// #define SUPPORT_OutputType_TM1814
// #define SUPPORT_OutputType_TLS3001

// #define SUPPORT_RELAY_OUTPUT

#if defined(SUPPORT_OutputType_WS2801) || defined(SUPPORT_OutputType_APA102)
#   define SUPPORT_SPI_OUTPUT

// SPI Output
#define DEFAULT_SPI_DATA_GPIO  gpio_num_t::GPIO_NUM_15
#define DEFAULT_SPI_CLOCK_GPIO gpio_num_t::GPIO_NUM_25

#endif // defined(SUPPORT_OutputType_WS2801) || defined(SUPPORT_OutputType_TM1814)

// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_19
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_23
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_18
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_5
