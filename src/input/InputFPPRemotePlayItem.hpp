#pragma once
/*
* InputFPPRemotePlayItem.hpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021 Shelby Merrick
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
*   Common Play object use to parse and play a play list or file
*/

#include "../ESPixelStick.h"

class c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayItem ();
    ~c_InputFPPRemotePlayItem ();

    virtual void   Poll           (uint8_t * Buffer, size_t BufferSize) = 0;
    virtual void   Start          (String & FileName, uint32_t FrameId) = 0;
    virtual void   Stop           () = 0;
    virtual void   Sync           (uint32_t FrameId) = 0;
    virtual void   GetStatus      (JsonObject & jsonStatus) = 0;
    virtual bool   IsIdle         () = 0;
            String GetFileName    () { return PlayItemName; }
            void   SetPlayCount   (uint32_t value) { RemainingPlayCount = value; }
            uint32_t GetRepeatCount () { return RemainingPlayCount; }
            void   SetDuration    (time_t value) { PlayDurationSec = value; }

protected:
    String   PlayItemName;
    uint32_t RemainingPlayCount = 0;
    time_t   PlayDurationSec = 0;

}; // c_InputFPPRemotePlayItem
