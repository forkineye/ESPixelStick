#pragma once
/*
* FileMgr.hpp - Output Management class
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
*/

#include "ESPixelStick.h"
#include <FS.h>

#ifdef ARDUINO_ARCH_ESP32
#   include <LITTLEFS.h>
#else if defined(ARDUINO_ARCH_ESP8266)
#   include <LittleFS.h>
#endif

class c_FileMgr
{
public:
    c_FileMgr ();
    virtual ~c_FileMgr ();

    typedef uint8_t FileId;

    void   Begin            ();
    void   Poll             ();

    typedef std::function<void (DynamicJsonDocument& json)> DeserializationHandler;

    void   DeleteConfigFile (String & FileName);
    bool   SaveConfigFile   (String & FileName,   String & FileData);
    bool   SaveConfigFile   (String & FileName,   JsonVariant & FileData);
    bool   ReadConfigFile   (String & FileName,   String & FileData);
    bool   ReadConfigFile   (String & FileName,   JsonDocument & FileData);
    bool   LoadConfigFile   (String & FileName,   DeserializationHandler Handler);
    bool   LoadConfigFile   (String & FileName,   DeserializationHandler Handler, size_t JsonDocSize);

    void   DeleteSdFile     (String & FileName);
    void   SaveSdFile       (String & FileName,   String FileData);
    void   SaveSdFile       (String & FileName,   JsonVariant & FileData);
    bool   OpenSdFile       (String & FileName,   FileId & FileHandle);
    size_t ReadSdFile       (FileId & FileHandle, byte * FileData, size_t maxBytes);
    void   CloseSdFile      (FileId & FileHandle);

    // Configuration file params
#if defined ARDUINO_ARCH_ESP8266
#   // define CONFIG_MAX_SIZE (3*1024)    ///< Sanity limit for config file
#else
#   // define CONFIG_MAX_SIZE (4*1024)    ///< Sanity limit for config file
#endif
private:

    void listDir (fs::FS& fs, String dirname, uint8_t levels);

    config_t * config = nullptr;                           // Current configuration

protected:

}; // c_FileMgr

extern c_FileMgr FileMgr;
