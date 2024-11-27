/*
* FastTimer.cpp - Output Management class
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

#include "FastTimer.hpp"

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
FastTimer::FastTimer ()
{
    CancelTimer();
} // FastTimer

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
FastTimer::~FastTimer ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~FastTimer

//-----------------------------------------------------------------------------
///< Start the module
void FastTimer::StartTimer (uint32_t _DurationMS, bool _continuous)
{
    // DEBUG_START;

    DurationMS = _DurationMS;
    Continuous = _continuous;

    uint64_t now = uint64_t(millis());

    EndTimeMS = now + uint64_t(DurationMS);
    OffsetMS = uint64_t((EndTimeMS > uint32_t(-1)) ? uint32_t(-1) : uint32_t(0));

    // DEBUG_V(String("DurationMS: ") + String(DurationMS));
    // DEBUG_V(String("continuous: ") + String(Continuous));
    // DEBUG_V(String("       now: ") + String(now));
    // DEBUG_V(String(" EndTimeMS: ") + String(EndTimeMS));
    // DEBUG_V(String("  OffsetMS: ") + String(OffsetMS));

    // DEBUG_END;
} // StartTimer

//-----------------------------------------------------------------------------
bool FastTimer::IsExpired ()
{
    bool Expired = ((uint64_t(millis ()) + uint64_t(OffsetMS)) >= EndTimeMS);

    // do we need to restart the timer?
    if(Expired && Continuous)
    {
        // adjust for next invocation
        EndTimeMS += DurationMS;
    }
    return Expired;

} // IsExpired

//-----------------------------------------------------------------------------
void FastTimer::CancelTimer()
{
    EndTimeMS  = 0;
    OffsetMS   = 0;
    DurationMS = 0;
    Continuous = false;
} // CancelTimer

//-----------------------------------------------------------------------------
uint32_t FastTimer::GetTimeRemaining()
{
    return (IsExpired()) ? 0 : uint32_t(EndTimeMS - (uint64_t(millis()) + uint64_t(OffsetMS)));

} // GetTimeRemaining
