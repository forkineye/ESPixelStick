#pragma once
/*
* E131Input.h - Code to wrap ESPAsyncE131 for input
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
#include <ESPAsyncE131.h>

class c_InputE131 : public c_InputCommon 
{
  private:
    static const uint16_t   UNIVERSE_MAX = 512;
    static const char       ConfigFileName[];

    ESPAsyncE131  * e131 = nullptr; ///< ESPAsyncE131
    e131_packet_t packet;           ///< Packet buffer for parsing

    /// JSON configuration parameters
    uint16_t    startUniverse          = 1;    ///< Universe to listen for
    uint16_t    universe_channel_limit = 512;  ///< Universe boundary limit
    uint16_t    channel_start          = 1;    ///< Channel to start listening at - 1 based

    /// from sketch globals
    uint16_t    channel_count = 0;    ///< Number of channels. Derived from output module configuration.
    uint16_t    LastUniverse  = 1;    ///< Last Universe to listen for
    uint8_t   * seqTracker = nullptr; ///< Current sequence numbers for each Universe
    uint32_t  * seqError = nullptr;   ///< Sequence error tracking for each universe

    void SubscribeToMulticastDomains();
    void validateConfiguration ();
    void WiFiStateChanged (bool IsConnected, bool RebootAllowed); // used by poorly designed rx functions

  public:

    c_InputE131 (c_InputMgr::e_InputChannelIds NewInputChannelId,
                 c_InputMgr::e_InputType       NewChannelType,
                 uint8_t                     * BufferStart,
                 uint16_t                      BufferSize);
    ~c_InputE131();

    // functions to be provided by the derived class
    void Begin ();                                          ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (JsonObject & jsonConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject & jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject & jsonStatus);
    void Process ();                                        ///< Call from loop(),  renders Input data
    void GetDriverName (String & sDriverName) { sDriverName = "E1.31"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint8_t * BufferStart, uint16_t BufferSize);
    void WiFiStateChanged (bool IsConnected); // used by poorly designed rx functions
    bool isShutDownRebootNeeded () { return true; }

};
