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
#include "../ESPixelStick.h"
#include "../FileIO.h"

extern "C" {
#if defined(ARDUINO_ARCH_ESP8266)
#include <eagle_soc.h>
#include <ets_sys.h>
#include <uart.h>
#include <uart_register.h>
#else
#   include <esp32-hal-uart.h>
#   include <soc/soc.h>
#   include <soc/uart_reg.h>
#   include <rom/ets_sys.h>
#   include <driver/uart.h>

// Define ESP8266 style macro conversions to limit changes in the rest of the code.
#   define UART_CONF0           UART_CONF0_REG
#   define UART_CONF1           UART_CONF1_REG
#   define UART_INT_ENA         UART_INT_ENA_REG
#   define UART_INT_CLR         UART_INT_CLR_REG
#   define UART_INT_ST          UART_INT_ST_REG
#   define UART0                uart_port_t::UART_NUM_0
#   define UART1                uart_port_t::UART_NUM_1
#   define UART2                uart_port_t::UART_NUM_2
#   define UART_TX_FIFO_SIZE    UART_FIFO_LEN
#   define PIXEL_TXD            (13)
#   define PIXEL_RXD            (UART_PIN_NO_CHANGE)
#   define PIXEL_RTS            (UART_PIN_NO_CHANGE)
#   define PIXEL_CTS            (UART_PIN_NO_CHANGE)
#   define WS2811_DATA_SPEED    (800000)
    // Set TX FIFO trigger. 80 bytes gives 200 microsecs to refill the FIFO

    // TX FIFO trigger level. 40 bytes gives 100us before the FIFO goes empty
    // We need to fill the FIFO at a rate faster than 0.3us per byte (1.2us/pixel)
#   define PIXEL_FIFO_TRIGGER_LEVEL (40)

// static uart_intr_config_t IsrConfig;
#endif
}

#define NUM_INTENSITY_BYTES_PER_PIXEL    	3
#define NUM_DATA_BYTES_PER_INTENSITY_BYTE	4

/*
* Inverted 6N1 UART lookup table for ws2811, first 2 bits ignored.
* Start and stop bits are part of the pixel stream.
*/
char LOOKUP_2811[4] = {
    0b00110111,     // 00 - (1)000 100(0)
    0b00000111,     // 01 - (1)000 111(0)
    0b00110100,     // 10 - (1)110 100(0)
    0b00000100      // 11 - (1)110 111(0)
};

static const uint8_t    *uart_buffer;       // Buffer tracker
static const uint8_t    *uart_buffer_tail;  // Buffer tracker

uint8_t gamma_table[256] = { 0 };
uint8_t WS2811::rOffset = 0;
uint8_t WS2811::gOffset = 1;
uint8_t WS2811::bOffset = 2;

const char WS2811::KEY[] = "ws2811";
const char WS2811::CONFIG_FILE[] = "/ws2811.json";

WS2811::~WS2811() {
    destroy();
}

void WS2811::destroy() {
    // free stuff
    if (asyncdata) free(asyncdata);
    asyncdata = NULL;
    Serial1.end();
    pinMode(DATA_PIN, INPUT);
}

void WS2811::init() {
    Serial.println(F("** WS2811 Initialization **"));

    // Load and validate our configuration
    load();

    // Set output pins
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);

    // Setup asyncdata buffer
    szBuffer = pixel_count * 3;
    if (asyncdata) free(asyncdata);
    if (asyncdata = static_cast<uint8_t *>(malloc(szBuffer))) {
        memset(asyncdata, 0, szBuffer);
    } else {
        szBuffer = 0;
    }

    // Calculate our refresh time
    refreshTime = WS2811_TFRAME * pixel_count + WS2811_TIDLE;

    // Initialize for WS2811 via UART
#ifdef ARDUINO_ARCH_ESP8266
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
#elif defined (ARDUINO_ARCH_ESP32)
    // In the ESP32 you need to be careful which CPU is being configured 
    // to handle interrupts. These API functions are supposed to handle this 
    // selection.

    // kill the Arduino driver (if it is runing)
    Serial1.end ();

    // make sure no existing low level driver is running
    uart_driver_delete (UART1);

    // start the generic UART driver.
    // NOTE: Zero for RX buffer size causes errors in the uart API. 
    // Must be at least one byte larger than the fifo size

    /* Serial rate is 4x 800KHz for WS2811 */
    uart_config_t uart_config;
    uart_config.baud_rate           = 4 * 800000;
    uart_config.data_bits           = uart_word_length_t::UART_DATA_6_BITS;
    uart_config.flow_ctrl           = uart_hw_flowcontrol_t::UART_HW_FLOWCTRL_DISABLE;
    uart_config.parity              = uart_parity_t::UART_PARITY_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 1;
    uart_config.stop_bits           = uart_stop_bits_t::UART_STOP_BITS_1;
    uart_config.use_ref_tick        = false;

    ESP_ERROR_CHECK (uart_driver_install   (UART, UART_FIFO_LEN+1, 0, 0, NULL, 0));
    ESP_ERROR_CHECK (uart_param_config     (UART, &uart_config));
    ESP_ERROR_CHECK (uart_set_pin          (UART, PIXEL_TXD, PIXEL_RXD, PIXEL_RTS, PIXEL_CTS));
    ESP_ERROR_CHECK (uart_set_line_inverse (UART, UART_INVERSE_TXD));
    ESP_ERROR_CHECK (uart_set_mode         (UART, uart_mode_t::UART_MODE_UART));

    // release the pre registered UART handler/subroutines
    ESP_ERROR_CHECK (uart_disable_tx_intr (UART_NUM_1));
    ESP_ERROR_CHECK (uart_disable_rx_intr (UART_NUM_1));
    ESP_ERROR_CHECK (uart_isr_free (UART_NUM_1));

    ESP_ERROR_CHECK (uart_isr_register (UART_NUM_1, handleWS2811, nullptr, UART_TXFIFO_EMPTY_INT_ENA | ESP_INTR_FLAG_IRAM, nullptr));
