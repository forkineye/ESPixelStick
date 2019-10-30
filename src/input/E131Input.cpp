/*
* E131Input.h - Code to wrap ESPAsyncE131 for input
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2019 Shelby Merrick
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

#include "E131Input.h"

const String E131Input::KEY = "e131";
const String E131Input::CONFIG_FILE = "/e131.json";

E131Input::~E131Input() {
    destroy();
}

void E131Input::destroy() {
    if (seqTracker) free(seqTracker);
    if (seqError) free(seqError);
    delete e131;
}

void E131Input::init() {
    Serial.println(F("** E1.31 Initialization **"));

    // Call destroy for good measure and create a new ESPAsyncE131
    destroy();
    e131 = new ESPAsyncE131(10);

    // Load and validate our configuration
    load();

    // Get on with business
    if (multicast) {
        if (e131->begin(E131_MULTICAST, universe,
                uniLast - universe + 1)) {
            LOG_PORT.println(F("E131 Multicast Enabled."));
        }  else {
            LOG_PORT.println(F("*** E131 MULTICAST INIT FAILED ****"));
        }
    } else {
        if (e131->begin(E131_UNICAST)) {
            LOG_PORT.print(F("E131 Unicast port: "));
            LOG_PORT.println(E131_DEFAULT_PORT);
        } else {
            LOG_PORT.println(F("*** E131 UNICAST INIT FAILED ****"));
        }
    }
}

String E131Input::getKey() {
    return KEY;
}

String E131Input::getBrief() {
    return "E1.31 (sACN)";
}

void E131Input::validate() {
    if (universe < 1)
        universe = 1;
    if (universe_limit > UNIVERSE_MAX || universe_limit < 1)
        universe_limit = UNIVERSE_MAX;
    if (channel_start < 1)
        channel_start = 1;
    else if (channel_start > universe_limit)
        channel_start = universe_limit;

//From updateConfig():
   // Find the last universe we should listen for
    uint16_t span = channel_start + showBufferSize - 1;
    if (span % universe_limit)
        uniLast = universe + span / universe_limit;
    else
        uniLast = universe + span / universe_limit - 1;

    // Setup the sequence error tracker
    uint8_t uniTotal = (uniLast + 1) - universe;

    if (seqTracker) free(seqTracker);
    if ((seqTracker = static_cast<uint8_t *>(malloc(uniTotal))))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if ((seqError = static_cast<uint32_t *>(malloc(uniTotal * 4))))
        memset(seqError, 0x00, uniTotal * 4);

    // Zero out packet stats
    e131->stats.num_packets = 0;

    LOG_PORT.printf("Listening for %u channels from Universe %u to %u.\n",
            showBufferSize, universe, uniLast);

    // Setup IGMP subscriptions if multicast is enabled
    if (multicast)
        multiSub();
}

void E131Input::deserialize(DynamicJsonDocument &json) {
    if (json.containsKey("e131")) {
        universe = json["e131"]["universe"];
        universe_limit = json["e131"]["universe_limit"];
        channel_start = json["e131"]["channel_start"];
        multicast = json["e131"]["multicast"];
    } else {
        LOG_PORT.println("No e131 settings found.");
    }
}

void E131Input::load() {
    if (FileIO::loadConfig(CONFIG_FILE, std::bind(
            &E131Input::deserialize, this, std::placeholders::_1))) {
        validate();
    } else {
        // Load failed, create a new config file and save it
        universe = 1;
        universe_limit = 512;
        channel_start = 1;
        multicast = true;
        validate();
        save();
    }
}

String E131Input::serialize() {
    DynamicJsonDocument json(1024);

    JsonObject e131 = json.createNestedObject("e131");
    e131["universe"] = universe;
    e131["universe_limit"] = universe_limit;
    e131["channel_start"] = channel_start;
    e131["multicast"] = multicast;

    String jsonString;
    serializeJsonPretty(json, jsonString);
    return jsonString;
}

void E131Input::save() {
    FileIO::saveConfig(CONFIG_FILE, serialize());
}

// Subscribe to "n" universes, starting at "universe"
void E131Input::multiSub() {
    uint8_t count;
    ip_addr_t ifaddr;
    ip_addr_t multicast_addr;

    count = uniLast - universe + 1;
    ifaddr.addr = static_cast<uint32_t>(WiFi.localIP());
    for (uint8_t i = 0; i < count; i++) {
        multicast_addr.addr = static_cast<uint32_t>(IPAddress(239, 255,
                (((universe + i) >> 8) & 0xff),
                (((universe + i) >> 0) & 0xff)));
        igmp_joingroup(&ifaddr, &multicast_addr);
    }
}

void E131Input::process() {
    // Parse a packet and update pixels
    while (!e131->isEmpty()) {
        e131_packet_t packet;
        e131->pull(&packet);
        uint16_t universe = htons(packet.universe);
        uint8_t *data = packet.property_values + 1;
        //LOG_PORT.print(universe);
        //LOG_PORT.println(packet.sequence_number);
        if ((universe >= universe) && (universe <= uniLast)) {
            // Universe offset and sequence tracking
            uint8_t uniOffset = (universe - universe);
            if (packet.sequence_number != seqTracker[uniOffset]++) {
                LOG_PORT.print(F("Sequence Error - expected: "));
                LOG_PORT.print(seqTracker[uniOffset] - 1);
                LOG_PORT.print(F(" actual: "));
                LOG_PORT.print(packet.sequence_number);
                LOG_PORT.print(F(" universe: "));
                LOG_PORT.println(universe);
                seqError[uniOffset]++;
                seqTracker[uniOffset] = packet.sequence_number + 1;
            }

            // Offset the channels if required
            uint16_t offset = 0;
            offset = channel_start - 1;

            // Find start of data based off the Universe
            int16_t dataStart = uniOffset * universe_limit - offset;

            // Calculate how much data we need for this buffer
            uint16_t dataStop = showBufferSize;
            uint16_t channels = htons(packet.property_value_count) - 1;
            if (universe_limit < channels)
                channels = universe_limit;
            if ((dataStart + channels) < dataStop)
                dataStop = dataStart + channels;

            // Set the data
            uint16_t buffloc = 0;

            // ignore data from start of first Universe before channel_start
            if (dataStart < 0) {
                dataStart = 0;
                buffloc = channel_start - 1;
            }

            for (int i = dataStart; i < dataStop; i++) {
                showBuffer[i] = data[buffloc];
//                pixels.setValue(i, data[buffloc]);
                buffloc++;
            }
        }
    }

}