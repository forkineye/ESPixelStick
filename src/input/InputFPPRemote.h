#pragma once
/*
* InputFPPRemote.h
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

#include "InputCommon.hpp"
#include "../WebMgr.hpp"
#include "../service/FPPDiscovery.h"

class c_InputFPPRemote : public c_InputCommon
{
  public:

      c_InputFPPRemote (
          c_InputMgr::e_InputChannelIds NewInputChannelId,
          c_InputMgr::e_InputType       NewChannelType,
          uint8_t                     * BufferStart,
          uint16_t                      BufferSize);
      
      ~c_InputFPPRemote ();

      // functions to be provided by the derived class
      void Begin ();                           ///< set up the operating environment based on the current config (or defaults)
      bool SetConfig (JsonObject& jsonConfig); ///< Set a new config in the driver
      void GetConfig (JsonObject& jsonConfig); ///< Get the current config used by the driver
      void GetStatus (JsonObject& jsonStatus);
      void Process ();                         ///< Call from loop(),  renders Input data
      void GetDriverName (String& sDriverName) { sDriverName = "FPP Remote"; } ///< get the name for the instantiated driver
      void SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize);

private:

    void validateConfiguration ();
    // void onMessage (EspFPPRemoteDevice* pDevice);

    void load ();          ///< Load configuration from File System
    void save ();          ///< Save configuration to File System

#   define JSON_NAME_MISO         F("miso_pin")
#   define JSON_NAME_MOSI         F("mosi_pin")
#   define JSON_NAME_CLOCK        F("clock_pin")
#   define JSON_NAME_CS           F("cs_pin")
#   define JSON_NAME_FILE_TO_PLAY F("fseqfilename")

    uint8_t  miso_pin = SD_CARD_MISO_PIN;
    uint8_t  mosi_pin = SD_CARD_MOSI_PIN;
    uint8_t  clk_pin  = SD_CARD_CLK_PIN;
    uint8_t  cs_pin   = SD_CARD_CS_PIN;

    String  FseqFileToPlay;

};
