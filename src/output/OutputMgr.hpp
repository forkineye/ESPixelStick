#pragma once
/*
* OutputMgr.hpp - Output Management class
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
*   This is a factory class used to manage the output port. It creates and deletes
*   the output channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "../ESPixelStick.h"

#include "../memdebug.h"
#include "../FileMgr.hpp"

class c_OutputCommon; ///< forward declaration to the pure virtual output class that will be defined later.

class c_OutputMgr
{
public:
    c_OutputMgr ();
    virtual ~c_OutputMgr ();

    void      Begin             ();                        ///< set up the operating environment based on the current config (or defaults)
    void      Render            ();                        ///< Call from loop(),  renders output data
    void      LoadConfig        ();                        ///< Read the current configuration data from nvram
    void      GetConfig         (char * Response);
    void      GetConfig         (String & Response);
    void      SetConfig         (const char * NewConfig);  ///< Save the current configuration data to nvram
    void      GetStatus         (JsonObject & jsonStatus);
    void      PauseOutput       (bool PauseTheOutput) { IsOutputPaused = PauseTheOutput; }
    void      GetPortCounts     (uint16_t& PixelCount, uint16_t& SerialCount) {PixelCount = uint16_t(OutputChannelId_End); SerialCount = min(uint16_t(OutputChannelId_End), uint16_t(2)); }
    uint8_t*  GetBufferAddress  () { return OutputBuffer; } ///< Get the address of the buffer into which the E1.31 handler will stuff data
    uint16_t  GetBufferUsedSize () { return UsedBufferSize; } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    uint16_t  GetBufferSize     () { return sizeof(OutputBuffer); } ///< Get the size (in intensities) of the buffer into which the E1.31 handler will stuff data
    void      DeleteConfig      () { FileMgr.DeleteConfigFile (ConfigFileName); }
    void      PauseOutputs      ();

    // handles to determine which output channel we are dealing with
    enum e_OutputChannelIds
    {
        OutputChannelId_1 = 0,
#ifdef ARDUINO_ARCH_ESP32
        OutputChannelId_2,
#endif // def ARDUINO_ARCH_ESP32
        OutputChannelId_Relay,
        OutputChannelId_End, // must be last in the list
        OutputChannelId_Start = OutputChannelId_1
    };

    enum e_OutputType
    {
        OutputType_WS2811 = 0,
        OutputType_GECE,
        OutputType_DMX,
        OutputType_Renard,
        OutputType_Serial,
        OutputType_Relay,
        OutputType_Servo_PCA9685,
        OutputType_Disabled,
        OutputType_End, // must be last
        OutputType_Start = OutputType_WS2811,
    };

#ifdef ARDUINO_ARCH_ESP8266
#   define OM_MAX_NUM_CHANNELS  (800 * 3)
#else
#   define OM_MAX_NUM_CHANNELS  (2000 * 3)
#endif // !def ARDUINO_ARCH_ESP8266

private:

    void InstantiateNewOutputChannel (e_OutputChannelIds ChannelIndex, e_OutputType NewChannelType);
    void CreateNewConfig ();

    c_OutputCommon * pOutputChannelDrivers[e_OutputChannelIds::OutputChannelId_End]; ///< pointer(s) to the current active output driver

    // configuration parameter names for the channel manager within the config file

#ifdef ARDUINO_ARCH_ESP8266
#   define OM_MAX_CONFIG_SIZE      ((size_t)(5*1024))
#else
#   define OM_MAX_CONFIG_SIZE      ((size_t)(6*1024))
#endif // !def ARDUINO_ARCH_ESP8266

    bool HasBeenInitialized = false;
    bool ConfigLoadNeeded   = false;
    bool IsOutputPaused     = false;

    bool ProcessJsonConfig (JsonObject & jsonConfig);
    void CreateJsonConfig  (JsonObject & jsonConfig);
    void UpdateDisplayBufferReferences (void);

    String ConfigFileName;

    uint8_t OutputBuffer[OM_MAX_NUM_CHANNELS];
    uint16_t UsedBufferSize = 0;

}; // c_OutputMgr

extern c_OutputMgr OutputMgr;
