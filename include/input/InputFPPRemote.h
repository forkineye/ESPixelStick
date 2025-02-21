#pragma once
/*
* InputFPPRemote.h
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
*/

#include "InputCommon.hpp"
#include "WebMgr.hpp"
#include "InputFPPRemotePlayItem.hpp"

class c_InputFPPRemote : public c_InputCommon
{
  public:

      c_InputFPPRemote (c_InputMgr::e_InputChannelIds NewInputChannelId,
                        c_InputMgr::e_InputType       NewChannelType,
                        uint32_t                        BufferSize);

      virtual ~c_InputFPPRemote ();

      // functions to be provided by the derived class
      void Begin ();                           ///< set up the operating environment based on the current config (or defaults)
      bool SetConfig (JsonObject& jsonConfig); ///< Set a new config in the driver
      void GetConfig (JsonObject& jsonConfig); ///< Get the current config used by the driver
      void GetStatus (JsonObject& jsonStatus);
      void Process   ();
      void GetDriverName (String& sDriverName) { sDriverName = "FPP Remote"; } ///< get the name for the instantiated driver
      void SetBufferInfo (uint32_t BufferSize);
      void ProcessButtonActions(c_ExternalInput::InputValue_t value);
      void SetOperationalState (bool ActiveFlag);

      void FppStartRemoteFilePlay (String & FileName, uint32_t ElapsedTimeSec);
      void FppStopRemoteFilePlay  ();
      void FppSyncRemoteFilePlay  (String & FileName, uint32_t ElapsedTimeMS);
      void GetFppRemotePlayStatus (JsonObject& jsonStatus);
      bool IsIdle();
      bool AllowedToPlayRemoteFile();
      void SetBackgroundFile      ();

protected:

    c_InputFPPRemotePlayItem * pInputFPPRemotePlayItem = nullptr;
    int32_t GetSyncOffsetMS () { return SyncOffsetMS; }
    bool    GetSendFppSync ()  { return SendFppSync; }

    String StatusType;

private:

    void validateConfiguration ();
    void StartPlaying (String & FileName);
    void StartPlayingLocalFile (String & FileName);
    void StartPlayingRemoteFile (String & FileName);
    void StopPlaying ();
    bool PlayingFile ();
    bool PlayingRemoteFile ();
    void PlayNextFile ();
    bool Poll ();

    void load ();          ///< Load configuration from File System
    void save ();          ///< Save configuration to File System

    int32_t SyncOffsetMS = 0;
    bool    SendFppSync = false;
    String  FileBeingPlayed = CN_No_LocalFileToPlay;
    String  ConfiguredFileToPlay = CN_No_LocalFileToPlay;
    bool    Stopping = false;
    bool    FppSyncOverride = false;
};
