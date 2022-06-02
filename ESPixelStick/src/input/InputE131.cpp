/*
* E131Input.cpp - Code to wrap ESPAsyncE131 for input
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
*/

#include "InputE131.hpp"
#include "../network/NetworkMgr.hpp"

//-----------------------------------------------------------------------------
c_InputE131::c_InputE131 (c_InputMgr::e_InputChannelIds NewInputChannelId,
                          c_InputMgr::e_InputType       NewChannelType,
                          size_t                        BufferSize) :
    c_InputCommon(NewInputChannelId, NewChannelType, BufferSize)

{
    // DEBUG_START;
    // DEBUG_V ("BufferSize: " + String (BufferSize));
    e131 = new ESPAsyncE131 (0);

    memset ((void*)UniverseArray, 0x00, SaferArrayByteSize(UniverseArray));

    // DEBUG_END;
} // c_InputE131

//-----------------------------------------------------------------------------
c_InputE131::~c_InputE131()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_InputE131

//-----------------------------------------------------------------------------
void c_InputE131::Begin ()
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V ("InputDataBufferSize: " + String(InputDataBufferSize));

        validateConfiguration ();
        // DEBUG_V ("");

        // DEBUG_V ("");
        e131->registerCallback ( (void*)this, [] (e131_packet_t* Packet, void * pThis)
            {
                ((c_InputE131*)pThis)->ProcessIncomingE131Data (Packet);
            });

        NetworkStateChanged (NetworkMgr.IsConnected (), false);

        HasBeenInitialized = true;

    } while (false);

    // DEBUG_END;

} // Begin

