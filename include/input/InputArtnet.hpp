#pragma once
/*
* ArtnetInput.h - Code to wrap ESPAsyncArtnet for input
*
* Project: ESPixelStick - An ESP8266 / ESP32 and Artnet based pixel driver
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
*/

#include "InputCommon.hpp"
#include <Artnet.h>

class c_InputArtnet : public c_InputCommon
{
  private:
    static const uint16_t   UNIVERSE_MAX = 512;
    static const char       ConfigFileName[];
    static const uint8_t    MAX_NUM_UNIVERSES = 10;

    Artnet * pArtnet = nullptr;

    /// JSON configuration parameters
    uint16_t    startUniverse              = 1;    ///< Universe to listen for
    uint16_t    LastUniverse               = 1;    ///< Last Universe to listen for
    uint16_t    ChannelsPerUniverse        = 512;  ///< Universe boundary limit
    uint16_t    FirstUniverseChannelOffset = 1;    ///< Channel to start listening at - 1 based
    uint32_t    num_packets                = 0;
    uint32_t    packet_errors              = 0;
    uint32_t    PollCounter                = 0;

    uint8_t     lastData = 255;

    /// from sketch globals
    uint16_t    channel_count = 0;       ///< Number of channels. Derived from output module configuration.

    struct Universe_t
    {
        uint32_t    DestinationOffset;
        uint32_t    BytesToCopy;
        uint32_t    SourceDataOffset;
        uint32_t    SequenceErrorCounter;
        uint8_t     SequenceNumber;
        uint32_t    num_packets;

    };
    Universe_t UniverseArray[MAX_NUM_UNIVERSES];

    void SetUpArtnet ();
    void validateConfiguration ();
    void NetworkStateChanged (bool IsConnected, bool RebootAllowed); // used by poorly designed rx functions
    void SetBufferTranslation ();
    void onDmxFrame (uint16_t CurrentUniverseId, uint32_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP);
    void onDmxPoll (IPAddress  broadcastIP);

  public:

    c_InputArtnet (c_InputMgr::e_InputChannelIds NewInputChannelId,
                   c_InputMgr::e_InputType       NewChannelType,
                   uint32_t                        BufferSize);
    virtual ~c_InputArtnet();

    // functions to be provided by the derived class
    void Begin ();                                          ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (JsonObject & jsonConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject & jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject & jsonStatus);
    void GetDriverName (String & sDriverName) { sDriverName = "Artnet"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint32_t BufferSize);
    void NetworkStateChanged (bool IsConnected); // used by poorly designed rx functions
    bool isShutDownRebootNeeded () { return HasBeenInitialized; }
    virtual void Process () {}                                       ///< Call from loop(),  renders Input data

};
