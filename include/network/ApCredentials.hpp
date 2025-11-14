#pragma once
/*
* ApCredentials.hpp - Output Management class
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

#include "ESPixelStick.h"

class c_ApCredentials
{
public:
    c_ApCredentials ();
    virtual ~c_ApCredentials ();

    void GetConfig (JsonObject & json);
    bool SetConfig (JsonObject & json);

    struct ApCredentials
    {
        char ssid[65];
        char passphrase[65];
    };
    bool    GetCurrentCredentials (ApCredentials & OutValue);
    void    ResetCurrentCredentials ();
    bool    GetNextCredentials ();
    void    GetDriverName (String & Name) { Name = F("AP Credentials"); }

private:
#   define MAX_NUM_CREDENTIALS 4
    ApCredentials _ApCredentials[MAX_NUM_CREDENTIALS];
    uint ApCredentialsIterator = MAX_NUM_CREDENTIALS;

}; // c_ApCredentials
