/*
* GECE.cpp - GECE driver code for ESPixelStick
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
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

#include "GECE.h"
#include "../ESPixelStick.h"
#include "../FileIO.h"

extern "C" {
#include <eagle_soc.h>
#include <ets_sys.h>
#include <uart.h>
#include <uart_register.h>
}

static const uint8_t    *uart_buffer;       // Buffer tracker
static const uint8_t    *uart_buffer_tail;  // Buffer tracker

const char GECE::KEY[] = "gece";
const char GECE::CONFIG_FILE[] = "/gece.json";

GECE::~GECE() {
    destroy();
}

void GECE::destroy() {
    // free stuff
    if (pbuff) free(pbuff);
    pbuff = NULL;
    Serial1.end();
    pinMode(DATA_PIN, INPUT);
}

void GECE::init() {
    Serial.println(F("** GECE Initialization **"));

    // Load and validate our configuration
    load();

    // Set output pins
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);

    if (pbuff) free(pbuff);
    if (pbuff = static_cast<uint8_t *>(malloc(GECE_PSIZE))) {
        memset(pbuff, 0, GECE_PSIZE);
    } else {
        pixel_count = 0;
    }

    refreshTime = (GECE_TFRAME + GECE_TIDLE) * pixel_count;

    // Serial rate is 3x 100KHz for GECE
    Serial1.begin(300000, SERIAL_7N1, SERIAL_TX_ONLY);
    SET_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_BRK);
    delayMicroseconds(GECE_TIDLE);
}

const char* GECE::getKey() {
    return KEY;
}

const char* GECE::getBrief() {
    return "GE Color Effects";
}

uint8_t GECE::getTupleSize() {
    return 3;   // 3 bytes per pixel
}

uint16_t GECE::getTupleCount() {
    return pixel_count;
}

void GECE::validate() {
    if (pixel_count > PIXEL_LIMIT)
        pixel_count = PIXEL_LIMIT;
    else if (pixel_count < 1)
        pixel_count = PIXEL_LIMIT;
}

boolean GECE::deserialize(DynamicJsonDocument &json) {
    boolean retval = false;
    if (json.containsKey(KEY)) {
        retval = FileIO::setFromJSON(pixel_count, json[KEY]["pixel_count"]);
    } else {
        LOG_PORT.println("No GECE settings found.");
    }
    return retval;
}

void GECE::load() {
    if (FileIO::loadConfig(CONFIG_FILE, std::bind(
            &GECE::deserialize, this, std::placeholders::_1))) {
        validate();
    } else {
        // Load failed, create a new config file and save it
        pixel_count = 63;
        save();
    }
}

String GECE::serialize(boolean pretty = false) {
    DynamicJsonDocument json(1024);

    JsonObject pixel = json.createNestedObject(KEY);
    pixel["pixel_count"] = pixel_count;

    String jsonString;
    if (pretty)
        serializeJsonPretty(json, jsonString);
    else
        serializeJson(json, jsonString);

    return jsonString;
}

void GECE::save() {
    validate();
    FileIO::saveConfig(CONFIG_FILE, serialize());
}


void ICACHE_RAM_ATTR GECE::render() {
    if (!canRefresh()) return;
    if (!showBuffer) return;

    uint32_t packet = 0;
    uint32_t pTime = 0;

    // Build a GECE packet
    startTime = micros();
    for (uint8_t i = 0; i < pixel_count; i++) {
        packet = (packet & ~GECE_ADDRESS_MASK) | (i << 20);
        packet = (packet & ~GECE_BRIGHTNESS_MASK) |
                (GECE_DEFAULT_BRIGHTNESS << 12);
        packet = (packet & ~GECE_BLUE_MASK) | (showBuffer[i*3+2] << 4);
        packet = (packet & ~GECE_GREEN_MASK) | showBuffer[i*3+1];
        packet = (packet & ~GECE_RED_MASK) | (showBuffer[i*3] >> 4);

        uint8_t shift = GECE_PSIZE;
        for (uint8_t i = 0; i < GECE_PSIZE; i++)
            pbuff[i] = LOOKUP_GECE[(packet >> --shift) & 0x1];

        // Wait until ready
        while ((micros() - pTime) < (GECE_TFRAME + GECE_TIDLE)) {}

        // 10us start bit
        pTime = micros();
        uint32_t c = _getCycleCount();
        CLEAR_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_BRK);
        while ((_getCycleCount() - c) < CYCLES_GECE_START - 100) {}

        // Send packet and idle low (break)
        Serial1.write(pbuff, GECE_PSIZE);
        SET_PERI_REG_MASK(UART_CONF0(UART), UART_TXD_BRK);
    }
}