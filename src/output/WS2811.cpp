/*
* WS2811.cpp - WS2811 driver code for ESPixelStick
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

#include "WS2811.h"

extern "C" {
#include <eagle_soc.h>
#include <ets_sys.h>
#include <uart.h>
#include <uart_register.h>
}

static const uint8_t    *uart_buffer;       // Buffer tracker
static const uint8_t    *uart_buffer_tail;  // Buffer tracker

uint8_t WS2811::rOffset = 0;
uint8_t WS2811::gOffset = 1;
uint8_t WS2811::bOffset = 2;

const String WS2811::KEY = "ws2811";
const String WS2811::CONFIG_FILE = "/ws2811.json";

WS2811::~WS2811() {
    destroy();
}

void WS2811::destroy() {
    // free stuff
    if (asyncdata) free(asyncdata);
    Serial1.end();
    pinMode(DATA_PIN, INPUT);
}

void WS2811::init() {
    Serial.println(F("** WS2811 Initialization **"));

    // Call destroy for good measure and create new stuff if needed
    destroy();

    // Load and validate our configuration
    load();

    // Set output pins
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);

    // Setup asyncdata buffer
    szBuffer = pixelCount * 3;
    if (asyncdata) free(asyncdata);
    if (asyncdata = static_cast<uint8_t *>(malloc(szBuffer))) {
        memset(asyncdata, 0, szBuffer);
    } else {
        szBuffer = 0;
    }

    // Calculate our refresh time
    refreshTime = WS2811_TFRAME * pixelCount + WS2811_TIDLE;

    // Initialize for WS2811 via UART
    // Serial rate is 4x 800KHz for WS2811
    Serial1.begin(3200000, SERIAL_6N1, SERIAL_TX_ONLY);
    CLEAR_PERI_REG_MASK(UART_CONF0(UART), UART_INV_MASK);
    SET_PERI_REG_MASK(UART_CONF0(UART), (BIT(22)));

    // Clear FIFOs
    SET_PERI_REG_MASK(UART_CONF0(UART), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(UART), UART_RXFIFO_RST | UART_TXFIFO_RST);

    // Disable all interrupts
    ETS_UART_INTR_DISABLE();

    // Atttach interrupt handler
    ETS_UART_INTR_ATTACH(handleWS2811, NULL);

    // Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO
    WRITE_PERI_REG(UART_CONF1(UART), 80 << UART_TXFIFO_EMPTY_THRHD_S);

    // Disable RX & TX interrupts. It is enabled by uart.c in the SDK
    CLEAR_PERI_REG_MASK(UART_INT_ENA(UART), UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);

    // Clear all pending interrupts in UART1
    WRITE_PERI_REG(UART_INT_CLR(UART), 0xffff);

    // Reenable interrupts
    ETS_UART_INTR_ENABLE();
}

String WS2811::getKey() {
    return KEY;
}

String WS2811::getBrief() {
    return "WS2811 800kHz";
}

uint8_t WS2811::getTupleSize() {
    return 3;   // 3 bytes per pixel
}

uint16_t WS2811::getTupleCount() {
    return 170;
}

void WS2811::validate() {
    if (pixelCount > PIXEL_LIMIT)
        pixelCount = PIXEL_LIMIT;
    else if (pixelCount < 1)
        pixelCount = 170;

    if (groupSize > pixelCount)
        groupSize = pixelCount;
    else if (groupSize < 1)
        groupSize = 1;

    // Default gamma value
    if (gammaVal <= 0)
        gammaVal = 2.2;

    // Default brightness value
    if (briteVal <= 0)
        briteVal = 1.0;

    setGroup(groupSize, zigSize);
    updateGammaTable(gammaVal, briteVal);
    updateOrder(color);
}

void WS2811::deserialize(DynamicJsonDocument &json) {
    if (json.containsKey("ws2811")) {
        pixel_color = PixelColor(static_cast<uint8_t>(json["pixel"]["color"]));
        pixelCount = json["ws2811"]["pixelCount"];
        groupSize = json["ws2811"]["groupSize"];
        zigSize = json["ws2811"]["zigSize"];
        gammaVal = json["ws2811"]["gammaVal"];
        briteVal = json["ws2811"]["briteVal"];
    } else {
        LOG_PORT.println("No WS2811 settings found.");
    }
}

void WS2811::load() {
    if (FileIO::loadConfig(CONFIG_FILE, std::bind(
            &WS2811::deserialize, this, std::placeholders::_1))) {
        validate();
    } else {
        // Load failed, create a new config file and save it
        pixel_color = PixelColor::RGB;
        groupSize = 1;
        zigSize = 0;
        gammaVal = 2.2;
        briteVal = 1.0;
        validate();
        save();
    }
}

String WS2811::serialize(boolean pretty = false) {
    DynamicJsonDocument json(1024);

    JsonObject pixel = json.createNestedObject("ws2811");
    pixel["color"] = static_cast<uint8_t>(pixel_color);
    pixel["pixelCount"] = pixelCount;
    pixel["groupSize"] = groupSize;
    pixel["zigSize"] = zigSize;
    pixel["gammaVal"] = gammaVal;
    pixel["briteVal"] = briteVal;

    String jsonString;
    if (pretty)
        serializeJsonPretty(json, jsonString);
    else
        serializeJson(json, jsonString);

    return jsonString;
}

void WS2811::save() {
    FileIO::saveConfig(CONFIG_FILE, serialize());
}

void WS2811::updateOrder(PixelColor color) {
    this->color = color;

    switch (color) {
        case PixelColor::GRB:
            rOffset = 1;
            gOffset = 0;
            bOffset = 2;
            break;
        case PixelColor::BRG:
            rOffset = 1;
            gOffset = 2;
            bOffset = 0;
            break;
        case PixelColor::RBG:
            rOffset = 0;
            gOffset = 2;
            bOffset = 1;
            break;
        case PixelColor::GBR:
            rOffset = 2;
            gOffset = 0;
            bOffset = 1;
            break;
        case PixelColor::BGR:
            rOffset = 2;
            gOffset = 1;
            bOffset = 0;
            break;
        default:
            rOffset = 0;
            gOffset = 1;
            bOffset = 2;
    }
}

void ICACHE_RAM_ATTR WS2811::handleWS2811(void *param) {
    /* Process if UART1 */
    if (READ_PERI_REG(UART_INT_ST(UART1))) {
        // Fill the FIFO with new data
        uart_buffer = fillWS2811(uart_buffer, uart_buffer_tail);

        // Disable TX interrupt when done
        if (uart_buffer == uart_buffer_tail)
            CLEAR_PERI_REG_MASK(UART_INT_ENA(UART1), UART_TXFIFO_EMPTY_INT_ENA);

        // Clear all interrupts flags (just in case)
        WRITE_PERI_REG(UART_INT_CLR(UART1), 0xffff);
    }

    /* Clear if UART0 */
    if (READ_PERI_REG(UART_INT_ST(UART0)))
        WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);
}

