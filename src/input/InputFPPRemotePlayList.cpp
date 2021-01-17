/*
* InputFPPRemotePlayList.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2020 Shelby Merrick
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
*   Playlist object used to parse and play a playlist
*/

#include "InputFPPRemotePlayList.hpp"
#include "../service/FPPDiscovery.h"

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayList::c_InputFPPRemotePlayList (String & NameOfPlaylist) :
    c_InputFPPRemotePlayItem(NameOfPlaylist)
{
    // DEBUG_START;


    // DEBUG_END;
} // c_InputFPPRemotePlayList

//-----------------------------------------------------------------------------
c_InputFPPRemotePlayList::~c_InputFPPRemotePlayList ()
{

} // ~c_InputFPPRemotePlayList

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Start ()
{
    // DEBUG_START;


    // DEBUG_END;
} // Start

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Stop ()
{
    // DEBUG_START;


    // DEBUG_END;
} // Stop

//-----------------------------------------------------------------------------
void c_InputFPPRemotePlayList::Sync (time_t syncTime)
{
    // DEBUG_START;


    // DEBUG_END;
} // Sync
