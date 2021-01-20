/*
* InputFPPRemotePlayEffect.cpp
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
*   PlayFile object used to parse and play an FSEQ File
*/

#include "InputFPPRemotePlayEffect.hpp"

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayEffect::c_InputFPPRemotePlayEffect () :
    c_InputFPPRemotePlayItem ()
{
    // DEBUG_START;


    // DEBUG_END;
} // c_InputFPPRemotePlayEffect

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayEffect::~c_InputFPPRemotePlayEffect ()
{

} // ~c_InputFPPRemotePlayEffect

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::Start (String & FileName, uint32_t FrameId)
{
    // DEBUG_START;

    pCurrentFsmState->Start (FileName, FrameId);

    // DEBUG_END;
} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::Stop ()
{
    // DEBUG_START;


    // DEBUG_END;
} // Stop

//-----------------------------------------------------------------------------
bool c_InputFPPRemotePlayEffect::Sync (uint32_t FrameId)
{
    // DEBUG_START;


    // DEBUG_END;
} // Sync

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::Poll (uint8_t * Buffer, size_t BufferSize)
{
    // DEBUG_START;

    // DEBUG_END;

} // Poll

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayEffect::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    // DEBUG_END;

} // GetStatus
