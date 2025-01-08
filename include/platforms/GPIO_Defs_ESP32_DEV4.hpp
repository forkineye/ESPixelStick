#pragma once
/*
* GPIO_Defs_ESP32_DEV4.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2024 Shelby Merrick
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
#define DEFAULT_RMT_0_GPIO      gpio_num_t::GPIO_NUM_13
#define DEFAULT_RMT_1_GPIO      gpio_num_t::GPIO_NUM_14
#define DEFAULT_RMT_2_GPIO      gpio_num_t::GPIO_NUM_27
#define DEFAULT_RMT_3_GPIO      gpio_num_t::GPIO_NUM_26


// SPI Output
#define SUPPORT_SPI_OUTPUT
#define DEFAULT_SPI_DATA_GPIO   gpio_num_t::GPIO_NUM_15
#define DEFAULT_SPI_CLOCK_GPIO  gpio_num_t::GPIO_NUM_25
#define DEFAULT_SPI_CS_GPIO     gpio_num_t::GPIO_NUM_0
#define DEFAULT_SPI_DEVICE      VSPI_HOST

#define DEFAULT_I2C_SDA         gpio_num_t::GPIO_NUM_21
#define DEFAULT_I2C_SCL         gpio_num_t::GPIO_NUM_22

// File Manager
#define SUPPORT_SD
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_19
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_23
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_18
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_5

// Output Types
// Not Finished - #define SUPPORT_OutputType_TLS3001
#define SUPPORT_OutputType_APA102           // SPI
#define SUPPORT_OutputType_DMX              // UART
#define SUPPORT_OutputType_GECE             // UART
#define SUPPORT_OutputType_GS8208           // UART / RMT
#define SUPPORT_OutputType_GRINCH           // SPI
#define SUPPORT_OutputType_Renard           // UART
#define SUPPORT_OutputType_Serial           // UART
#define SUPPORT_OutputType_TM1814           // UART / RMT
#define SUPPORT_OutputType_UCS1903          // UART / RMT
#define SUPPORT_OutputType_UCS8903          // UART / RMT
#define SUPPORT_OutputType_WS2801           // SPI
#define SUPPORT_OutputType_WS2811           // UART / RMT
#define SUPPORT_OutputType_Relay            // GPIO
#define SUPPORT_OutputType_Servo_PCA9685    // I2C (default pins)


#ifndef GPIO_INITIALIZER_HPP
#define GPIO_INITIALIZER_HPP

#include <driver/gpio.h> // Include the GPIO driver for ESP-IDF

// GPIO Pin Definitions
#define STATUS_LED_1_GPIO       gpio_num_t::GPIO_NUM_33
#define STATUS_LED_2_GPIO       gpio_num_t::GPIO_NUM_32
#define INPUT_1_GPIO            gpio_num_t::GPIO_NUM_35
#define INPUT_2_GPIO            gpio_num_t::GPIO_NUM_34
#define DEBUG_GPIO              gpio_num_t::GPIO_NUM_12
#define DEBUG_GPIO1             gpio_num_t::GPIO_NUM_14


class GPIOInitializer {
public:
    GPIOInitializer() {
        initializePins();
    }

private:
    void initializePins() {
        // Configure output pins
        configurePin(STATUS_LED_1_GPIO, GPIO_MODE_OUTPUT, 0); // Set low
        configurePin(STATUS_LED_2_GPIO, GPIO_MODE_OUTPUT, 0); // Set low

        // Configure input pins
        configurePin(INPUT_1_GPIO, GPIO_MODE_INPUT, 0);
        configurePin(INPUT_2_GPIO, GPIO_MODE_INPUT, 0);
    }

    void configurePin(gpio_num_t pin, gpio_mode_t mode, int initial_level) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.mode = mode;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;

        gpio_config(&io_conf);

        if (mode == GPIO_MODE_OUTPUT) {
            gpio_set_level(pin, initial_level);
        }
    }
};

// Static instance to ensure the constructor runs automatically
static GPIOInitializer gpioInitializer;

#endif // GPIO_INITIALIZER_HPP