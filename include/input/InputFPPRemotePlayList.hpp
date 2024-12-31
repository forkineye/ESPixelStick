#pragma once
/*
* InputFPPRemotePlayList.hpp
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
*   Playlist object use to parse and play a playlist
*/

#include "ESPixelStick.h"
#include "InputFPPRemotePlayItem.hpp"
#include "InputFPPRemotePlayListFsm.hpp"
#include "FileMgr.hpp"

/*****************************************************************************/
class c_InputFPPRemotePlayList : public c_InputFPPRemotePlayItem
{
public:
    c_InputFPPRemotePlayList (c_InputMgr::e_InputChannelIds InputChannelId);
    virtual ~c_InputFPPRemotePlayList ();

    virtual void Start (String & FileName, float SecondsElapsed, uint32_t PlayCount);
    virtual void Stop  ();
    virtual void Sync  (String & FileName, float SecondsElapsed);
    virtual bool Poll  ();
    virtual void GetStatus (JsonObject & jsonStatus);
    virtual bool IsIdle () { return (pCurrentFsmState == &fsm_PlayList_state_Idle_imp); }

private:

protected:
    friend class fsm_PlayList_state_WaitForStart;
    friend class fsm_PlayList_state_Idle;
    friend class fsm_PlayList_state_PlayingFile;
    friend class fsm_PlayList_state_PlayingEffect;
    friend class fsm_PlayList_state_Paused;
    friend class fsm_PlayList_state;

    fsm_PlayList_state_WaitForStart  fsm_PlayList_state_WaitForStart_imp;
    fsm_PlayList_state_Idle          fsm_PlayList_state_Idle_imp;
    fsm_PlayList_state_PlayingFile   fsm_PlayList_state_PlayingFile_imp;
    fsm_PlayList_state_PlayingEffect fsm_PlayList_state_PlayingEffect_imp;
    fsm_PlayList_state_Paused        fsm_PlayList_state_Paused_imp;

    fsm_PlayList_state * pCurrentFsmState = nullptr;

    c_InputFPPRemotePlayItem * pInputFPPRemotePlayItem = nullptr;

    uint32_t PlayListEntryId     = 0;

    // BUGBUG -- time_t creates issues for portable code, and for overflow-safe code
    // time_t is only required to be a "real" type, which means it can be either a float or an integer.
    // even when time_t is an integer type, it can be signed or unsigned
    //
    // Current code appears to assume that time_t is not a float.
    //
    // C++11 does not expose any constant for minimum / maximum value for the time_t type.
    // This makes it impossible (without type traits) to ensure math operations on time_t types do
    // not result in undefined behavior (signed types only) and/or overflow (all types).
    //
    // Therefore, it is highly recommended to move AWAY from use of time_t wherever possible,
    // and instead use types that expose their minimum and maximum values, respectively.
    //
    // For now, because the code presumes time_t is an integer type, the following assertion
    // ensures this for all practical purposes.
    static_assert( (((time_t)1) / 2) == 0 ); // Verify time_t is an integer type (alternative: float)

    FastTimer PauseDelayTimer;
    uint32_t PlayListRepeatCount = 1;

    bool ProcessPlayListEntry ();

}; // c_InputFPPRemotePlayList
