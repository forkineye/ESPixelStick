/*
* InputFPPRemotePlayItem.cpp
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
*   Common Play object use to parse and play a play list or file
*/

#include "input/InputFPPRemotePlayItem.hpp"
#include <Int64String.h>

byte *LocalIntensityBuffer = nullptr;

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayItem::c_InputFPPRemotePlayItem (c_InputMgr::e_InputChannelIds _InputChannelId)
{
    // DEBUG_START;

    InputChannelId = _InputChannelId;

    // DEBUG_END;
} // c_InputFPPRemotePlayItem

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayItem::~c_InputFPPRemotePlayItem ()
{

} // ~c_InputFPPRemotePlayItem

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayItem::ClearFileNames()
{
    // DEBUG_START;

    BackgroundFileName = emptyString;
    FileControl[NextFile].FileName = emptyString;

    // DEBUG_END;
} // ClearFileNames
