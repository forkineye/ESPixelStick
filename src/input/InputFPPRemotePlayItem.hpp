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
    c_InputFPPRemotePlayItem (String & NameOfPlayItem);
    ~c_InputFPPRemotePlayItem ();

    virtual void Start () = 0;
    virtual void Stop  () = 0;
    virtual void Sync  (time_t syncTime) = 0;
    virtual void Poll  () = 0;
    virtual void GetStatus (JsonObject& jsonStatus) = 0;

private:
    String PlayItemName;

};
