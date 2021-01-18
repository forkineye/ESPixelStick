#pragma once
/*
* InputFPPRemotePlayListFsm.hpp
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
*   Playlist object use to parse and play a playlist
*/

#include "../ESPixelStick.h"

class c_InputFPPRemotePlayList;

/*****************************************************************************/
/*
*	Generic fsm base class.
*/
/*****************************************************************************/
/*****************************************************************************/
class fsm_PlayList_state
{
public:
    virtual void Poll (void) = 0;
    virtual void Init (c_InputFPPRemotePlayList * Parent) = 0;
    virtual void GetStateName (String & sName) = 0;
    virtual void Start (void) = 0;
    virtual void Stop (void) = 0;
    virtual void Sync (time_t syncTime) = 0;

protected:
    c_InputFPPRemotePlayList * pInputFPPRemotePlayList;

}; // fsm_PlayList_state

/*****************************************************************************/
class fsm_PlayList_state_Idle : fsm_PlayList_state
{
public:
    virtual void Poll (void);
    virtual void Init (c_InputFPPRemotePlayList* Parent);
    virtual void GetStateName (String& sName) { sName = F ("Idle"); }
    virtual void Start (void);
    virtual void Stop (void);
    virtual void Sync (time_t syncTime);

}; // fsm_PlayList_state_Idle

/*****************************************************************************/
class fsm_PlayList_state_PlayingFile : fsm_PlayList_state
{
public:
    virtual void Poll (void);
    virtual void Init (c_InputFPPRemotePlayList* Parent);
    virtual void GetStateName (String& sName) { sName = F ("File"); }
    virtual void Start (void);
    virtual void Stop (void);
    virtual void Sync (time_t syncTime);

}; // fsm_PlayList_state_PlayingFile

/*****************************************************************************/
class fsm_PlayList_state_PlayingEffect : fsm_PlayList_state
{
public:
    virtual void Poll (void);
    virtual void Init (c_InputFPPRemotePlayList* Parent);
    virtual void GetStateName (String& sName) { sName = F ("Effect"); }
    virtual void Start (void);
    virtual void Stop (void);
    virtual void Sync (time_t syncTime);

}; // fsm_PlayList_state_PlayingEffect

/*****************************************************************************/
class fsm_PlayList_state_Paused : fsm_PlayList_state
{
public:
    virtual void Poll (void);
    virtual void Init (c_InputFPPRemotePlayList* Parent);
    virtual void GetStateName (String& sName) { sName = F ("Paused"); }
    virtual void Start (void);
    virtual void Stop (void);
    virtual void Sync (time_t syncTime);

}; // fsm_PlayList_state_Paused





