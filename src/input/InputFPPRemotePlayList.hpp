#pragma once
/*
* InputFPPRemotePlayList.hpp
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
*   Playlist object use to parse and play a playlist
*/

#include "../espixelstick.h"
#include "InputFPPRemotePlayItem.hpp"

class c_InputFPPRemotePlayList : c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayList (String & NameOfPlaylist);
    ~c_InputFPPRemotePlayList ();

    virtual void Start ();
    virtual void Stop  ();
    virtual void Sync  (time_t syncTime);

private:

}; // c_InputFPPRemotePlayList
