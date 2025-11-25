/*
* MainRelayMgr.cpp - Main Relay Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2025 Shelby Merrick
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
*   This class manages a main relay output that can be toggled on when
*   data is being received and off when no data is present.
*
*/

#include "ESPixelStick.h"
#include "output/MainRelayMgr.hpp"

#ifdef SUPPORT_MAIN_RELAY

//-----------------------------------------------------------------------------
c_MainRelayMgr::c_MainRelayMgr()
{
    // Constructor - initialized in member initializer list
}

//-----------------------------------------------------------------------------
c_MainRelayMgr::~c_MainRelayMgr()
{
    // Destructor - clean up GPIO if initialized
    if (HasBeenInitialized && GPIO_IS_VALID_OUTPUT_GPIO(RelayGpio))
    {
        // Set relay to OFF state before cleanup
        digitalWrite(RelayGpio, RelayInvert ? HIGH : LOW);
        ResetGpio(RelayGpio);
    }
}

//-----------------------------------------------------------------------------
void c_MainRelayMgr::Begin()
{
    // DEBUG_START;

    // Only initialize if we have a valid GPIO configured
    if (GPIO_IS_VALID_OUTPUT_GPIO(RelayGpio))
    {
        pinMode(RelayGpio, OUTPUT);
        // Set to OFF state (respecting invert setting)
        digitalWrite(RelayGpio, RelayInvert ? HIGH : LOW);
        logcon(String(F("Main Relay initialized on pin ")) + String(RelayGpio) +
               String(RelayInvert ? F(" (inverted)") : F(" (normal)")));
        HasBeenInitialized = true;
    }

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_MainRelayMgr::GetConfig(JsonObject& JsonData)
{
    // DEBUG_START;

    JsonWrite(JsonData, CN_main_relay_gpio, int(RelayGpio));
    JsonWrite(JsonData, CN_main_relay_invert, RelayInvert);

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_MainRelayMgr::SetConfig(JsonObject& JsonData)
{
    // DEBUG_START;

    bool ConfigChanged = false;

    // Read GPIO configuration
    int TempRelayGpio = int(RelayGpio);
    ConfigChanged |= setFromJSON(TempRelayGpio, JsonData, CN_main_relay_gpio);
    gpio_num_t NewRelayGpio = gpio_num_t(TempRelayGpio);

    // Read invert configuration
    bool NewRelayInvert = RelayInvert;
    ConfigChanged |= setFromJSON(NewRelayInvert, JsonData, CN_main_relay_invert);

    // If configuration changed, reinitialize
    if (ConfigChanged)
    {
        // Clean up old GPIO if it was initialized
        if (HasBeenInitialized && GPIO_IS_VALID_OUTPUT_GPIO(RelayGpio))
        {
            digitalWrite(RelayGpio, RelayInvert ? HIGH : LOW); // Turn OFF
            ResetGpio(RelayGpio);
        }

        // Update configuration
        RelayGpio = NewRelayGpio;
        RelayInvert = NewRelayInvert;
        HasBeenInitialized = false;

        // Initialize with new configuration
        Begin();
    }

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_MainRelayMgr::SetRelayState(bool Active)
{
    // Only control relay if we have a valid GPIO configured
    if (GPIO_IS_VALID_OUTPUT_GPIO(RelayGpio))
    {
        // Active = true means turn relay ON (data present)
        // Active = false means turn relay OFF (no data)
        // Respect invert setting
        if (Active)
        {
            digitalWrite(RelayGpio, RelayInvert ? LOW : HIGH);
        }
        else
        {
            digitalWrite(RelayGpio, RelayInvert ? HIGH : LOW);
        }
    }
}

//-----------------------------------------------------------------------------
// Create global instance
c_MainRelayMgr MainRelayMgr;

#endif // SUPPORT_MAIN_RELAY
