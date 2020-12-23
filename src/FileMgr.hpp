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
#include <map>

#ifdef ARDUINO_ARCH_ESP32
#   include <LITTLEFS.h>
#	include <SD.h>
#	define SDFS SD
#else if defined(ARDUINO_ARCH_ESP8266)
#   include <LittleFS.h>
#	include <SDFS.h>
#   define LITTLEFS LittleFS
#endif

class c_FileMgr
{
public:
    c_FileMgr ();
    virtual ~c_FileMgr ();

    typedef uint32_t FileId;

    void   Begin            ();
    void   Poll             ();
    void   SetSpiIoPins     (uint8_t miso, uint8_t mosi, uint8_t clock, uint8_t cs);
    void   handleFileUpload (String filename, size_t index, uint8_t * data, size_t len, bool final);

    typedef std::function<void (DynamicJsonDocument& json)> DeserializationHandler;

    typedef enum
    {
        FileRead = 0,
        FileWrite,
        FileAppend,
    } FileMode;

    void   DeleteConfigFile (String & FileName);
    bool   SaveConfigFile   (String & FileName,   String & FileData);
    bool   SaveConfigFile   (String & FileName,   JsonVariant & FileData);
    bool   ReadConfigFile   (String & FileName,   String & FileData);
    bool   ReadConfigFile   (String & FileName,   JsonDocument & FileData);
    bool   LoadConfigFile   (String & FileName,   DeserializationHandler Handler);
    bool   LoadConfigFile   (String & FileName,   DeserializationHandler Handler, size_t JsonDocSize);

    bool   SdCardIsInstalled () { return SdCardInstalled; }
    void   DeleteSdFile     (String & FileName);
    void   SaveSdFile       (String & FileName,   String FileData);
    void   SaveSdFile       (String & FileName,   JsonVariant & FileData);
    bool   OpenSdFile       (String & FileName,   FileMode Mode, FileId & FileHandle);
    size_t ReadSdFile       (FileId & FileHandle, byte * FileData, size_t NumBytesToRead);
    size_t ReadSdFile       (FileId & FileHandle, byte * FileData, size_t NumBytesToRead,  size_t StartingPosition);
    size_t WriteSdFile      (FileId & FileHandle, byte * FileData, size_t NumBytesToWrite);
    size_t WriteSdFile      (FileId & FileHandle, byte * FileData, size_t NumBytesToWrite, size_t StartingPosition);
    void   CloseSdFile      (FileId & FileHandle);
    void   GetListOfSdFiles (String & Response);
    size_t GetSdFileSize    (FileId & FileHandle);

    // Configuration file params
#if defined ARDUINO_ARCH_ESP8266
#   // define CONFIG_MAX_SIZE (3*1024)    ///< Sanity limit for config file
#else
#   // define CONFIG_MAX_SIZE (4*1024)    ///< Sanity limit for config file
#endif
private:
#   define SD_CARD_MISO_PIN    19
#   define SD_CARD_MOSI_PIN    23
#   define SD_CARD_CLK_PIN     18
#   define SD_CARD_CS_PIN      4

    void listDir (fs::FS& fs, String dirname, uint8_t levels);
    void DescribeSdCardToUser ();
    void handleFileUploadNewFile (String & filename);

#ifdef ESP32
    void printDirectory (File dir, int numTabs);
#else
    void printDirectory (Dir dir, int numTabs);
#endif // def ESP32

    bool     SdCardInstalled = false;
    uint8_t  miso_pin = SD_CARD_MISO_PIN;
    uint8_t  mosi_pin = SD_CARD_MOSI_PIN;
    uint8_t  clk_pin  = SD_CARD_CLK_PIN;
    uint8_t  cs_pin   = SD_CARD_CS_PIN;
    FileId   fsUploadFile;
    String   fsUploadFileName;
    bool     fsUploadFileSavedIsEnabled = false;
    char     XlateFileMode[3] = { 'r', 'w', 'w' };

    std::map<uint32_t, File> FileList;

protected:

}; // c_FileMgr

extern c_FileMgr FileMgr;
