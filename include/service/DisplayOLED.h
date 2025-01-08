#pragma once
/*
 * DisplayOLED.h
 *
 * Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
 * Copyright (c) 2018, 2024 Shelby Merrick
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

 */

#include "ESPixelStick.h"

#ifdef SUPPORT_OLED

class c_OLED
{
private:
    String dispIP;
    String reason;
    String dispHostName;
    uint64_t dispRSSI;

public:
    
    c_OLED() {};
    virtual ~c_OLED() {}

    void Begin();
    void Loading();
    void Update();
    void GetDriverName(String &name) { name = "OLED"; }
};

extern c_OLED OLED;
#endif // def SUPPORT_OLED