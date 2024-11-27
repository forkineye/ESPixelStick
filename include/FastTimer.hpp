#pragma once
/*
* FastTimer.hpp
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

#include "ESPixelStick.h"

class FastTimer
{
public:
    FastTimer ();
    virtual ~FastTimer ();

    void StartTimer (uint32_t DurationMS, bool continuous);
    bool IsExpired();
    void CancelTimer();
    uint32_t GetTimeRemaining();

private:
    bool     Continuous = false;
    uint32_t DurationMS = 0;
    uint64_t EndTimeMS = 0;
    uint32_t OffsetMS = 0;

protected:

}; // FastTimer
