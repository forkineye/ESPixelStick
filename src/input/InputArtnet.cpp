/*
* ArtnetInput.cpp - Code to wrap ESPAsyncArtnet for input
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

#include "input/InputArtnet.hpp"
#include "input/externalInput.h"
#include "network/NetworkMgr.hpp"

//-----------------------------------------------------------------------------
c_InputArtnet::c_InputArtnet (c_InputMgr::e_InputChannelIds NewInputChannelId,
                              c_InputMgr::e_InputType       NewChannelType,
                              uint32_t                        BufferSize) :
    c_InputCommon(NewInputChannelId, NewChannelType, BufferSize)

{
    // DEBUG_START;
    // DEBUG_V ("BufferSize: " + String (BufferSize));
    memset ((void*)UniverseArray, 0x00, sizeof (UniverseArray));
    // DEBUG_END;
} // c_InputArtnet

//-----------------------------------------------------------------------------
c_InputArtnet::~c_InputArtnet()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_InputArtnet

//-----------------------------------------------------------------------------
void c_InputArtnet::Begin ()
{
    // DEBUG_START;

    do // once
    {
        if (HasBeenInitialized) { break; }

        // DEBUG_V ("InputDataBufferSize: " + String(InputDataBufferSize));

        validateConfiguration ();
        // DEBUG_V ("");

        NetworkStateChanged (NetworkMgr.IsConnected (), false);

        // DEBUG_V ("");
        HasBeenInitialized = true;

    } while (false);

    // DEBUG_END;

} // Begin

//-----------------------------------------------------------------------------
void c_InputArtnet::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    JsonWrite(jsonConfig, CN_universe,       startUniverse);
    JsonWrite(jsonConfig, CN_universe_limit, ChannelsPerUniverse);
    JsonWrite(jsonConfig, CN_universe_start, FirstUniverseChannelOffset);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputArtnet::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonObject ArtnetStatus = jsonStatus[F ("Artnet")].to<JsonObject> ();
    JsonWrite(ArtnetStatus, CN_unifirst,   startUniverse);
    JsonWrite(ArtnetStatus, CN_unilast,    LastUniverse);
    JsonWrite(ArtnetStatus, CN_unichanlim, ChannelsPerUniverse);
    // DEBUG_V ("");

    JsonWrite(ArtnetStatus, F ("lastData"),   lastData);
    JsonWrite(ArtnetStatus, CN_num_packets,   num_packets);
    JsonWrite(ArtnetStatus, CN_packet_errors, packet_errors);
    JsonWrite(ArtnetStatus, CN_last_clientIP, pArtnet->getRemoteIP().toString ());
    JsonWrite(ArtnetStatus, CN_PollCounter,   PollCounter);

    JsonArray ArtnetUniverseStatus = ArtnetStatus[(char*)CN_channels].to<JsonArray> ();

    for (auto & CurrentUniverse : UniverseArray)
    {
        JsonObject ArtnetCurrentUniverseStatus = ArtnetUniverseStatus.add<JsonObject> ();

        JsonWrite(ArtnetCurrentUniverseStatus, CN_errors,      CurrentUniverse.SequenceErrorCounter);
        JsonWrite(ArtnetCurrentUniverseStatus, CN_num_packets, CurrentUniverse.num_packets);
    }

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputArtnet::onDmxFrame (uint16_t  CurrentUniverseId,
                                uint32_t  length,
                                uint8_t   SequenceNumber,
                                uint8_t * data,
                                IPAddress remoteIP)
{
    // DEBUG_START;
    if(!IsInputChannelActive)
    {}
    else if ((startUniverse <= CurrentUniverseId) && (LastUniverse >= CurrentUniverseId))
    {
        // Universe offset and sequence tracking
        Universe_t & CurrentUniverse = UniverseArray[CurrentUniverseId - startUniverse];

        // Do we need to update a sequnce error?
        if (SequenceNumber != CurrentUniverse.SequenceNumber)
        {
            // DEBUG_V(String("             CurrentUniverseId: ") + String(CurrentUniverseId));
            // DEBUG_V(String("                SequenceNumber: ") + String(SequenceNumber));
            // DEBUG_V(String("CurrentUniverse.SequenceNumber: ") + String(CurrentUniverse.SequenceNumber));
            CurrentUniverse.SequenceNumber = SequenceNumber;
            // some systems always send a zero sequence number so we ignore the error in that case.
            if(0 != SequenceNumber)
            {
	            CurrentUniverse.SequenceErrorCounter++;
                ++packet_errors;
            }
        }

        ++CurrentUniverse.SequenceNumber;
        ++CurrentUniverse.num_packets;
        ++num_packets;

        // DEBUG_V (String ("data[0]: ") + String (data[0], HEX));

        lastData = data[0];
        OutputMgr.WriteChannelData( CurrentUniverse.DestinationOffset,
                                 min(CurrentUniverse.BytesToCopy, length),
                                 &data[CurrentUniverse.SourceDataOffset]);

        InputMgr.RestartBlankTimer (GetInputChannelId ());
    }
    else
    {
        // DEBUG_V ("Not interested in this universe");
        // DEBUG_V(String("CurrentUniverseId: ") + String(CurrentUniverseId));
        // DEBUG_V(String("    startUniverse: ") + String(startUniverse));
        // DEBUG_V(String("     LastUniverse: ") + String(LastUniverse));
    }
    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_InputArtnet::onDmxPoll (IPAddress  BroadcastIP)
{
    // DEBUG_START;

    logcon(String(F("POLL from ")) + pArtnet->getRemoteIP().toString() +  F(", broadcast addr: ") + BroadcastIP.toString());
    PollCounter ++;

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_InputArtnet::SetBufferInfo (uint32_t BufferSize)
{
    // DEBUG_START;

    InputDataBufferSize = BufferSize;

    if (HasBeenInitialized)
    {
        // buffer has moved. Start Over
        HasBeenInitialized = false;
        Begin ();
    }

    SetBufferTranslation ();

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
void c_InputArtnet::SetBufferTranslation ()
{
    // DEBUG_START;

    // for each possible universe, set the start and size

    uint32_t InputOffset = FirstUniverseChannelOffset - 1;
    uint32_t DestinationOffset = 0;
    uint32_t BytesLeftToMap = InputDataBufferSize;

    // set up the bytes for the First Universe
    uint32_t BytesInUniverse = ChannelsPerUniverse - InputOffset;
    // DEBUG_V (String ("ChannelsPerUniverse: ") + String (uint32_t (ChannelsPerUniverse), HEX));

    for (auto& CurrentUniverse : UniverseArray)
    {
        uint32_t BytesInThisUniverse        = min (BytesInUniverse, BytesLeftToMap);
        CurrentUniverse.DestinationOffset = DestinationOffset;
        CurrentUniverse.BytesToCopy       = BytesInThisUniverse;
        CurrentUniverse.SourceDataOffset  = InputOffset;
        // CurrentUniverse.SequenceErrorCounter = 0;
        // CurrentUniverse.SequenceNumber = 0;

        // DEBUG_V (String ("        Destination: ") + String (uint32_t (CurrentUniverse.Destination), HEX));
        // DEBUG_V (String ("        BytesToCopy: ") + String (CurrentUniverse.BytesToCopy, HEX));
        // DEBUG_V (String ("   SourceDataOffset: ") + String (CurrentUniverse.SourceDataOffset, HEX));

        DestinationOffset += BytesInThisUniverse;
        BytesLeftToMap    -= BytesInThisUniverse;
        BytesInUniverse    = ChannelsPerUniverse;
        InputOffset        = 0;
    }

    if (0 != BytesLeftToMap)
    {
        logcon (String (F ("ERROR: Universe configuration is too small to fill output buffer. Outputs have been truncated.")));
    }

    // DEBUG_END;

} // SetBufferTranslation

//-----------------------------------------------------------------------------
bool c_InputArtnet::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    setFromJSON (startUniverse,              jsonConfig, CN_universe);
    setFromJSON (ChannelsPerUniverse,        jsonConfig, CN_universe_limit);
    setFromJSON (FirstUniverseChannelOffset, jsonConfig, CN_universe_start);

    validateConfiguration ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    // DEBUG_END;
    return true;
} // SetConfig

static c_InputArtnet * fMe = nullptr;

//-----------------------------------------------------------------------------
// Subscribe to "n" universes, starting at "universe"
void c_InputArtnet::SetUpArtnet ()
{
    // DEBUG_START;

    if (nullptr == pArtnet)
    {
        // DEBUG_V ("");
        pArtnet = new Artnet ();
        pArtnet->begin ();

        byte broadcast[] = { 10, 0, 1, 255 };
        pArtnet->setBroadcast (broadcast);
        logcon (String (F ("Subscribed to broadcast")));

        // DEBUG_V ("");

        fMe = this; // hate this
        pArtnet->setArtDmxCallback ([](uint16_t UniverseId, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP)
        {
            fMe->onDmxFrame (UniverseId, length, sequence, data, remoteIP);
        });

        pArtnet->setArtPollCallback ([](IPAddress BroadcastIP)
        {
            fMe->onDmxPoll (BroadcastIP);
        });
    }
    // DEBUG_V ("");

    logcon (String (F ("Listening for ")) + InputDataBufferSize +
        F (" channels from Universe ") + startUniverse +
        F (" to ") + LastUniverse);
    // DEBUG_END;

} // SubscribeToBroadcastDomain

//-----------------------------------------------------------------------------
void c_InputArtnet::validateConfiguration ()
{
    // DEBUG_START;

    // DEBUG_V (String ("             startUniverse: ") + String (startUniverse));
    // DEBUG_V (String ("       ChannelsPerUniverse: ") + String (ChannelsPerUniverse));
    // DEBUG_V (String ("FirstUniverseChannelOffset: ") + String (FirstUniverseChannelOffset));
    // DEBUG_V (String ("              LastUniverse: ") + String (startUniverse));

    // DEBUG_V ("");
    if (ChannelsPerUniverse > UNIVERSE_MAX || ChannelsPerUniverse < 1)
    {
        // DEBUG_V (String ("ERROR: ChannelsPerUniverse: ") + String (ChannelsPerUniverse));
        ChannelsPerUniverse = UNIVERSE_MAX;
    }

    // DEBUG_V ("");
    if (FirstUniverseChannelOffset < 1)
    {
        // DEBUG_V (String ("ERROR: FirstUniverseChannelOffset: ") + String (FirstUniverseChannelOffset));
        // move to the start of the first universe
        FirstUniverseChannelOffset = 1;
    }
    else if (FirstUniverseChannelOffset > ChannelsPerUniverse)
    {
        // DEBUG_V (String ("ERROR: FirstUniverseChannelOffset: ") + String (FirstUniverseChannelOffset));

        // channel start must be within the first universe
        FirstUniverseChannelOffset = ChannelsPerUniverse - 1;
    }

    // Find the last universe we should listen for
     // DEBUG_V ("");
    uint32_t span = FirstUniverseChannelOffset + InputDataBufferSize - 1;
    if (span % ChannelsPerUniverse)
    {
        LastUniverse = startUniverse + span / ChannelsPerUniverse;
    }
    else
    {
        LastUniverse = startUniverse + span / ChannelsPerUniverse - 1;
    }

    // DEBUG_V ("");

    SetBufferTranslation ();

    // DEBUG_V ("");
    // pArtnet->stats.num_packets = 0;

    // DEBUG_END;

} // validateConfiguration

//-----------------------------------------------------------------------------
void c_InputArtnet::NetworkStateChanged (bool IsConnected)
{
    // DEBUG_START;

    // NetworkStateChanged (IsConnected, true);
    NetworkStateChanged (IsConnected, false);

    // DEBUG_END;

} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_InputArtnet::NetworkStateChanged (bool IsConnected, bool ReBootAllowed)
{
    // DEBUG_START;

    if (IsConnected)
    {
        SetUpArtnet ();
    }

    // DEBUG_END;

} // NetworkStateChanged
