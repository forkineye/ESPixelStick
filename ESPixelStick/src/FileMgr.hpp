#pragma once
/*
* FileMgr.hpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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
#include <LittleFS.h>
#ifdef SUPPORT_SD_MMC
#   include <SD_MMC.h>
#else
#   include <FS.h>
#   include <SD.h>
#endif // def SUPPORT_SD_MMC
#include <map>
#include <vector>

#ifdef ARDUINO_ARCH_ESP32
#   ifdef SUPPORT_SD_MMC
#       define ESP_SD   SD_MMC
#	    define ESP_SDFS SD_MMC
#   else // !def SUPPORT_SD_MMC
#       define ESP_SD   SD
#	    define ESP_SDFS SD
#   endif // !def SUPPORT_SD_MMC
#else // !ARDUINO_ARCH_ESP32
#   define ESP_SD   SD
#   define ESP_SDFS SDFS
#endif // !ARDUINO_ARCH_ESP32

class c_FileMgr
{
public:
    c_FileMgr ();
    virtual ~c_FileMgr ();

    typedef uint32_t FileId;
    const static FileId INVALID_FILE_HANDLE = 0;

    void    Begin     ();
    void    Poll      () {}
    void    GetConfig (JsonObject& json);
    bool    SetConfig (JsonObject& json);
    void    GetStatus (JsonObject& json);

    void    handleFileUpload (const String & filename, size_t index, uint8_t * data, size_t len, bool final, uint32_t totalLen);

    typedef std::function<void (DynamicJsonDocument& json)> DeserializationHandler;

    typedef enum
    {
        FileRead = 0,
        FileWrite,
        FileAppend,
    } FileMode;

    void   DeleteFlashFile (const String & FileName);
    bool   SaveFlashFile   (const String & FileName, String & FileData);
    bool   SaveFlashFile   (const String & FileName, const char * FileData);
    bool   SaveFlashFile   (const String & FileName, JsonDocument & FileData);
    bool   SaveFlashFile   (const String   filename, uint32_t index, uint8_t *data, uint32_t len, bool final);

    bool   ReadFlashFile   (const String & FileName, String & FileData);
    bool   ReadFlashFile   (const String & FileName, JsonDocument & FileData);
    bool   ReadFlashFile   (const String & FileName, byte * FileData, size_t maxlen);
    bool   LoadFlashFile   (const String & FileName, DeserializationHandler Handler);
    bool   FlashFileExists (const String & FileName);

    bool   SdCardIsInstalled () { return SdCardInstalled; }
    FileId CreateSdFileHandle ();
    void   DeleteSdFile     (const String & FileName);
    void   SaveSdFile       (const String & FileName,   String & FileData);
    void   SaveSdFile       (const String & FileName,   JsonVariant & FileData);
    bool   OpenSdFile       (const String & FileName,   FileMode Mode, FileId & FileHandle);
    size_t ReadSdFile       (const FileId & FileHandle, byte * FileData, size_t NumBytesToRead);
    size_t ReadSdFile       (const FileId & FileHandle, byte * FileData, size_t NumBytesToRead,  size_t StartingPosition);
    bool   ReadSdFile       (const String & FileName,   String & FileData);
    bool   ReadSdFile       (const String & FileName,   JsonDocument & FileData);
    size_t WriteSdFile      (const FileId & FileHandle, byte * FileData, size_t NumBytesToWrite);
    size_t WriteSdFile      (const FileId & FileHandle, byte * FileData, size_t NumBytesToWrite, size_t StartingPosition);
    void   CloseSdFile      (FileId & FileHandle);
    void   GetListOfSdFiles (std::vector<String> & Response);
    size_t GetSdFileSize    (const String & FileName);
    size_t GetSdFileSize    (const FileId & FileHandle);
    void   BuildFseqList    ();
    
    void   GetDriverName    (String& Name) { Name = "FileMgr"; }

    // Configuration file params
#if defined ARDUINO_ARCH_ESP8266
#   // define CONFIG_MAX_SIZE (3*1024)    ///< Sanity limit for config file
#else
#   // define CONFIG_MAX_SIZE (4*1024)    ///< Sanity limit for config file
#endif
private:
    void   SetSpiIoPins ();
    void   ResetSdCard ();

#   define SD_CARD_CLK_MHZ     SD_SCK_MHZ(50)  // 50 MHz SPI clock

    void listDir (fs::FS& fs, String dirname, uint8_t levels);
    void DescribeSdCardToUser ();
    void handleFileUploadNewFile (const String & filename);
    void printDirectory (File dir, int numTabs);

    bool     SdCardInstalled = false;
    uint8_t  miso_pin = SD_CARD_MISO_PIN;
    uint8_t  mosi_pin = SD_CARD_MOSI_PIN;
    uint8_t  clk_pin  = SD_CARD_CLK_PIN;
    uint8_t  cs_pin   = SD_CARD_CS_PIN;
    FileId fsUploadFile;
    String   fsUploadFileName;
    bool     fsUploadFileSavedIsEnabled = false;
    uint32_t fsUploadStartTime;
    char     XlateFileMode[3] = { 'r', 'w', 'w' };

#define MaxOpenFiles 5
    struct FileListEntry_t
    {
        FileId  handle;
        File    info;
        size_t  size;
        int     entryId;
    };
    FileListEntry_t FileList[MaxOpenFiles];
    int FileListFindSdFileHandle (FileId HandleToFind);
    void InitSdFileList ();

    File        FileSendDir;
    uint32_t    LastFileSent = 0;
    uint32_t    expectedIndex = 0;

protected:

}; // c_FileMgr

extern c_FileMgr FileMgr;
