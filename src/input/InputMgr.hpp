#pragma once
/*
* InputMgr.hpp - Input Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2020 Shelby Merrick
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
*   This is a factory class used to manage the input channels. It creates and deletes
*   the input channel functionality as needed to support any new configurations 
*   that get sent from from the WebPage.
*
*/

#include "../ESPixelStick.h"
#include "../output/OutputMgr.hpp"

class c_InputCommon; ///< forward declaration to the pure virtual Input class that will be defined later.

class c_InputMgr
{
public:
    c_InputMgr ();
    virtual ~c_InputMgr ();

    void Begin      (uint8_t * BufferStart, uint16_t BufferSize); ///< set up the operating environment based on the current config (or defaults)
    void LoadConfig ();                        ///< Read the current configuration data from nvram
    void SaveConfig ();                        ///< Save the current configuration data to nvram
    void GetConfig  (char * Response);         ///< Get the current config used by the driver
    void GetStatus  (JsonObject & jsonStatus);
    bool SetConfig  (JsonObject & jsonConfig); ///< Set a new config in the driver
    void Process    ();                        ///< Call from loop(),  renders Input data
    void GetOptions (JsonObject & jsonOptions);
    void SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize);
    void SetOperationalState (bool Active);
    void ResetBlankTimer ();

    enum e_InputType
    {
        InputType_Disabled = 0,
        InputType_E1_31,
        InputType_Effects,
        InputType_MQTT,
        InputType_Alexa,
        InputType_DDP,
        InputType_FPP,
        InputType_End,
        InputType_Start   = InputType_Disabled,
        InputType_Default = InputType_Disabled,
    };

    // handles to determine which input channel we are dealing with
    enum e_InputChannelIds
    {
        InputChannelId_1 = 0,
        InputChannelId_2 = 1,
        InputChannelId_End,
        InputChannelId_Start = InputChannelId_1,
        InputChannelId_ALL = InputChannelId_End
    };

private:

    void InstantiateNewInputChannel (e_InputChannelIds InputChannelId, e_InputType NewChannelType);
    void CreateNewConfig ();

    c_InputCommon * pInputChannelDrivers[InputChannelId_End]; ///< pointer(s) to the current active Input driver
    uint8_t       * InputDataBuffer     = nullptr;
    uint16_t        InputDataBufferSize = 0;
    bool            HasBeenInitialized  = false;
    bool            ConfigSaveNeeded    = false;

    // configuration parameter names for the channel manager within the config file
#   define IM_SECTION_NAME         F("input_config")
#   define IM_CHANNEL_SECTION_NAME F("channels")
#   define IM_CHANNEL_TYPE_NAME    F("type")

    bool ProcessJsonConfig           (JsonObject & jsonConfig);
    void CreateJsonConfig            (JsonObject & jsonConfig);
    bool ProcessJsonChannelConfig    (JsonObject & jsonConfig, uint32_t ChannelIndex);
    bool InputTypeIsAllowedOnChannel (e_InputType type, e_InputChannelIds ChannelId);

    String ConfigFileName;
    String ConfigData;

#define IM_JSON_SIZE (5*1024)

protected:

}; // c_InputMgr

extern c_InputMgr InputMgr;
