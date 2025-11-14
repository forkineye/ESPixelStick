/*
* ApCredentials.cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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

#include "network/ApCredentials.hpp"

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_ApCredentials::c_ApCredentials ()
{
    // DEBUG_START;

    memset(_ApCredentials, 0x00, sizeof(_ApCredentials));
    ResetCurrentCredentials();

    // DEBUG_END;
} // c_ApCredentials

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_ApCredentials::~c_ApCredentials()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_ApCredentials

//-----------------------------------------------------------------------------
void c_ApCredentials::GetConfig (JsonObject& json)
{
    // DEBUG_START;

    JsonArray ApCredentialsArray = json.createNestedArray(CN_ApCredentials);
    for(auto & currentCredentials : _ApCredentials)
    {
        JsonObject NewArrayEntry = ApCredentialsArray.createNestedObject();
        JsonWrite(NewArrayEntry, CN_ssid,       currentCredentials.ssid);
        JsonWrite(NewArrayEntry, CN_passphrase, currentCredentials.passphrase);
    }

    PrettyPrint(json, "Credentials Config");

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
bool c_ApCredentials::SetConfig (JsonObject & json)
{
    // DEBUG_START;

    bool ConfigChanged = false;

    do // once
    {
        // is this an old style config?
        if(json.containsKey(CN_ssid))
        {
            // DEBUG_V("Old Style Config");
            memset(_ApCredentials, 0x00, sizeof(_ApCredentials));
            ConfigChanged |= setFromJSON (_ApCredentials[0].ssid,       json, CN_ssid);
            ConfigChanged |= setFromJSON (_ApCredentials[0].passphrase, json, CN_passphrase);
            break;
        }

        // DEBUG_V("New style config");
        JsonArray CredentialsArray = json[CN_ApCredentials];
        // adjust the number of entries in the vector
        // DEBUG_V(String("MAX_NUM_CREDENTIALS: ") + String(MAX_NUM_CREDENTIALS));
        // DEBUG_V(String("CredentialsArray.size: ") + String(CredentialsArray.size()));
        if(CredentialsArray.size() != MAX_NUM_CREDENTIALS)
        {
            logcon("ERROR: AP Credentials Config data is not valid.");
            break;
        }

        uint CredentialsIndex = 0;
        for(JsonObject CurrentCredentials : CredentialsArray)
        {
            ConfigChanged |= setFromJSON (_ApCredentials[CredentialsIndex].ssid,       CurrentCredentials, CN_ssid);
            ConfigChanged |= setFromJSON (_ApCredentials[CredentialsIndex].passphrase, CurrentCredentials, CN_passphrase);

            // DEBUG_V(String("CredentialsIndex: ") + String(CredentialsIndex));
            // DEBUG_V(String("            ssid: ") + String(_ApCredentials[CredentialsIndex].ssid));
            // DEBUG_V(String("      passphrase: ") + String(_ApCredentials[CredentialsIndex].passphrase));

            if((String(_ApCredentials[CredentialsIndex].passphrase).length() < 8) && (String(_ApCredentials[CredentialsIndex].passphrase).length() > 0))
            {
                logcon (String (F ("WiFi Passphrase '")) + _ApCredentials[CredentialsIndex].passphrase + F("' is too short. Using Empty String"));
                memset(_ApCredentials[CredentialsIndex].passphrase,0x00,sizeof(_ApCredentials[CredentialsIndex].passphrase));
            }
            CredentialsIndex++;
        }
    } while (false);

    // DEBUG_V (String("ConfigChanged: ") + String(ConfigChanged));
    if(ConfigChanged)
    {
        ResetCurrentCredentials();
    }

    // DEBUG_END;
    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
bool c_ApCredentials::GetCurrentCredentials (ApCredentials & OutValue)
{
    // DEBUG_START;
    bool Response = false;

    // DEBUG_V(String("CredentialsIndex: ") + String(ApCredentialsIterator));
    if(ApCredentialsIterator < MAX_NUM_CREDENTIALS)
    {
        // DEBUG_V(String("            ssid: ") + String(_ApCredentials[ApCredentialsIterator].ssid));
        // DEBUG_V(String("      passphrase: ") + String(_ApCredentials[ApCredentialsIterator].passphrase));
        OutValue = _ApCredentials[ApCredentialsIterator];
        Response = true;
    }
    else
    {
        // DEBUG_V("No credentials to send");
    }

    // DEBUG_END;
    return Response;

} // GetCurrentCredentials

//-----------------------------------------------------------------------------
bool c_ApCredentials::GetNextCredentials ()
{
    // DEBUG_START;

    ApCredentialsIterator++;

    // DEBUG_END;
    return ApCredentialsIterator < MAX_NUM_CREDENTIALS;

} // GetNextCredentials

//-----------------------------------------------------------------------------
void c_ApCredentials::ResetCurrentCredentials ()
{
    // DEBUG_START;

    ApCredentialsIterator = 0;

    // DEBUG_END;

} // ResetCurrentCredentials
