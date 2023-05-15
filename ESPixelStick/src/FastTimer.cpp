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
void FastTimer::StartTimer (uint32_t DurationMS)
{
    // DEBUG_START;

    uint64_t now = uint64_t(millis());

    EndTimeMS = now + uint64_t(DurationMS);
    offsetMS = uint64_t((EndTimeMS > uint32_t(-1)) ? uint32_t(-1) : uint32_t(0));

    // DEBUG_END;
} // StartTimer

//-----------------------------------------------------------------------------
bool FastTimer::IsExpired ()
{
    return ((uint64_t(millis ()) + uint64_t(offsetMS)) >= EndTimeMS);

} // IsExpired

//-----------------------------------------------------------------------------
void FastTimer::CancelTimer()
{
    EndTimeMS = millis();
    offsetMS = 0;

} // CancelTimer