#endif
}

const char* WS2811::getKey() {
    return KEY;
}

const char* WS2811::getBrief() {
    return "WS2811 800kHz";
}

uint8_t WS2811::getTupleSize() {
    return 3;   // 3 bytes per pixel
}

uint16_t WS2811::getTupleCount() {
    return pixel_count;
}

void WS2811::validate() {
    if (pixel_count > PIXEL_LIMIT)
        pixel_count = PIXEL_LIMIT;
    else if (pixel_count < 1)
        pixel_count = 170;

    if (group_size > pixel_count)
        group_size = pixel_count;
    else if (group_size < 1)
        group_size = 1;

    // Default gamma value
    if (gamma <= 0)
        gamma = 2.2;

    // Default brightness value
    if (brightness <= 0)
        brightness = 1.0;

    updateGammaTable();
    updateColorOrder();
}

boolean WS2811::deserialize(DynamicJsonDocument &json) {
    boolean retval = false;
    if (json.containsKey(KEY)) {
        retval = retval | FileIO::setFromJSON(color_order, json[KEY]["color_order"]);
        retval = retval | FileIO::setFromJSON(pixel_count, json[KEY]["pixel_count"]);
        retval = retval | FileIO::setFromJSON(group_size, json[KEY]["group_size"]);
        retval = retval | FileIO::setFromJSON(zig_size, json[KEY]["zig_size"]);
        retval = retval | FileIO::setFromJSON(gamma, json[KEY]["gamma"]);
        retval = retval | FileIO::setFromJSON(brightness, json[KEY]["brightness"]);
    } else {
        LOG_PORT.println("No WS2811 settings found.");
    }
    return retval;
}

void WS2811::load() {
    if (FileIO::loadConfig(CONFIG_FILE, std::bind(
            &WS2811::deserialize, this, std::placeholders::_1))) {
        validate();
    } else {
        // Load failed, create a new config file and save it
        color_order = "rgb";
        pixel_count = 170;
        group_size = 1;
        zig_size = 0;
        gamma = 2.2;
        brightness = 1.0;
        save();
    }
}

String WS2811::serialize(boolean pretty = false) {
    DynamicJsonDocument json(1024);

    JsonObject pixel = json.createNestedObject(KEY);
    pixel["color_order"] = color_order.c_str();
    pixel["pixel_count"] = pixel_count;
    pixel["group_size"] = group_size;
    pixel["zig_size"] = zig_size;
    pixel["gamma"] = gamma;
    pixel["brightness"] = brightness;

    String jsonString;
    if (pretty)
        serializeJsonPretty(json, jsonString);
    else
        serializeJson(json, jsonString);

    return jsonString;
}

void WS2811::save() {
    validate();
    FileIO::saveConfig(CONFIG_FILE, serialize());
}

void WS2811::updateColorOrder() {
    if (color_order.equalsIgnoreCase("grb")) {
        rOffset = 1; gOffset = 0; bOffset = 2;
    } else if (color_order.equalsIgnoreCase("brg")) {
        rOffset = 1; gOffset = 2; bOffset = 0;
    } else if (color_order.equalsIgnoreCase("rbg")) {
        rOffset = 0; gOffset = 2; bOffset = 1;
    } else if (color_order.equalsIgnoreCase("gbr")) {
        rOffset = 2; gOffset = 0; bOffset = 1;
    } else if (color_order.equalsIgnoreCase("bgr")) {
        rOffset = 2; gOffset = 1; bOffset = 0;
    } else {
        color_order="rgb";
        rOffset = 0; gOffset = 1; bOffset = 2;
    }
}

