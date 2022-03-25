#pragma once
/*
* GPIO_Defs_ESP32_CAM.hpp - Output Management class
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
#define SUPPORT_UART_OUTPUT
#define DEFAULT_UART_1_GPIO     gpio_num_t::GPIO_NUM_0
#define DEFAULT_UART_2_GPIO     gpio_num_t::GPIO_NUM_1
#define UART_LAST               OutputChannelId_UART_2

#define SUPPORT_RMT_OUTPUT
#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_3
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_16
#define RMT_LAST                OutputChannelId_RMT_1

#define LED_FLASH_GPIO          gpio_num_t::GPIO_NUM_33
#define LED_FLASH_OFF           HIGH

// File Manager
// #define USE_MISO_PULLUP
// #define SUPPORT_SD_MMC
#define SUPPORT_SD
#ifdef SUPPORT_SD_MMC
#   define SD_CARD_CMD             gpio_num_t::GPIO_NUM_15
#   define SD_CARD_CLK             gpio_num_t::GPIO_NUM_14
#   define SD_CARD_DATA_0          gpio_num_t::GPIO_NUM_2
#   define SD_CARD_DATA_1          gpio_num_t::GPIO_NUM_4
#   define SD_CARD_DATA_2          gpio_num_t::GPIO_NUM_12
#   define SD_CARD_DATA_3          gpio_num_t::GPIO_NUM_13
#endif // def SUPPORT_SD_MMC

#ifdef SUPPORT_SD
#   define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_2
#   define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_15
#   define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_14
#   define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_13
#endif // SUPPORT SD

// Output Types
// #define SUPPORT_OutputType_TLS3001
// #define SUPPORT_OutputType_APA102
#define SUPPORT_OutputType_DMX
#define SUPPORT_OutputType_GECE
// #define SUPPORT_OutputType_GS8208
#define SUPPORT_OutputType_Renard
#define SUPPORT_OutputType_Serial
// #define SUPPORT_OutputType_TM1814
// #define SUPPORT_OutputType_UCS1903
// #define SUPPORT_OutputType_UCS8903
// #define SUPPORT_OutputType_WS2801
#define SUPPORT_OutputType_WS2811
// #define SUPPORT_RELAY_OUTPUT