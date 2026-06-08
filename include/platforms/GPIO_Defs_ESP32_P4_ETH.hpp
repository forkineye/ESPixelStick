#pragma once
/*
 * GPIO_Defs_ESP32_P4_ETH.hpp - Output Management class
 *
 * Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
 * Copyright (c) 2021 - 2026 Shelby Merrick
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
 * ----------------------------------------------------------------------------
 * Waveshare ESP32-P4-ETH
 *   - ESP32-P4 (dual core RISC-V). No native WiFi radio -> Ethernet only.
 *   - 10/100 Ethernet via the internal EMAC and an IP101GR (IP101) RMII PHY.
 *   - RMII data plane pins are fixed by the ESP32-P4 IO_MUX:
 *       TXD0=34 TXD1=35 TX_EN=49 RXD0=30 RXD1=29 CRS_DV=28 REF_CLK=50
 *     The arduino-esp32 v3.x ETH driver configures those automatically; we
 *     only supply the routable control/clock signals below.
 *   - The clock is supplied externally (REF_CLK in on GPIO50), so the clock
 *     mode is EMAC_CLK_EXT_IN.
 *
 * Free header GPIOs used for outputs: 1,2,3,4,5,6,48 plus SPI on 20/26/22.
 * (GPIO0 is a strapping / user-button pin and is intentionally avoided.)
 * The onboard microSD slot (SDMMC on GPIO39-44) is left disabled for now,
 * mirroring the other Ethernet board definitions.
 */

// Output Manager
// The ESP32-P4 provides four RMT TX channels, so four serial (pixel) ports are
// defined. Each can alternatively be driven as a relay output. An SPI port is
// provided for clocked protocols (WS2801 / APA102).
const OM_OutputPortDefinition_t OM_OutputPortDefinitions[] =
{
    {OM_PortId_t(0), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_1}},
    {OM_PortId_t(0), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_1}},
    {OM_PortId_t(1), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_2}},
    {OM_PortId_t(1), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_2}},
    {OM_PortId_t(2), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_3}},
    {OM_PortId_t(2), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_3}},
    {OM_PortId_t(3), OM_PortType_t::OM_SERIAL, {gpio_num_t::GPIO_NUM_4}},
    {OM_PortId_t(3), OM_PortType_t::OM_RELAY,  {gpio_num_t::GPIO_NUM_4}},
    {OM_PortId_t(4), OM_PortType_t::OM_SPI,    {gpio_num_t::GPIO_NUM_20, gpio_num_t::GPIO_NUM_26, gpio_num_t::GPIO_NUM_22}},
};

// File Manager
// The onboard microSD slot uses the SDMMC bus and is not enabled by default.
// #define SUPPORT_SD_MMC
// #define SD_CARD_DATA_0          gpio_num_t::GPIO_NUM_39
// #define SD_CARD_DATA_1          gpio_num_t::GPIO_NUM_40
// #define SD_CARD_DATA_2          gpio_num_t::GPIO_NUM_41
// #define SD_CARD_DATA_3          gpio_num_t::GPIO_NUM_42

// SUPPORT_SD is left undefined so SD support is not built. FileMgr still
// references these pin names for its member initializers, so define inert
// placeholders (the SPI header pins) even though the SPI SD path is disabled.
#define SD_CARD_MISO_PIN        gpio_num_t::GPIO_NUM_21
#define SD_CARD_MOSI_PIN        gpio_num_t::GPIO_NUM_20
#define SD_CARD_CLK_PIN         gpio_num_t::GPIO_NUM_26
#define SD_CARD_CS_PIN          gpio_num_t::GPIO_NUM_22

// This board has no WiFi radio, so the firmware is built Ethernet-only.
// WIFI_NOT_SUPPORTED is supplied as a build flag (see platformio.ini) because
// it must be visible before this platform header is reached in the include
// graph (ESPixelStick.h consumes it to gate SUPPORT_WIFI).

#define SUPPORT_ETHERNET
#include <ETH.h>

// PHY address of the IP101 on the RMII bus
#define ETH_ADDR_PHY_IP101             1
#define DEFAULT_ETH_ADDR               ETH_ADDR_PHY_IP101

// Type of the Ethernet PHY. The IP101 family is selected via the TLK110 alias
// in the arduino-esp32 ETH driver (ETH_PHY_IP101 maps to ETH_PHY_TLK110).
#define DEFAULT_ETH_TYPE               eth_phy_type_t::ETH_PHY_TLK110

// Management (MDIO) bus - routable control plane signals.
#define DEFAULT_ETH_MDC_PIN            gpio_num_t::GPIO_NUM_31
#define DEFAULT_ETH_MDIO_PIN           gpio_num_t::GPIO_NUM_52

// PHY reset / power enable.
#define DEFAULT_ETH_POWER_PIN          gpio_num_t::GPIO_NUM_51
#define ETH_POWER_PIN                  DEFAULT_ETH_POWER_PIN
#define DEFAULT_ETH_POWER_PIN_ACTIVE   HIGH

// The 50MHz RMII reference clock is supplied externally (clock in).
#define DEFAULT_ETH_CLK_MODE           eth_clock_mode_t::EMAC_CLK_EXT_IN

// Output Types
#define SUPPORT_OutputProtocol_TLS3001          // OM_SERIAL
#define SUPPORT_OutputProtocol_APA102           // OM_SPI
#define SUPPORT_OutputProtocol_DMX              // OM_SERIAL
#define SUPPORT_OutputProtocol_GECE             // OM_SERIAL
#define SUPPORT_OutputProtocol_GS8208           // OM_SERIAL
#define SUPPORT_OutputProtocol_Renard           // OM_SERIAL
#define SUPPORT_OutputProtocol_Serial           // OM_SERIAL
#define SUPPORT_OutputProtocol_TM1814           // OM_SERIAL
#define SUPPORT_OutputProtocol_UCS1903          // OM_SERIAL
#define SUPPORT_OutputProtocol_UCS8903          // OM_SERIAL
#define SUPPORT_OutputProtocol_WS2801           // OM_SPI
#define SUPPORT_OutputProtocol_WS2811           // OM_SERIAL
#define SUPPORT_OutputProtocol_Relay            // OM_RELAY
#define SUPPORT_OutputProtocol_Servo_PCA9685    // OM_I2C
#define SUPPORT_OutputProtocol_FireGod          // OM_SERIAL
#define SUPPORT_OutputProtocol_GRINCH           // OM_SPI