void ICACHE_RAM_ATTR WS2811::handleWS2811(void *param) {
    /* Process if UART1 */
    if (READ_PERI_REG(UART_INT_ST(UART1))) 
    {
        // Fill the FIFO with new data
        // free space in the FIFO divided by the number of data bytes per intensity 
        // gives the max number of intensities we can process
        register uint16_t IntensitySpaceInFifo = (((uint16_t)UART_TX_FIFO_SIZE) - (getFifoLength)) / NUM_DATA_BYTES_PER_INTENSITY_BYTE;

        // how many intesity bytes are present in the buffer to send?
        register uint16_t RemainingIntensityCount = (((uint16_t)(uart_buffer_tail - uart_buffer)));

        // determine how many intensities we can process right now.
        register uint16_t IntensitiesToSend = (IntensitySpaceInFifo < RemainingIntensityCount) ? (IntensitySpaceInFifo) : RemainingIntensityCount;

        // only read from ram once per intensity
        register uint8_t subpix = 0;

        while (0 != IntensitiesToSend--)
        {
            subpix = uart_buffer[0];
            enqueue ((LOOKUP_2811[(subpix >> 6) & 0x3]));
            enqueue ((LOOKUP_2811[(subpix >> 4) & 0x3]));
            enqueue ((LOOKUP_2811[(subpix >> 2) & 0x3]));
            enqueue ((LOOKUP_2811[(subpix >> 0) & 0x3]));

            // advance to the next intensity byte in the buffer
            uart_buffer++;
        };

        // Disable TX interrupt when done
        if (uart_buffer == uart_buffer_tail)
        {
            CLEAR_PERI_REG_MASK(UART_INT_ENA(UART1), UART_TXFIFO_EMPTY_INT_ENA);
        }

        // Clear all interrupts flags (just in case)
        WRITE_PERI_REG(UART_INT_CLR(UART1), 0xffff);
    }

    /* Clear if UART0 */
    if (READ_PERI_REG(UART_INT_ST(UART0)))
        WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);

#ifdef ARDUINO_ARCH_ESP32
    // todo. Should we touching the other UARTS?
    /* Clear if UART2 */
    if (READ_PERI_REG (UART_INT_ST (UART2)))
    {
        WRITE_PERI_REG (UART_INT_CLR (UART2), UART_INTR_MASK);
    }
#endif
}

void WS2811::updateGammaTable() {
    for (int i = 0; i < 256; i++) {
        gamma_table[i] = (uint8_t) min((255.0 * pow(i * brightness / 255.0, gamma) + 0.5), 255.0);
    }
}

void WS2811::render() {
    if (!canRefresh()) return;
    if (!showBuffer) return;

    // set up pointers into the pixel data space
    uint8_t* pSourceData = showBuffer; // source buffer
    uint8_t* pTargetData = asyncdata;  // target buffer

    // make sure the group count is valid
    if (0 == group_size) { group_size = 1; }

    // what type of copy are we making?
    if (!zig_size)
    {
        // Normal / group copy

        // for each destination pixel
        for (size_t CurrentDestinationPixelIndex = 0;
            CurrentDestinationPixelIndex < pixel_count;
            CurrentDestinationPixelIndex++)
        {
            // for each output pixel in the group (minimum of 1)
            for (size_t CurrentGroupIndex = 0;
                CurrentGroupIndex < group_size;
                ++CurrentGroupIndex, ++CurrentDestinationPixelIndex)
            {
                // write data to the output buffer
                *pTargetData++ = gamma_table[pSourceData[rOffset]];
                *pTargetData++ = gamma_table[pSourceData[gOffset]];
                *pTargetData++ = gamma_table[pSourceData[bOffset]];
            } // End for each intensity in current input pixel

            // point at the next pixel in the input buffer
            pSourceData += NUM_INTENSITY_BYTES_PER_PIXEL;

        } // end for each pixel in the output buffer
    } // end normal copy
    else
    {
        // Zigzag copy
        for (size_t CurrentDestinationPixelIndex = 0;
            CurrentDestinationPixelIndex < pixel_count;
            CurrentDestinationPixelIndex++)
        {
            if (CurrentDestinationPixelIndex / zig_size % 2)
            {
                // Odd "zig"
                int group = zig_size * (CurrentDestinationPixelIndex / zig_size);
                pSourceData = showBuffer + (NUM_INTENSITY_BYTES_PER_PIXEL * ((group + zig_size - (CurrentDestinationPixelIndex % zig_size) - 1) / group_size));
            } // end zig
            else
            {
                // Even "zag"
                pSourceData = showBuffer + (NUM_INTENSITY_BYTES_PER_PIXEL * (CurrentDestinationPixelIndex / group_size));
            } // end zag

            // now that we have decided on a data source, copy one 
            // pixels worth of data
            *pTargetData++ = gamma_table[pSourceData[rOffset]];
            *pTargetData++ = gamma_table[pSourceData[gOffset]];
            *pTargetData++ = gamma_table[pSourceData[bOffset]];

        } // end for each pixel in the output buffer
    } // end zig zag copy

        // set the intensity transmit buffer pointers.
    uart_buffer      = asyncdata;
    uart_buffer_tail = asyncdata + szBuffer;

#ifdef ARDUINO_ARCH_ESP8266
    SET_PERI_REG_MASK(UART_INT_ENA(1), UART_TXFIFO_EMPTY_INT_ENA);
#else
    ESP_ERROR_CHECK (uart_enable_tx_intr (UART, true, PIXEL_FIFO_TRIGGER_LEVEL));
#endif
    startTime = micros();
}