//-----------------------------------------------------------------------------
void c_InputE131::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    jsonConfig[CN_universe]       = startUniverse;
    jsonConfig[CN_universe_limit] = ChannelsPerUniverse;
    jsonConfig[CN_universe_start] = FirstUniverseChannelOffset;
    jsonConfig[CN_port]           = PortId;

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputE131::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonObject e131Status = jsonStatus.createNestedObject (F ("e131"));
    e131Status[CN_id]         = InputChannelId;
    e131Status[CN_unifirst]   = startUniverse;
    e131Status[CN_unilast ]   = LastUniverse;
    e131Status[CN_unichanlim] = ChannelsPerUniverse;

    e131Status[CN_num_packets]   = e131->stats.num_packets;
    e131Status[CN_last_clientIP] = uint32_t(e131->stats.last_clientIP);
    // DEBUG_V ("");

    JsonArray e131UniverseStatus = e131Status.createNestedArray (CN_channels);
    uint32_t TotalErrors = e131->stats.packet_errors;
    for (auto & CurrentUniverse : UniverseArray)
    {
        JsonObject e131CurrentUniverseStatus = e131UniverseStatus.createNestedObject ();

        e131CurrentUniverseStatus[CN_errors] = CurrentUniverse.SequenceErrorCounter;
        TotalErrors += CurrentUniverse.SequenceErrorCounter;
    }

    e131Status[CN_packet_errors] = TotalErrors;

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputE131::Process ()
{
    // DEBUG_START;

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputE131::ProcessIncomingE131Data (e131_packet_t * packet)
{
    // DEBUG_START;

    uint8_t   * E131Data;
    uint16_t    CurrentUniverseId;

    do // once
    {
        if (0 == InputDataBufferSize)
        {
            // no place to put any data
            break;
        }

        CurrentUniverseId = ntohs (packet->universe);
        E131Data = packet->property_values + 1;

        // DEBUG_V ("     CurrentUniverseId: " + String(CurrentUniverseId));
        // DEBUG_V ("packet.sequence_number: " + String(packet.sequence_number));

        if ((startUniverse <= CurrentUniverseId) && (LastUniverse >= CurrentUniverseId))
        {
            // Universe offset and sequence tracking
            Universe_t& CurrentUniverse = UniverseArray[CurrentUniverseId - startUniverse];

            // Do we need to update a sequnce error?
            if (packet->sequence_number != CurrentUniverse.SequenceNumber)
            {
                // DEBUG_V (F ("E1.31 Sequence Error - expected: "));
                // DEBUG_V (CurrentUniverse.SequenceNumber);
                // DEBUG_V (F (" actual: "));
                // DEBUG_V (packet->sequence_number);
                // DEBUG_V (" " + String (CN_universe) + " : ");
                // DEBUG_V (CurrentUniverseId);

                CurrentUniverse.SequenceErrorCounter++;
                CurrentUniverse.SequenceNumber = packet->sequence_number;
            }

            ++CurrentUniverse.SequenceNumber;

            size_t NumBytesOfE131Data = size_t(ntohs (packet->property_value_count) - 1);
            OutputMgr.WriteChannelData(CurrentUniverse.DestinationOffset, 
                                    min(CurrentUniverse.BytesToCopy, NumBytesOfE131Data), 
                                    &E131Data[CurrentUniverse.SourceDataOffset]);
/*
            memcpy(CurrentUniverse.Destination, 
                   &E131Data[CurrentUniverse.SourceDataOffset],
                   min(CurrentUniverse.BytesToCopy, NumBytesOfE131Data));
*/
            InputMgr.RestartBlankTimer (GetInputChannelId ());
        }
        else
        {
            // DEBUG_V ("Not interested in this universe");
        }

    } while (false);

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputE131::SetBufferInfo (size_t BufferSize)
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
void c_InputE131::SetBufferTranslation ()
{
    // DEBUG_START;

    // for each possible universe, set the start and size

    size_t InputOffset = FirstUniverseChannelOffset - 1;
    size_t DestinationOffset = 0;
    size_t  BytesLeftToMap = InputDataBufferSize;

    // set up the bytes for the First Universe
    size_t BytesInUniverse = ChannelsPerUniverse - InputOffset;
    // DEBUG_V (String ("    ChannelsPerUniverse:   ") + String (uint32_t (ChannelsPerUniverse)));
    // DEBUG_V (String ("    InputDataBufferSize:   ") + String (uint32_t (InputDataBufferSize)));

    for (auto& CurrentUniverse : UniverseArray)
    {
        uint16_t BytesInThisUniverse = min (BytesInUniverse, BytesLeftToMap);
        // DEBUG_V (String ("BytesInThisUniverse: 0x") + String (BytesInThisUniverse, HEX));
        CurrentUniverse.DestinationOffset = DestinationOffset;
        CurrentUniverse.BytesToCopy = BytesInThisUniverse;
        CurrentUniverse.SourceDataOffset = InputOffset;
        CurrentUniverse.SequenceErrorCounter = 0;
        CurrentUniverse.SequenceNumber = 0;

        // DEBUG_V (String ("        Destination: 0x") + String (uint32_t (CurrentUniverse.Destination), HEX));
        // DEBUG_V (String ("        BytesToCopy:   ") + String (CurrentUniverse.BytesToCopy));
        // DEBUG_V (String ("   SourceDataOffset: 0x") + String (CurrentUniverse.SourceDataOffset, HEX));

        DestinationOffset += BytesInThisUniverse;
        BytesLeftToMap -= BytesInThisUniverse;
        BytesInUniverse = ChannelsPerUniverse;
        InputOffset = 0;
        // DEBUG_V (String ("     BytesLeftToMap:   ") + String (BytesLeftToMap));
    }

    if (0 != BytesLeftToMap)
    {
        logcon (String (F ("ERROR: Universe configuration is too small to fill output buffer. Outputs have been truncated.")));
    }

    // DEBUG_END;

} // SetBufferTranslation

//-----------------------------------------------------------------------------
bool c_InputE131::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    ESPAsyncE131PortId OldPortId = PortId;

    setFromJSON (startUniverse,              jsonConfig, CN_universe);
    setFromJSON (ChannelsPerUniverse,        jsonConfig, CN_universe_limit);
    setFromJSON (FirstUniverseChannelOffset, jsonConfig, CN_universe_start);
    setFromJSON (PortId,                     jsonConfig, CN_port);

    if ((OldPortId != PortId) && (ESPAsyncE131Initialized))
    {
        // ask for a reboot. 
        reboot = true;
        logcon (String (F ("Requesting reboot on change of UDP port.")));
    }

    validateConfiguration ();

    // Update the config fields in case the validator changed them
    GetConfig (jsonConfig);

    // DEBUG_END;
    return true;
} // SetConfig

//-----------------------------------------------------------------------------
void c_InputE131::validateConfiguration ()
{
    // DEBUG_START;

    // DEBUG_V (String ("             startUniverse: ") + String (startUniverse));
    // DEBUG_V (String ("       ChannelsPerUniverse: ") + String (ChannelsPerUniverse));
    // DEBUG_V (String ("FirstUniverseChannelOffset: ") + String (FirstUniverseChannelOffset));
    // DEBUG_V (String ("              LastUniverse: ") + String (startUniverse));

    if (startUniverse < 1)
    {
        // DEBUG_V (String("ERROR: startUniverse: ") + String(startUniverse));

        startUniverse = 1;
    }

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
    size_t span = FirstUniverseChannelOffset + InputDataBufferSize - 1;
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

    // DEBUG_END;

} // validateConfiguration

//-----------------------------------------------------------------------------
void c_InputE131::NetworkStateChanged (bool IsConnected)
{
    NetworkStateChanged (IsConnected, false);
} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_InputE131::NetworkStateChanged (bool IsConnected, bool ReBootAllowed)
{
    // DEBUG_START;

    if (IsConnected)
    {
        // Get on with business
        if (e131->begin (e131_listen_t::E131_MULTICAST, PortId, startUniverse, LastUniverse - startUniverse + 1))
        {
            // logcon (String (F ("Multicast enabled")));
        }
        else
        {
            logcon (String (CN_stars) + F (" E1.31 MULTICAST INIT FAILED ") + CN_stars);
        }

        // DEBUG_V ("");

        if (e131->begin (e131_listen_t::E131_UNICAST, PortId, startUniverse, LastUniverse - startUniverse + 1))
        {
            // logcon (String (F ("Listening on port ")) + PortId);
        }
        else
        {
            logcon (CN_stars + String (F (" E1.31 UNICAST INIT FAILED ")) + CN_stars);
        }

        logcon (String (F ("Listening for ")) + InputDataBufferSize +
                        F (" channels from Universe ") + startUniverse +
                        F (" to ") + LastUniverse + 
                        F (" on port ") + PortId);

        ESPAsyncE131Initialized = true;
    }
    else if (ReBootAllowed)
    {
        // handle a disconnect
        // E1.31 does not do this gracefully. A loss of connection needs a reboot
        // extern bool reboot;
        reboot = true;
        logcon (String (F ("Input requesting reboot on loss of WiFi connection.")));
    }

    // DEBUG_END;

} // NetworkStateChanged
