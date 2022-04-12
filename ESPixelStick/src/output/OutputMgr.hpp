#pragma once
/*
* OutputMgr.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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

#include "../ESPixelStick.h"

#include "../memdebug.h"
#include "../FileMgr.hpp"

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
    void      Render            ();                        ///< Call from loop(),  renders output data
    void      LoadConfig        ();                        ///< Read the current configuration data from nvram
    void      GetConfig         (byte * Response, size_t maxlen);
    void      GetConfig         (String & Response);
    void      SetConfig         (const char * NewConfig);  ///< Save the current configuration data to nvram
    void      SetConfig         (ArduinoJson::JsonDocument & NewConfig);  ///< Save the current configuration data to nvram
    void      GetStatus         (JsonObject & jsonStatus);
    void      PauseOutput       (bool PauseTheOutput) { IsOutputPaused = PauseTheOutput; }
    void      GetPortCounts     (uint16_t& PixelCount, uint16_t& SerialCount) {PixelCount = uint16_t(OutputChannelId_End); SerialCount = uint16_t(NUM_UARTS); }
    uint8_t*  GetBufferAddress  () { return OutputBuffer; } ///< Get the address of the buffer into which the E1.31 handler will stuff data
    size_t    GetBufferUsedSize () { return UsedBufferSize; } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    size_t    GetBufferSize     () { return sizeof(OutputBuffer); } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    void      DeleteConfig      () { FileMgr.DeleteConfigFile (ConfigFileName); }
    void      PauseOutputs      ();
    void      GetDriverName     (String & Name) { Name = "OutputMgr"; }
    void      WriteChannelData  (size_t StartChannelId, size_t ChannelCount, byte * pData);
    void      ReadChannelData   (size_t StartChannelId, size_t ChannelCount, byte *pTargetData);
    void      ClearBuffer       ();

    // handles to determine which output channel we are dealing with
    enum e_OutputChannelIds
    {
#ifdef DEFAULT_UART_1_GPIO
        OutputChannelId_UART_1,
#endif // def DEFAULT_UART_1_GPIO
#ifdef DEFAULT_UART_2_GPIO
        OutputChannelId_UART_2,
#endif // def DEFAULT_UART_2_GPIO
#ifdef DEFAULT_RMT_0_GPIO
        OutputChannelId_RMT_1,
#endif // def DEFAULT_RMT_0_GPIO
#ifdef DEFAULT_RMT_1_GPIO
        OutputChannelId_RMT_2,
#endif // def DEFAULT_RMT_1_GPIO
#ifdef DEFAULT_RMT_2_GPIO
        OutputChannelId_RMT_3,
#endif // def DEFAULT_RMT_2_GPIO
#ifdef DEFAULT_RMT_3_GPIO
        OutputChannelId_RMT_4,
#endif // def DEFAULT_RMT_3_GPIO
#ifdef DEFAULT_RMT_4_GPIO
        OutputChannelId_RMT_5,
#endif // def DEFAULT_RMT_3_GPIO
#ifdef DEFAULT_RMT_5_GPIO
        OutputChannelId_RMT_6,
#endif // def DEFAULT_RMT_3_GPIO
#ifdef DEFAULT_RMT_6_GPIO
        OutputChannelId_RMT_7,
#endif // def DEFAULT_RMT_3_GPIO
#ifdef DEFAULT_RMT_7_GPIO
        OutputChannelId_RMT_8,
#endif // def DEFAULT_RMT_3_GPIO
#ifdef SUPPORT_SPI_OUTPUT
        OutputChannelId_SPI_1,
#endif // def SUPPORT_SPI_OUTPUT
#if defined(SUPPORT_OutputType_Relay) || defined(SUPPORT_OutputType_Servo_PCA9685)
        OutputChannelId_Relay,
#endif // def SUPPORT_RELAY_OUTPUT

        OutputChannelId_End, // must be last in the list
        OutputChannelId_Start = 0,

#ifdef SUPPORT_UART_OUTPUT
        OutputChannelId_UART_FIRST = OutputChannelId_UART_1,
        OutputChannelId_UART_LAST = UART_LAST,
#endif // def SUPPORT_UART_OUTPUT

#ifdef SUPPORT_RMT_OUTPUT
        OutputChannelId_RMT_FIRST = OutputChannelId_RMT_1,
        OutputChannelId_RMT_LAST = RMT_LAST,
#endif // def SUPPORT_RMT_OUTPUT
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
            // Add new types here
            OutputType_End, // must be last
            OutputType_Start = OutputType_Disabled,
    };

#ifdef ARDUINO_ARCH_ESP8266
#   define OM_MAX_NUM_CHANNELS      (1200 * 3)
#   define OM_MAX_CONFIG_SIZE       ((size_t)(5 * 1024))
#else // ARDUINO_ARCH_ESP32
#   ifdef BOARD_HAS_PSRAM
#       define OM_MAX_NUM_CHANNELS  (7000 * 3)
#       define OM_MAX_CONFIG_SIZE   ((size_t)(20 * 1024))
#   else
#       define OM_MAX_NUM_CHANNELS  (3000 * 3)
#       define OM_MAX_CONFIG_SIZE   ((size_t)(11 * 1024))
#   endif // !def BOARD_HAS_PSRAM
#endif // !def ARDUINO_ARCH_ESP32

private:
        // pointer(s) to the current active output drivers
        struct DriverInfo_t
        {
                e_OutputChannelIds DriverId = OutputChannelId_Start;
                c_OutputCommon *pOutputChannelDriver = nullptr;
                size_t StartingChannelId = 0;
                size_t ChannelCount = 0;
                size_t EndChannelId = 0;
    };

    DriverInfo_t OutputChannelDrivers[OutputChannelId_End];

    // configuration parameter names for the channel manager within the config file

    bool HasBeenInitialized = false;
    bool ConfigLoadNeeded   = false;
    bool IsOutputPaused     = false;
    bool BuildingNewConfig  = false;

    bool ProcessJsonConfig (JsonObject & jsonConfig);
    void CreateJsonConfig  (JsonObject & jsonConfig);
    void UpdateDisplayBufferReferences (void);
    void InstantiateNewOutputChannel(DriverInfo_t &ChannelIndex, e_OutputType NewChannelType, bool StartDriver = true);
    void CreateNewConfig();

    String ConfigFileName;

    uint8_t OutputBuffer[OM_MAX_NUM_CHANNELS];
    size_t  UsedBufferSize = 0;

#ifdef SUPPORT_UART_OUTPUT
#       define OM_IS_UART ((CurrentOutputChannelDriver.DriverId >= OutputChannelId_UART_FIRST) && (CurrentOutputChannelDriver.DriverId <= OutputChannelId_UART_LAST))
#else
#   define OM_IS_UART false
#endif // def SUPPORT_UART_OUTPUT
#ifdef SUPPORT_RMT_OUTPUT
#       define OM_IS_RMT ((CurrentOutputChannelDriver.DriverId >= OutputChannelId_RMT_FIRST) && (CurrentOutputChannelDriver.DriverId <= OutputChannelId_RMT_LAST))
#else
#   define OM_IS_RMT false
#endif // def SUPPORT_RMT_OUTPUT

}; // c_OutputMgr

extern c_OutputMgr OutputMgr;