const uint8_t* ICACHE_RAM_ATTR WS2811::fillWS2811(const uint8_t *buff,
        const uint8_t *tail) {
    uint8_t avail = (UART_TX_FIFO_SIZE - getFifoLength()) / 4;
    if (tail - buff > avail)
        tail = buff + avail;

    while (buff + 2 < tail) {
        uint8_t subpix = buff[rOffset];
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 6) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 4) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 2) & 0x3]);
        enqueue(LOOKUP_2811[GAMMA_TABLE[subpix] & 0x3]);

        subpix = buff[gOffset];
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 6) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 4) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 2) & 0x3]);
        enqueue(LOOKUP_2811[GAMMA_TABLE[subpix] & 0x3]);

        subpix = buff[bOffset];
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 6) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 4) & 0x3]);
        enqueue(LOOKUP_2811[(GAMMA_TABLE[subpix] >> 2) & 0x3]);
        enqueue(LOOKUP_2811[GAMMA_TABLE[subpix] & 0x3]);

        buff += 3;
    }

    return buff;
}

void WS2811::render() {
    if (!canRefresh()) return;
    if (!showBuffer) return;

    if (!cntZigzag) {  // Normal / group copy
        for (size_t led = 0; led < szBuffer / 3; led++) {
            uint16 modifier = led / cntGroup;
            asyncdata[3 * led + 0] = showBuffer[3 * modifier + 0];
            asyncdata[3 * led + 1] = showBuffer[3 * modifier + 1];
            asyncdata[3 * led + 2] = showBuffer[3 * modifier + 2];
        }
    } else {  // Zigzag copy
        for (size_t led = 0; led < szBuffer / 3; led++) {
            uint16 modifier = led / cntGroup;
            if (led / cntZigzag % 2) { // Odd "zig"
                int group = cntZigzag * (led / cntZigzag);
                int this_led = (group + cntZigzag - (led % cntZigzag) - 1) / cntGroup;
                asyncdata[3 * led + 0] = showBuffer[3 * this_led + 0];
                asyncdata[3 * led + 1] = showBuffer[3 * this_led + 1];
                asyncdata[3 * led + 2] = showBuffer[3 * this_led + 2];
            } else { // Even "zag"
                asyncdata[3 * led + 0] = showBuffer[3 * modifier + 0];
                asyncdata[3 * led + 1] = showBuffer[3 * modifier + 1];
                asyncdata[3 * led + 2] = showBuffer[3 * modifier + 2];
            }
        }

    }

    uart_buffer = asyncdata;
    uart_buffer_tail = asyncdata + szBuffer;

    SET_PERI_REG_MASK(UART_INT_ENA(1), UART_TXFIFO_EMPTY_INT_ENA);
    startTime = micros();
}

uint8_t* WS2811::getData() {
    return asyncdata;	// data post grouping or zigzaging
}
