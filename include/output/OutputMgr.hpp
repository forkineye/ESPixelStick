#pragma once
/*
* OutputMgr.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2025 Shelby Merrick
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
*   This is a factory class used to manage the output port. It creates and deletes
*   the output channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "ESPixelStick.h"
#include "memdebug.h"
#include "FileMgr.hpp"
#include <TimeLib.h>

class c_OutputCommon; ///< forward declaration to the pure virtual output class that will be defined later.

#ifdef UART_LAST
#       define NUM_UARTS UART_LAST
#else
#       define NUM_UARTS 0
#endif

class c_OutputMgr
{
public:
    c_OutputMgr ();
    virtual ~c_OutputMgr ();

    void      Begin             ();                        ///< set up the operating environment based on the current config (or defaults)
    void      Poll              ();                        ///< Call from loop(),  renders output data
    void      ScheduleLoadConfig () {ConfigLoadNeeded = now();}
    void      LoadConfig        ();                        ///< Read the current configuration data from nvram
    void      GetConfig         (byte * Response, uint32_t maxlen);
    void      GetConfig         (String & Response);
    void      SetConfig         (const char * NewConfig);  ///< Save the current configuration data to nvram
    void      SetConfig         (ArduinoJson::JsonDocument & NewConfig);  ///< Save the current configuration data to nvram
    void      GetStatus         (JsonObject & jsonStatus);
    void      GetPortCounts     (uint16_t& PixelCount, uint16_t& SerialCount) {PixelCount = uint16_t(OutputChannelId_End); SerialCount = uint16_t(NUM_UARTS); }
    uint8_t*  GetBufferAddress  () { return OutputBuffer; } ///< Get the address of the buffer into which the E1.31 handler will stuff data
    uint32_t  GetBufferUsedSize () { return UsedBufferSize; } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    uint32_t  GetBufferSize     () { return sizeof(OutputBuffer); } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    void      DeleteConfig      () { FileMgr.DeleteFlashFile (ConfigFileName); }
    void      PauseOutputs      (bool NewState);
    void      GetDriverName     (String & Name) { Name = "OutputMgr"; }
    void      WriteChannelData  (uint32_t StartChannelId, uint32_t ChannelCount, uint8_t * pData);
    void      ReadChannelData   (uint32_t StartChannelId, uint32_t ChannelCount, uint8_t *pTargetData);
    void      ClearBuffer       ();
    void      TaskPoll          ();

    // handles to determine which output channel we are dealing with
    enum e_OutputChannelIds
    {
        #ifdef DEFAULT_UART_0_GPIO
        OutputChannelId_UART_0,
        #endif // def DEFAULT_UART_0_GPIO

        #ifdef DEFAULT_UART_1_GPIO
        OutputChannelId_UART_1,
        #endif // def DEFAULT_UART_1_GPIO

        #ifdef DEFAULT_UART_2_GPIO
        OutputChannelId_UART_2,
        #endif // def DEFAULT_UART_2_GPIO

        #ifdef DEFAULT_RMT_0_GPIO
        OutputChannelId_RMT_0,
        #endif // def DEFAULT_RMT_0_GPIO

        #ifdef DEFAULT_RMT_1_GPIO
        OutputChannelId_RMT_1,
        #endif // def DEFAULT_RMT_1_GPIO

        #ifdef DEFAULT_RMT_2_GPIO
        OutputChannelId_RMT_2,
        #endif // def DEFAULT_RMT_2_GPIO

        #ifdef DEFAULT_RMT_3_GPIO
        OutputChannelId_RMT_3,
        #endif // def DEFAULT_RMT_3_GPIO

        #ifdef DEFAULT_RMT_4_GPIO
        OutputChannelId_RMT_4,
        #endif // def DEFAULT_RMT_3_GPIO

        #ifdef DEFAULT_RMT_5_GPIO
        OutputChannelId_RMT_5,
        #endif // def DEFAULT_RMT_3_GPIO

        #ifdef DEFAULT_RMT_6_GPIO
        OutputChannelId_RMT_6,
        #endif // def DEFAULT_RMT_3_GPIO

        #ifdef DEFAULT_RMT_7_GPIO
        OutputChannelId_RMT_7,
        #endif // def DEFAULT_RMT_3_GPIO

        #ifdef SUPPORT_SPI_OUTPUT
        OutputChannelId_SPI_1,
        #endif // def SUPPORT_SPI_OUTPUT

        #if defined(SUPPORT_OutputType_Relay) || defined(SUPPORT_OutputType_Servo_PCA9685)
        OutputChannelId_Relay,
        #endif // def SUPPORT_RELAY_OUTPUT

        OutputChannelId_End, // must be last in the list
        OutputChannelId_Start = 0,
    };

    // do NOT insert into the middle of this list. Always add new types to the end of the list
    enum e_OutputType
    {
        OutputType_Disabled = 0,

        #ifdef SUPPORT_OutputType_WS2811
        OutputType_WS2811 = 1,
        #endif // def SUPPORT_OutputType_WS2811

        #ifdef SUPPORT_OutputType_GECE
        OutputType_GECE = 2,
        #endif // def SUPPORT_OutputType_GECE

        #ifdef SUPPORT_OutputType_DMX
        OutputType_DMX = 3,
        #endif // def SUPPORT_OutputType_DMX

        #ifdef SUPPORT_OutputType_Renard
        OutputType_Renard = 4,
        #endif // def SUPPORT_OutputType_Renard

        #ifdef SUPPORT_OutputType_Serial
        OutputType_Serial = 5,
        #endif // def SUPPORT_OutputType_Serial

        #ifdef SUPPORT_OutputType_Relay
        OutputType_Relay = 6,
        #endif // def SUPPORT_OutputType_Relay

        #ifdef SUPPORT_OutputType_Servo_PCA9685
        OutputType_Servo_PCA9685 = 7,
        #endif // def SUPPORT_OutputType_Servo_PCA9685

        #ifdef SUPPORT_OutputType_UCS1903
        OutputType_UCS1903 = 8,
        #endif // def SUPPORT_OutputType_UCS1903

        #ifdef SUPPORT_OutputType_TM1814
        OutputType_TM1814 = 9,
        #endif // def SUPPORT_OutputType_TM1814

        #ifdef SUPPORT_OutputType_WS2801
        OutputType_WS2801 = 10,
        #endif // def SUPPORT_OutputType_WS2801

        #ifdef SUPPORT_OutputType_APA102
        OutputType_APA102 = 11,
        #endif // def SUPPORT_OutputType_APA102

        #ifdef SUPPORT_OutputType_GS8208
        OutputType_GS8208 = 12,
        #endif // def SUPPORT_OutputType_GS8208

        #ifdef SUPPORT_OutputType_UCS8903
        OutputType_UCS8903 = 13,
        #endif // def SUPPORT_OutputType_UCS8903

        #ifdef SUPPORT_OutputType_TLS3001
        OutputType_TLS3001 = 14,
        #endif // def SUPPORT_OutputType_TLS3001

        #ifdef SUPPORT_OutputType_GRINCH
        OutputType_GRINCH = 15,
        #endif // def SUPPORT_OutputType_GRINCH

        // Add new types here
        OutputType_End, // must be last
        OutputType_Start = OutputType_Disabled,
    };

#ifdef ARDUINO_ARCH_ESP8266
#   define OM_MAX_NUM_CHANNELS  (1200 * 3)
#else // ARDUINO_ARCH_ESP32
#   define OM_MAX_NUM_CHANNELS  (3000 * 3)
#endif // !def ARDUINO_ARCH_ESP32

    enum OM_PortType_t
    {
        Uart = 0,
        Rmt,
        Spi,
        Relay,
        Undefined
    };

private:
    struct DriverInfo_t
    {
        uint32_t            OutputBufferStartingOffset  = 0;
        uint32_t            OutputBufferDataSize        = 0;
        uint32_t            OutputBufferEndOffset       = 0;

        uint32_t            OutputChannelStartingOffset = 0;
        uint32_t            OutputChannelSize           = 0;
        uint32_t            OutputChannelEndOffset      = 0;

        gpio_num_t          GpioPin                     = gpio_num_t(-1);
        OM_PortType_t       PortType                    = OM_PortType_t::Undefined;
        uart_port_t         PortId                      = uart_port_t(-1);
        e_OutputChannelIds  DriverId                    = e_OutputChannelIds(-1);
        c_OutputCommon      *pOutputChannelDriver       = nullptr;
    };

    // pointer(s) to the current active output drivers
    DriverInfo_t OutputChannelDrivers[OutputChannelId_End];

    // configuration parameter names for the channel manager within the config file
    #define NO_CONFIG_NEEDED time_t(-1)
    bool HasBeenInitialized = false;
    time_t ConfigLoadNeeded = NO_CONFIG_NEEDED;
    bool ConfigInProgress   = false;
    bool OutputIsPaused     = false;
    bool BuildingNewConfig  = false;

    bool ProcessJsonConfig (JsonDocument & jsonConfig);
    void CreateJsonConfig  (JsonObject & jsonConfig);
    void UpdateDisplayBufferReferences (void);
    void InstantiateNewOutputChannel(DriverInfo_t &ChannelIndex, e_OutputType NewChannelType, bool StartDriver = true);
    void CreateNewConfig();
    void SetSerialUart();
    bool FindJsonChannelConfig (JsonDocument& jsonConfig, e_OutputChannelIds ChanId, e_OutputType Type, JsonObject& ChanConfig);

    String ConfigFileName;

    uint8_t    OutputBuffer[OM_MAX_NUM_CHANNELS];
    uint32_t   UsedBufferSize = 0;
    gpio_num_t ConsoleTxGpio  = gpio_num_t::GPIO_NUM_1;
    gpio_num_t ConsoleRxGpio  = gpio_num_t::GPIO_NUM_3;
#if defined(ARDUINO_ARCH_ESP32)
    TaskHandle_t myTaskHandle = NULL;
    // uint32_t PollCount = 0;
#endif // defined(ARDUINO_ARCH_ESP32)

#define OM_IS_UART (CurrentOutputChannelDriver.PortType == OM_PortType_t::Uart)
#define OM_IS_RMT  (CurrentOutputChannelDriver.PortType == OM_PortType_t::Rmt)

}; // c_OutputMgr

extern c_OutputMgr OutputMgr;
