#pragma once
/*
* GPIO_Defs_ESP32_WROVER.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2025 Shelby Merrick
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
#define DEFAULT_RMT_0_GPIO     gpio_num_t::GPIO_NUM_4
#define DEFAULT_RMT_1_GPIO     gpio_num_t::GPIO_NUM_13

#define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_26
#define DEFAULT_RMT_3_GPIO      gpio_num_t::GPIO_NUM_33

//#define DEFAULT_RMT_4_GPIO      gpio_num_t::GPIO_NUM_32
//#define DEFAULT_RMT_5_GPIO      gpio_num_t::GPIO_NUM_25

//#define DEFAULT_RMT_6_GPIO      gpio_num_t::GPIO_NUM_27
//#define DEFAULT_RMT_7_GPIO      gpio_num_t::GPIO_NUM_14

#define BUTTON_GPIO1            gpio_num_t::GPIO_NUM_34
//#define STATUS_LED1             gpio_num_t::GPIO_NUM_2
//#define STATUS_LED2             gpio_num_t::GPIO_NUM_0


// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_19
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_23
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_18
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_5

// Output Types
#define SUPPORT_OutputType_DMX              // UART / RMT
#define SUPPORT_OutputType_GECE             // UART / RMT
#define SUPPORT_OutputType_GS8208           // UART / RMT
#define SUPPORT_OutputType_Renard           // UART / RMT
#define SUPPORT_OutputType_Serial           // UART / RMT
#define SUPPORT_OutputType_TM1814           // UART / RMT
#define SUPPORT_OutputType_UCS1903          // UART / RMT
#define SUPPORT_OutputType_UCS8903          // UART / RMT
#define SUPPORT_OutputType_WS2811           // UART / RMT
