#pragma once
/*
* OutputMgr.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2026 Shelby Merrick
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
#include "OutputMgrPortDefs.hpp"
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
private:
    #ifdef ARDUINO_ARCH_ESP8266
    #define OM_MAX_NUM_CHANNELS  (1200 * 3)
    #else // ARDUINO_ARCH_ESP32
    #define OM_MAX_NUM_CHANNELS  (3000 * 3)
    #endif // !def ARDUINO_ARCH_ESP32

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
//    void      GetPortCounts     (uint16_t& PixelCount, uint16_t& SerialCount) {PixelCount = uint16_t(OutputPortId_End); SerialCount = uint16_t(NUM_UARTS); }
    uint8_t*  GetBufferAddress  () { return pOutputBuffer; } ///< Get the address of the buffer into which the E1.31 handler will stuff data
    uint32_t  GetBufferUsedSize () { return UsedBufferSize; } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    uint32_t  GetBufferSize     () { return uint32_t(OM_MAX_NUM_CHANNELS); } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    void      DeleteConfig      () { FileMgr.DeleteFlashFile (ConfigFileName); }
    void      PauseOutputs      (bool NewState);
    void      GetDriverName     (String & Name) { Name = "OutputMgr"; }
    void      WriteChannelData  (uint32_t StartChannelId, uint32_t ChannelCount, uint8_t * pData);
    void      ReadChannelData   (uint32_t StartChannelId, uint32_t ChannelCount, uint8_t *pTargetData);
    void      ClearBuffer       ();
    void      TaskPoll          ();
    void      RelayUpdate       (uint8_t RelayId, String & NewValue, String & Response);
    void      ClearStatistics   (void);
    uint8_t   GetNumPorts       () {return NumOutputPorts;}

    // do NOT insert into the middle of this list. Always add new types to the end of the list
    enum e_OutputProtocolType
    {
        OutputProtocol_Disabled = 0,

        #ifdef SUPPORT_OutputProtocol_WS2811
        OutputProtocol_WS2811 = 1,
        #endif // def SUPPORT_OutputProtocol_WS2811

        #ifdef SUPPORT_OutputProtocol_GECE
        OutputProtocol_GECE = 2,
        #endif // def SUPPORT_OutputProtocol_GECE

        #ifdef SUPPORT_OutputProtocol_DMX
        OutputProtocol_DMX = 3,
        #endif // def SUPPORT_OutputProtocol_DMX

        #ifdef SUPPORT_OutputProtocol_Renard
        OutputProtocol_Renard = 4,
        #endif // def SUPPORT_OutputProtocol_Renard

        #ifdef SUPPORT_OutputProtocol_Serial
        OutputProtocol_Serial = 5,
        #endif // def SUPPORT_OutputProtocol_Serial

        #ifdef SUPPORT_OutputProtocol_Relay
        OutputProtocol_Relay = 6,
        #endif // def SUPPORT_OutputProtocol_Relay

        #ifdef SUPPORT_OutputProtocol_Servo_PCA9685
        OutputProtocol_Servo_PCA9685 = 7,
        #endif // def SUPPORT_OutputProtocol_Servo_PCA9685

        #ifdef SUPPORT_OutputProtocol_UCS1903
        OutputProtocol_UCS1903 = 8,
        #endif // def SUPPORT_OutputProtocol_UCS1903

        #ifdef SUPPORT_OutputProtocol_TM1814
        OutputProtocol_TM1814 = 9,
        #endif // def SUPPORT_OutputProtocol_TM1814

        #ifdef SUPPORT_OutputProtocol_WS2801
        OutputProtocol_WS2801 = 10,
        #endif // def SUPPORT_OutputProtocol_WS2801

        #ifdef SUPPORT_OutputProtocol_APA102
        OutputProtocol_APA102 = 11,
        #endif // def SUPPORT_OutputProtocol_APA102

        #ifdef SUPPORT_OutputProtocol_GS8208
        OutputProtocol_GS8208 = 12,
        #endif // def SUPPORT_OutputProtocol_GS8208

        #ifdef SUPPORT_OutputProtocol_UCS8903
        OutputProtocol_UCS8903 = 13,
        #endif // def SUPPORT_OutputProtocol_UCS8903

        #ifdef SUPPORT_OutputProtocol_TLS3001
        OutputProtocol_TLS3001 = 14,
        #endif // def SUPPORT_OutputProtocol_TLS3001

        #ifdef SUPPORT_OutputProtocol_GRINCH
        OutputProtocol_GRINCH = 15,
        #endif // def SUPPORT_OutputProtocol_GRINCH

        #ifdef SUPPORT_OutputProtocol_FireGod
        OutputProtocol_FireGod = 16,
        #endif // def SUPPORT_OutputProtocol_FireGod

        // Add new types here

        // Must be last
        OutputProtocol_Last,
    };
    uint32_t NumberOfOutputProtocols = uint32_t(0);

    // must be 16 byte aligned. Determined by upshifting the max size of all drivers
    // #define OutputDriverMemorySize 1200
    #define OutputDriverMemorySize 1400
    uint32_t GetDriverSize() {return OutputDriverMemorySize;}
private:
    struct alignas(16) DriverInfo_t
    {
        byte                OutputDriver[OutputDriverMemorySize];
        uint32_t            OutputBufferStartingOffset  = 0;
        uint32_t            OutputBufferDataSize        = 0;
        uint32_t            OutputBufferEndOffset       = 0;

        uint32_t            OutputChannelStartingOffset = 0;
        uint32_t            OutputChannelSize           = 0;
        uint32_t            OutputChannelEndOffset      = 0;

        OM_OutputPortDefinition_t PortDefinition;
        uint8_t             DriverId                    = -1;
        bool                OutputDriverInUse           = false;
    };

    // pointer(s) to the current active output drivers
    DriverInfo_t   *pOutputChannelDrivers = nullptr;
    uint8_t         NumOutputPorts = 0;
    uint32_t        SizeOfTable = 0;

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
    void InstantiateNewOutputChannel(DriverInfo_t &ChannelIndex, e_OutputProtocolType NewChannelType, bool StartDriver = true);
    void CreateNewConfig();
    void SetSerialUart();
    bool FindJsonChannelConfig (JsonDocument& jsonConfig, OM_PortId_t PortId, JsonObject& ChanConfig);
    bool SetPortDefnitionDefaults(DriverInfo_t & CurrentOutput, e_OutputProtocolType TargetProtocolType);

    String ConfigFileName;

    uint8_t    *pOutputBuffer = nullptr;
    uint32_t   UsedBufferSize = 0;

    #ifndef DEFAULT_CONSOLE_TX_GPIO
    #define DEFAULT_CONSOLE_TX_GPIO gpio_num_t::GPIO_NUM_1
    #define DEFAULT_CONSOLE_RX_GPIO gpio_num_t::GPIO_NUM_3
    #endif // ndef DEFAULT_CONSOLE_TX_GPIO

    gpio_num_t ConsoleTxGpio  = DEFAULT_CONSOLE_TX_GPIO;
    gpio_num_t ConsoleRxGpio  = DEFAULT_CONSOLE_RX_GPIO;

#if defined(ARDUINO_ARCH_ESP32)
    TaskHandle_t myTaskHandle = NULL;
    // uint32_t PollCount = 0;
#endif // defined(ARDUINO_ARCH_ESP32)

}; // c_OutputMgr

extern c_OutputMgr OutputMgr;
