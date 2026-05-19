#pragma once
/*
* GPIO_Defs_ESP32S3_DevkitC.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2025-2026 Shelby Merrick
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

// Output Manager
// MAX 8 Serial port on ESP32
const OM_OutputPortDefinition_t OM_OutputPortDefinitions[] =
{   
    
    {OM_PortId_t(0), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_2}},
    {OM_PortId_t(0), OM_PortType_t::OM_RELAY, {gpio_num_t::GPIO_NUM_2}},
    {OM_PortId_t(1), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_1}},
    {OM_PortId_t(1), OM_PortType_t::OM_RELAY, {gpio_num_t::GPIO_NUM_1}},
    {OM_PortId_t(2), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_8}},
    {OM_PortId_t(2), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_8}},
    {OM_PortId_t(3), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_9}},
    {OM_PortId_t(3), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_9}},
    //SD card reader gets very flaky /wifi doesn't work - skipping 5th output
    //{OM_PortId_t(4), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_48}},
    {OM_PortId_t(5), OM_PortType_t::OM_I2C,    {gpio_num_t::GPIO_NUM_12, gpio_num_t::GPIO_NUM_13}},
};

// File Manager
#define SUPPORT_SD
#define ENABLE_DEDICATED_SPI 1
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_4
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_5
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_6
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_7

#define DEFAULT_RELAY_GPIO      gpio_num_t::GPIO_NUM_15

// Special settings for the ESP32S3 Processors
#define DEFAULT_CONSOLE_TX_GPIO gpio_num_t::GPIO_NUM_43
#define DEFAULT_CONSOLE_RX_GPIO gpio_num_t::GPIO_NUM_44

// Output Types
// Not Finished - #define SUPPORT_OutputProtocol_TLS3001
// #define SUPPORT_OutputProtocol_APA102           // OM_SPI
#define SUPPORT_OutputProtocol_DMX              // OM_SERIAL
#define SUPPORT_OutputProtocol_GECE             // OM_SERIAL
#define SUPPORT_OutputProtocol_GS8208           // OM_SERIAL
#define SUPPORT_OutputProtocol_Renard           // OM_SERIAL
#define SUPPORT_OutputProtocol_Serial           // OM_SERIAL
#define SUPPORT_OutputProtocol_TM1814           // OM_SERIAL
#define SUPPORT_OutputProtocol_UCS1903          // OM_SERIAL
#define SUPPORT_OutputProtocol_UCS8903          // OM_SERIAL
// #define SUPPORT_OutputProtocol_WS2801           // OM_SPI
#define SUPPORT_OutputProtocol_WS2811           // OM_SERIAL
#define SUPPORT_OutputProtocol_Relay            // OM_RELAY
#define SUPPORT_OutputProtocol_Servo_PCA9685    // OM_I2C
#define SUPPORT_OutputProtocol_FireGod          // OM_SERIAL
// #define SUPPORT_OutputProtocol_GRINCH           // OM_SPI
