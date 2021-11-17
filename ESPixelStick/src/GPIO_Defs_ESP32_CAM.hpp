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
#define DEFAULT_UART_1_GPIO     gpio_num_t::GPIO_NUM_0
#define DEFAULT_UART_2_GPIO     gpio_num_t::GPIO_NUM_1
#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_3
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_16
#define LED_GPIO                gpio_num_t::GPIO_NUM_4

// File Manager
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_2
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_15
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_14
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_13
