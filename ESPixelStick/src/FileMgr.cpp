/*
* FileMgr.cpp - Output Management class
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
#include <Int64String.h>

#include "FileMgr.hpp"

#ifdef ARDUINO_ARCH_ESP32
#   define NumBlocksToBuffer        7
#else
#   define NumBlocksToBuffer        9
#endif

#include <SimpleFTPServer.h>

/*
#include "ESP-FTP-Server-Lib.h"
#include "FTPFilesystem.h"
*/
// #include <FTPServer.h>

FtpServer   ftpSrv;
// FTPServer   ftpSrv(ESP_SDFS);

const char XlateFileMode[3][2] = { FILE_READ, FILE_WRITE, FILE_APPEND };

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_FileMgr::c_FileMgr ()
{
} // c_FileMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_FileMgr::~c_FileMgr ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_FileMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_FileMgr::Begin ()
{
    // DEBUG_START;

    do // once
    {
        InitSdFileList ();

        if (!LittleFS.begin ())
        {
            logcon ( String(CN_stars) + F (" Flash file system did not initialize correctly ") + CN_stars);
        }
        else
        {
#ifdef ARDUINO_ARCH_ESP32
            logcon (String (F ("Flash file system initialized. Used = ")) + String (LittleFS.usedBytes ()) + String (F (" out of ")) + String (LittleFS.totalBytes()) );
#else
            logcon (String (F ("Flash file system initialized.")));
#endif // def ARDUINO_ARCH_ESP32

            listDir (LittleFS, String ("/"), 3);
        }

        SetSpiIoPins ();

    } while (false);

    // DEBUG_END;
} // begin

//-----------------------------------------------------------------------------
void c_FileMgr::NetworkStateChanged (bool NewState)
{
    // DEBUG_START;
    if(NewState)
    {
        // DEBUG_V("Start FTP");
        ftpSrv.begin("esps","esps", "ESPS FTP");

/*
        ftpSrv.addUser("esps", "esps");
        ftpSrv.addFilesystem("SD", &ESP_SDFS);
        ftpSrv.begin();
*/
/*
        ftpSrv.begin(F("esps"), F("esps")); //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
*/
    }
    else
    {
        // DEBUG_V("Stop FTP");
        ftpSrv.end();
        // ftpSrv.stop();
    }
    // DEBUG_END;
} // NetworkStateChanged

//-----------------------------------------------------------------------------
 void c_FileMgr::Poll()
 {
    // DEBUG_START;
    ftpSrv.handleFTP();
    // ftpSrv.handle();
    // DEBUG_END;
 } // Poll

//-----------------------------------------------------------------------------
bool c_FileMgr::SetConfig (JsonObject & json)
{
    // DEBUG_START;

    bool ConfigChanged = false;
    if (json.containsKey (CN_device))
    {
        JsonObject JsonDeviceConfig = json[CN_device];
/*
        extern void PrettyPrint (JsonObject& jsonStuff, String Name);
        PrettyPrint (JsonDeviceConfig, "c_FileMgr");

        // DEBUG_V("miso_pin: " + String(miso_pin));
        // DEBUG_V("mosi_pin: " + String(mosi_pin));
        // DEBUG_V(" clk_pin: " + String(clk_pin));
        // DEBUG_V("  cs_pin: " + String(cs_pin));
*/
        ConfigChanged |= setFromJSON (miso_pin, JsonDeviceConfig, CN_miso_pin);
        ConfigChanged |= setFromJSON (mosi_pin, JsonDeviceConfig, CN_mosi_pin);
        ConfigChanged |= setFromJSON (clk_pin,  JsonDeviceConfig, CN_clock_pin);
        ConfigChanged |= setFromJSON (cs_pin,   JsonDeviceConfig, CN_cs_pin);
/*
        // DEBUG_V("miso_pin: " + String(miso_pin));
        // DEBUG_V("mosi_pin: " + String(mosi_pin));
        // DEBUG_V(" clk_pin: " + String(clk_pin));
        // DEBUG_V("  cs_pin: " + String(cs_pin));

        // DEBUG_V("ConfigChanged: " + String(ConfigChanged));
*/
    }
    else
    {
        logcon (F ("No File Manager settings found."));
    }

    // DEBUG_V (String ("ConfigChanged: ") + String (ConfigChanged));

    if (ConfigChanged)
    {
        SetSpiIoPins ();
    }

    // DEBUG_END;

    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_FileMgr::GetConfig (JsonObject& json)
{
    // DEBUG_START;

    json[CN_miso_pin]  = miso_pin;
    json[CN_mosi_pin]  = mosi_pin;
    json[CN_clock_pin] = clk_pin;
    json[CN_cs_pin]    = cs_pin;

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_FileMgr::GetStatus (JsonObject& json)
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP32
    json[F ("size")] = LittleFS.totalBytes ();
    json[F ("used")] = LittleFS.usedBytes ();
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_FileMgr::SetSpiIoPins ()
{
    // DEBUG_START;
#if defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)
    if (SdCardInstalled)
    {
        // DEBUG_V("Terminate current SD session");
        ESP_SD.end ();
    }
#ifdef ARDUINO_ARCH_ESP32
    // DEBUG_V();
    try
#endif // def ARDUINO_ARCH_ESP32
    {
#ifdef SUPPORT_SD_MMC
        // DEBUG_V (String ("  Data 0: ") + String (SD_CARD_DATA_0));
        // DEBUG_V (String ("  Data 1: ") + String (SD_CARD_DATA_1));
        // DEBUG_V (String ("  Data 2: ") + String (SD_CARD_DATA_2));
        // DEBUG_V (String ("  Data 3: ") + String (SD_CARD_DATA_3));

        pinMode(SD_CARD_DATA_0, PULLUP);
        pinMode(SD_CARD_DATA_1, PULLUP);
        pinMode(SD_CARD_DATA_2, PULLUP);
        pinMode(SD_CARD_DATA_3, PULLUP);

        if(!ESP_SD.begin())
#else // ! SUPPORT_SD_MMC
#   ifdef ARDUINO_ARCH_ESP32
        // DEBUG_V (String ("miso_pin: ") + String (miso_pin));
        // DEBUG_V (String ("mosi_pin: ") + String (mosi_pin));
        // DEBUG_V (String (" clk_pin: ") + String (clk_pin));
        // DEBUG_V (String ("  cs_pin: ") + String (cs_pin));

        // DEBUG_V();
        SPI.end ();
        // DEBUG_V();
        pinMode(cs_pin, OUTPUT);
#       ifdef USE_MISO_PULLUP
        // DEBUG_V("USE_MISO_PULLUP");
        // on some hardware MISO is missing a required pull-up resistor, use internal pull-up.
        pinMode(miso_pin, INPUT_PULLUP);
#       else
        // DEBUG_V();
        pinMode(miso_pin, INPUT);
#       endif // def USE_MISO_PULLUP

        // DEBUG_V();
        SPI.begin (clk_pin, miso_pin, mosi_pin, cs_pin);
        // DEBUG_V();
        ResetSdCard();

        // DEBUG_V();
        if (!ESP_SD.begin (cs_pin))
#   else  // ! ARDUINO_ARCH_ESP32
        if (!ESP_SD.begin (SD_CARD_CS_PIN, SD_CARD_CLK_MHZ))
#   endif // ! ARDUINO_ARCH_ESP32
#endif // !def SUPPORT_SD_MMC
        {
            // DEBUG_V();
            logcon(String(F("No SD card installed")));
            SdCardInstalled = false;
        }
        else
        {
            // DEBUG_V();
            SdCardInstalled = true;
            DescribeSdCardToUser ();
            BuildFseqList();
        }
    }
#ifdef ARDUINO_ARCH_ESP32
    catch (const std::exception &e)
    {
        logcon (String (F ("ERROR: Could not init the SD Card: "))+ e.what());
        SdCardInstalled = false;
    }
#endif // def ARDUINO_ARCH_ESP32

#else // no sd defined
    SdCardInstalled = false;
#endif // defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)

    // DEBUG_END;

} // SetSpiIoPins

//-----------------------------------------------------------------------------
/*
    On occasion, the SD card is left in an unknown state that causes the SD card
    to appear to not be installed. A power cycle fixes the issue. Clocking the
    bus also causes any incomplete transaction to get cleared.
*/
void c_FileMgr::ResetSdCard()
{
    // DEBUG_START;

    // send 0xff bytes until we get 0xff back
    byte ResetValue = 0x00;
    digitalWrite(cs_pin, LOW);
    SPI.beginTransaction(SPISettings());
    // DEBUG_V();
    uint32_t retry = 2000;
    while((0xff != ResetValue) && (--retry))
    {
        delay(1);
        ResetValue = SPI.transfer(0xff);
    }
    SPI.endTransaction();
    digitalWrite(cs_pin, HIGH);

    // DEBUG_END;
} // ResetSdCard

//-----------------------------------------------------------------------------
void c_FileMgr::DeleteFlashFile (const String& FileName)
{
    // DEBUG_START;

    LittleFS.remove (FileName);

    // DEBUG_END;

} // DeleteConfigFile

//-----------------------------------------------------------------------------
void c_FileMgr::listDir (fs::FS& fs, String dirname, uint8_t levels)
{
    // DEBUG_START;
    do // once
    {
        logcon (String (F ("Listing directory: ")) + dirname);

        File root = fs.open (dirname, CN_r);
        if (!root)
        {
            logcon (String (CN_stars) + F ("failed to open directory: ") + dirname + CN_stars);
            break;
        }

        if (!root.isDirectory ())
        {
            logcon (String (F ("Is not a directory: ")) + dirname);
            break;
        }

        File MyFile = root.openNextFile ();

        while (MyFile)
        {
            if (MyFile.isDirectory ())
            {
                if (levels)
                {
                    listDir (fs, dirname + "/" + MyFile.name (), levels - 1);
                }
            }
            else
            {
                logcon ("'" + String (MyFile.name ()) + "': \t'" + String (MyFile.size ()) + "'");
            }
            MyFile = root.openNextFile ();
        }

    } while (false);

} // listDir

//-----------------------------------------------------------------------------
bool c_FileMgr::LoadFlashFile (const String& FileName, DeserializationHandler Handler)
{
    // DEBUG_START;

    bool retval = false;

    do // once
    {
        String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";

        // DEBUG_V ("allocate the JSON Doc");
/*
        String RawFileData;
        if (false == ReadConfigFile (FileName, RawFileData))
        {
            logcon (String(CN_stars) + CfgFileMessagePrefix + F ("Could not read file.") + CN_stars);
            break;
        }
        logcon(RawFileData);
*/
        // DEBUG_V();
        fs::File file = LittleFS.open (FileName.c_str (), "r");
        // DEBUG_V();
        if (!file)
        {
            if (!IsBooting)
            {
                logcon (String (CN_stars) + CfgFileMessagePrefix + String (F (" Could not open file for reading ")) + CN_stars);
            }
            break;
        }

        size_t JsonDocSize = file.size () * 3;
        // DEBUG_V(String("Allocate JSON document. Size = ") + String(JsonDocSize));
        // DEBUG_V(String("Heap: ") + ESP.getFreeHeap());
        DynamicJsonDocument jsonDoc(JsonDocSize);
        // DEBUG_V(String("jsonDoc.capacity: ") + String(jsonDoc.capacity()));

        // did we get enough memory?
        if(jsonDoc.capacity() < JsonDocSize)
        {
            logcon (String(CN_stars) + CfgFileMessagePrefix + String (F ("Could Not Allocate enough memory to process config ")) + CN_stars);

            // DEBUG_V(String("Allocate JSON document. Size = ") + String(JsonDocSize));
            // DEBUG_V(String("Heap: ") + ESP.getFreeHeap());
            // DEBUG_V(String("jsonDoc.capacity: ") + String(jsonDoc.capacity()));
            file.close ();
            break;
        }

        // DEBUG_V ("Convert File to JSON document");
        DeserializationError error = deserializeJson (jsonDoc, file);
        file.close ();

        // DEBUG_V ("Error Check");
        if (error)
        {
            // logcon (CN_Heap_colon + String (ESP.getMaxFreeBlockSize ()));
            logcon (String(CN_stars) + CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str () + CN_stars);
            // logcon (CN_plussigns + RawFileData + CN_minussigns);
	        // DEBUG_V (String ("                heap: ") + String (ESP.getFreeHeap ()));
    	    /// DEBUG_V (String (" getMaxFreeBlockSize: ") + String (ESP.getMaxFreeBlockSize ()));
        	// DEBUG_V (String ("           file.size: ") + String (file.size ()));
	        // DEBUG_V (String ("Expected JsonDocSize: ") + String (JsonDocSize));
    	    // DEBUG_V (String ("    jsonDoc.capacity: ") + String (jsonDoc.capacity ()));
            break;
        }

        // extern void PrettyPrint(DynamicJsonDocument & jsonStuff, String Name);
        // PrettyPrint(jsonDoc, CfgFileMessagePrefix);

        // DEBUG_V ();
        jsonDoc.garbageCollect ();

        logcon (CfgFileMessagePrefix + String (F ("loaded.")));

        // DEBUG_V ();
        Handler (jsonDoc);

        // DEBUG_V();
        retval = true;

    } while (false);

    // DEBUG_END;
    return retval;

} // LoadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile (const String& FileName, String& FileData)
{
    // DEBUG_START;

    bool Response = SaveFlashFile (FileName, FileData.c_str ());

    // DEBUG_END;
    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile (const String& FileName, const char * FileData)
{
    // DEBUG_START;

    bool Response = false;
    String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
    // DEBUG_V (FileData);

    fs::File file = LittleFS.open (FileName.c_str (), "w");
    if (!file)
    {
        logcon (String (CN_stars) + CfgFileMessagePrefix + String (F ("Could not open file for writing..")) + CN_stars);
    }
    else
    {
        file.seek (0, SeekSet);
        file.print (FileData);
        file.close ();

        file = LittleFS.open (FileName.c_str (), "r");
        logcon (CfgFileMessagePrefix + String (F ("saved ")) + String (file.size ()) + F (" bytes."));
        file.close ();

        Response = true;
    }

    // DEBUG_END;
    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile(const String &FileName, JsonDocument &FileData)
{
    // DEBUG_START;
    bool Response = false;

    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";
    // DEBUG_V(String("CfgFileMessagePrefix: ") + CfgFileMessagePrefix);

    // serializeJson(FileData, LOG_PORT);
    // DEBUG_V("");

    // delay(100);
    // DEBUG_V("");

    fs::File file = LittleFS.open(FileName.c_str(), "w");
    // DEBUG_V("");

    if (!file)
    {
        logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Could not open file for writing..")) + CN_stars);
    }
    else
    {
        // DEBUG_V("");
        file.seek(0, SeekSet);
        // DEBUG_V("");

        size_t NumBytesSaved = serializeJson(FileData, file);
        // DEBUG_V("");

        file.close();

        logcon(CfgFileMessagePrefix + String(F("saved ")) + String(NumBytesSaved) + F(" bytes."));

        Response = true;
    }

    // DEBUG_END;

    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile(const String FileName, uint32_t index, uint8_t *data, uint32_t len, bool final)
{
    // DEBUG_START;
    bool Response = false;

    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";
    // DEBUG_V(String("CfgFileMessagePrefix: ") + CfgFileMessagePrefix);
    // DEBUG_V(String("            FileName: ") + FileName);
    // DEBUG_V(String("               index: ") + String(index));
    // DEBUG_V(String("                 len: ") + String(len));
    // DEBUG_V(String("               final: ") + String(final));

    fs::File file;
    
    if(0 == index) 
    { 
        file = LittleFS.open(FileName.c_str(), "w");
    }
    else
    {
        file = LittleFS.open(FileName.c_str(), "a");
    }
    // DEBUG_V("");

    if (!file)
    {
        logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Could not open file for writing..")) + CN_stars);
    }
    else
    {
        // DEBUG_V("");
        file.seek(int(index), SeekMode::SeekSet);
        // DEBUG_V("");

        size_t NumBytesSaved = file.write(data, size_t(len));
        // DEBUG_V("");

        file.close();

        logcon(CfgFileMessagePrefix + String(F("saved ")) + String(NumBytesSaved) + F(" bytes."));

        Response = true;
    }

    // DEBUG_END;

    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadFlashFile (const String& FileName, String& FileData)
{
    // DEBUG_START;

    bool GotFileData = false;
    String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";

    // DEBUG_V (String("File '") + FileName + "' is being opened.");
    fs::File file = LittleFS.open (FileName.c_str (), CN_r);
    if (file)
    {
        // Suppress this for now, may add it back later
        //logcon (CfgFileMessagePrefix + String (F ("reading ")) + String (file.size()) + F (" bytes."));

        // DEBUG_V (String("File '") + FileName + "' is open.");
        file.seek (0, SeekSet);
        FileData = file.readString ();
        file.close ();
        GotFileData = true;

        // DEBUG_V (FileData);
    }
    else
    {
        logcon (String (CN_stars) + CN_Configuration_File_colon + "'" + FileName + F ("' not found.") + CN_stars);
    }

    // DEBUG_END;
    return GotFileData;
} // ReadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadFlashFile (const String& FileName, JsonDocument & FileData)
{
    // DEBUG_START;
    bool GotFileData = false;

    do // once
    {
        String RawFileData;
        if (false == ReadFlashFile (FileName, RawFileData))
        {
            // DEBUG_V ("Failed to read file");
            break;
        }

        // did we actually get any data
        if (0 == RawFileData.length ())
        {
            // DEBUG_V ("File is empty");
            // nope, no data
            GotFileData = false;
            break;
        }

        // DEBUG_V ("Convert File to JSON document");
        DeserializationError error = deserializeJson (FileData, (const String)RawFileData);

        // DEBUG_V ("Error Check");
        if (error)
        {
            String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
            logcon (CN_Heap_colon + String (ESP.getFreeHeap ()));
            logcon (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            logcon (CN_plussigns + RawFileData + CN_minussigns);
            break;
        }

        // DEBUG_V ("Got config data");
        GotFileData = true;

    } while (false);

    // DEBUG_END;
    return GotFileData;

} // ReadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadFlashFile (const String & FileName, byte * FileData, size_t maxlen)
{
    // DEBUG_START;
    bool GotFileData = false;

    do // once
    {
        // DEBUG_V (String("File '") + FileName + "' is being opened.");
        fs::File file = LittleFS.open (FileName.c_str (), CN_r);
        if (!file)
        {
            logcon (String (CN_stars) + CN_Configuration_File_colon + "'" + FileName + F ("' not found.") + CN_stars);
            break;
        }

        if (file.size() >= maxlen)
        {
            logcon (String (CN_stars) + CN_Configuration_File_colon + "'" + FileName + F ("' too large for buffer. ") + CN_stars);
            file.close ();
            break;
        }

        // Suppress this for now, may add it back later
        // Done at interrupt level. Cant use Strings
        // LOG_PORT.print   (FileName);
        // LOG_PORT.print   (" reading ");
        // LOG_PORT.print   (file.size ());
        // LOG_PORT.println (" bytes.");

        // DEBUG_V (String("File '") + FileName + "' is open.");
        file.seek (0, SeekSet);
        file.read (FileData, file.size());
        file.close ();

        GotFileData = true;

        /// DEBUG_V (FileData);

    } while (false);

    // DEBUG_END;
    return GotFileData;

} // ReadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::FlashFileExists (const String & FileName)
{
    return LittleFS.exists (FileName.c_str ());

} // FlashFileExists

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void c_FileMgr::InitSdFileList ()
{
    // DEBUG_START;

    int index = 0;
    for (auto& currentFileListEntry : FileList)
    {
        currentFileListEntry.handle  = INVALID_FILE_HANDLE;
        currentFileListEntry.entryId = index++;
    }

    // DEBUG_END;

} // InitFileList

//-----------------------------------------------------------------------------
int c_FileMgr::FileListFindSdFileHandle (FileId HandleToFind)
{
    // DEBUG_START;

    int response = -1;
    // DEBUG_V (String ("HandleToFind: ") + String (HandleToFind));

    if(INVALID_FILE_HANDLE != HandleToFind)
    {
        for (auto & currentFileListEntry : FileList)
        {
            // DEBUG_V (String ("currentFileListEntry.handle: ")  + String (currentFileListEntry.handle));
            // DEBUG_V (String ("currentFileListEntry.entryId: ") + String (currentFileListEntry.entryId));

            if (currentFileListEntry.handle == HandleToFind)
            {
                response = currentFileListEntry.entryId;
                break;
            }
        }
    }

    // DEBUG_END;

    return response;
} // FileListFindSdFileHandle

//-----------------------------------------------------------------------------
c_FileMgr::FileId c_FileMgr::CreateSdFileHandle ()
{
    // DEBUG_START;

    FileId response = INVALID_FILE_HANDLE;
    FileId FileHandle = millis ();

    // create a unique handle
    while (-1 != FileListFindSdFileHandle (FileHandle))
    {
        ++FileHandle;
    }

    // find an empty slot
    for (auto & currentFileListEntry : FileList)
    {
        if (currentFileListEntry.handle == INVALID_FILE_HANDLE)
        {
            currentFileListEntry.handle = FileHandle;
            response = FileHandle;
            // DEBUG_V(String("handle: ") + currentFileListEntry.handle);
            break;
        }
    }

    if (INVALID_FILE_HANDLE == response)
    {
        logcon (String (CN_stars) + F (" Could not allocate another file handle ") + CN_stars);
    }
    // DEBUG_V (String ("FileHandle: ") + String (FileHandle));

    // DEBUG_END;

    return response;
} // CreateFileHandle

//-----------------------------------------------------------------------------
void c_FileMgr::DeleteSdFile (const String & FileName)
{
    // DEBUG_START;
    String FileNamePrefix;
    if (!FileName.startsWith ("/"))
    {
        FileNamePrefix = "/";
    }
    // DEBUG_V ();

    if (ESP_SD.exists (FileNamePrefix+FileName))
    {
        // DEBUG_V (String ("Deleting '") + FileName + "'");
        ESP_SD.remove (FileNamePrefix+FileName);
        BuildFseqList();
    }

    // DEBUG_END;

} // DeleteSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::DescribeSdCardToUser ()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP32
    uint64_t cardSize = ESP_SD.cardSize () / (1024 * 1024);
#else
    uint64_t cardSize = ESP_SD.size64 () / (1024 * 1024);
#endif // def ARDUINO_ARCH_ESP32

    logcon (String (F ("SD Card Size: ")) + int64String (cardSize) + "MB");
/*
    // DEBUG_V("Open Root");
    File root = ESP_SD.open("/");
    printDirectory (root, 0);
    root.close();
    // DEBUG_V("Close Root");
*/
    // DEBUG_END;

} // DescribeSdCardToUser

//-----------------------------------------------------------------------------
void c_FileMgr::GetListOfSdFiles (std::vector<String> & Response)
{
    // DEBUG_START;

    Response.clear();
    do // once
    {
        if (false == SdCardIsInstalled ())
        {
            // DEBUG_V("No SD Card installed");
            break;
        }

        File dir = ESP_SDFS.open ("/", CN_r);

        while (true)
        {
            File entry = dir.openNextFile ();

            if (!entry)
            {
                // DEBUG_V("no more files");
                break;
            }

            String EntryName = String (entry.name ());
            EntryName = EntryName.substring ((('/' == EntryName[0]) ? 1 : 0));
            // DEBUG_V ("EntryName: '" + EntryName + "'");
            // DEBUG_V ("EntryName.length(): " + String(EntryName.length ()));

            if ((!EntryName.isEmpty ()) &&
                (!EntryName.equals(String (F ("System Volume Information")))) &&
                (0 != entry.size ())
               )
            {
                // DEBUG_V ("Adding File: '" + EntryName + "'");
                Response.push_back(EntryName);
            }

            entry.close ();
        }

        dir.close();
    } while (false);

    // DEBUG_END;
} // GetListOfSdFiles

//-----------------------------------------------------------------------------
void c_FileMgr::printDirectory (File dir, int numTabs)
{
    // DEBUG_START;
    while (true)
    {
        FeedWDT();
        File entry = dir.openNextFile();
        String tabs = "";

        if (!entry)
        {
            // no more files
            break;
        }

        for (uint8_t i = 0; i < numTabs; i++)
        {
            tabs = tabs + "  ";
        }

        if (entry.isDirectory ())
        {
            logcon (tabs + F ("> ") + entry.name());
            printDirectory (entry, numTabs + 1);
        }
        else
        {
            // files have sizes, directories do not
            logcon (tabs + entry.name() + F (" - ") + String(entry.size ()));
        }
        entry.close ();
    }

    // DEBUG_END;
} // printDirectory

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (const String & FileName, String & FileData)
{
    // DEBUG_START;

    do // once
    {
        FileId FileHandle = INVALID_FILE_HANDLE;
        if (false == OpenSdFile (FileName, FileMode::FileWrite, FileHandle))
        {
            logcon (String (F ("Could not open '")) + FileName + F ("' for writting."));
            break;
        }

        int WriteCount = WriteSdFile (FileHandle, (byte*)FileData.c_str (), (size_t)FileData.length ());
        logcon (String (F ("Wrote '")) + FileName + F ("' ") + String(WriteCount));

        CloseSdFile (FileHandle);

    } while (false);

    // DEBUG_END;

} // SaveSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (const String & FileName, JsonVariant & FileData)
{
    String Temp;
    serializeJson (FileData, Temp);
    SaveSdFile (FileName, Temp);

} // SaveSdFile

//-----------------------------------------------------------------------------
bool c_FileMgr::OpenSdFile (const String & FileName, FileMode Mode, FileId & FileHandle, int FileListIndex)
{
    // DEBUG_START;

    // DEBUG_V(String("Mode: ") + String(Mode));

    bool FileIsOpen = false;

    do // once
    {
        if (!SdCardInstalled)
        {
            // no SD card is installed
            FileHandle = INVALID_FILE_HANDLE;
            break;
        }

        // DEBUG_V ();

        String FileNamePrefix;
        if (!FileName.startsWith ("/"))
        {
            FileNamePrefix = "/";
        }

        // DEBUG_V (String("FileName: '") + FileNamePrefix + FileName + "'");

        if (FileMode::FileRead == Mode)
        {
            // DEBUG_V (String("Read FIle"));
            if (false == ESP_SDFS.exists (FileNamePrefix + FileName))
            {
                logcon (String (F ("ERROR: Cannot find '")) + FileName + F ("' for reading. File does not exist."));
                break;
            }
        }
        // DEBUG_V ("File Exists");

        // do we have a pre defined index?
        if(-1 == FileListIndex)
        {
            // DEBUG_V("No predefined File Handle");
            FileHandle = CreateSdFileHandle ();
            // DEBUG_V (String("FileHandle: ") + String(FileHandle));

            FileListIndex = FileListFindSdFileHandle (FileHandle);
            // DEBUG_V(String("Using lookup File Index: ") + String(FileListIndex));
        }
        else
        {
            // DEBUG_V(String("Using predefined File Index: ") + String(FileListIndex));
            FileHandle = FileList[FileListIndex].handle;
        }

        // did we get an index
        if (-1 != FileListIndex)
        {
            // DEBUG_V(String("Valid FileListIndex: ") + String(FileListIndex));
            FileList[FileListIndex].Filename = FileNamePrefix + FileName;
            // DEBUG_V(String("Got file handle: ") + String(FileHandle));
            FileList[FileListIndex].file = ESP_SDFS.open(FileList[FileListIndex].Filename, XlateFileMode[Mode]);
            FileList[FileListIndex].file.setBufferSize(1500);
            // DEBUG_V(String("ReadWrite: ") + String(XlateFileMode[Mode]));
            // DEBUG_V(String("File Size: ") + String(FileList[FileListIndex].file.size()));
            FileList[FileListIndex].mode = Mode;
            FileList[FileListIndex].Paused = false;

            // DEBUG_V("Open return");
            if (!FileList[FileListIndex].file)
            {
                logcon(String(F("ERROR: Cannot open '")) + FileName + F("'."));
                // release the file list entry
                CloseSdFile(FileHandle);
                break;
            }
            // DEBUG_V("");

            FileList[FileListIndex].size = FileList[FileListIndex].file.size();
            // DEBUG_V(String(FileList[FileListIndex].info.name()) + " - " + String(FileList[FileListIndex].size));

            if (FileMode::FileWrite == Mode)
            {
                // DEBUG_V ("Write Mode");
                FileList[FileListIndex].file.seek (0, SeekSet);
            }
            // DEBUG_V ();

            FileIsOpen = true;
            // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));
        }

    } while (false);

    // DEBUG_END;
    return FileIsOpen;

} // OpenSdFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadSdFile (const String & FileName, String & FileData)
{
    // DEBUG_START;

    bool GotFileData = false;
    FileId FileHandle = INVALID_FILE_HANDLE;

    // DEBUG_V (String("File '") + FileName + "' is being opened.");
    if (true == OpenSdFile (FileName, FileMode::FileRead, FileHandle))
    {
        // DEBUG_V (String("File '") + FileName + "' is open.");
        int FileListIndex;
        if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            FileList[FileListIndex].file.seek (0, SeekSet);
            FileData = FileList[FileListIndex].file.readString ();
        }

        CloseSdFile (FileHandle);
        GotFileData = (0 != FileData.length());

        // DEBUG_V (FileData);
    }
    else
    {
        CloseSdFile (FileHandle);
        logcon (String (F ("SD file: '")) + FileName + String (F ("' not found.")));
    }

    // DEBUG_END;
    return GotFileData;

} // ReadSdFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadSdFile (const String & FileName, JsonDocument & FileData)
{
    // DEBUG_START;

    bool GotFileData = false;
    FileId FileHandle = INVALID_FILE_HANDLE;

    // DEBUG_V (String("File '") + FileName + "' is being opened.");
    if (true == OpenSdFile (FileName, FileMode::FileRead, FileHandle))
    {
        // DEBUG_V (String("File '") + FileName + "' is open.");
        int FileListIndex;
        if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {

            FileList[FileListIndex].file.seek (0, SeekSet);
            String RawFileData = FileList[FileListIndex].file.readString ();

            // DEBUG_V ("Convert File to JSON document");
            DeserializationError error = deserializeJson (FileData, RawFileData);

            // DEBUG_V ("Error Check");
            if (error)
            {
                String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
                logcon (CN_Heap_colon + String (ESP.getFreeHeap ()));
                logcon (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
                logcon (CN_plussigns + RawFileData + CN_minussigns);
            }
            else
            {
                GotFileData = true;
            }
        }
        CloseSdFile(FileHandle);
    }
    else
    {
        logcon (String (F ("SD file: '")) + FileName + String (F ("' not found.")));
    }

    // DEBUG_END;
    return GotFileData;

} // ReadSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::ReadSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToRead, size_t StartingPosition)
{
    // DEBUG_START;

    size_t response = 0;

    // DEBUG_V (String ("       FileHandle: ") + String (FileHandle));
    // DEBUG_V (String ("   NumBytesToRead: ") + String (NumBytesToRead));
    // DEBUG_V (String (" StartingPosition: ") + String (StartingPosition));

    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        size_t BytesRemaining = size_t(FileList[FileListIndex].size - StartingPosition);
        size_t ActualBytesToRead = min(NumBytesToRead, BytesRemaining);
        // DEBUG_V(String("   BytesRemaining: ") + String(BytesRemaining));
        // DEBUG_V(String("ActualBytesToRead: ") + String(ActualBytesToRead));
        if(!FileList[FileListIndex].file.seek(StartingPosition, SeekSet))
        {
            logcon(F("ERROR: SD Card: Could not set file read start position"));
        }
        else
        {
            response = ReadSdFile(FileHandle, FileData, ActualBytesToRead);
            // DEBUG_V(String("         response: ") + String(response));
        }
    }
    else
    {
        logcon (String (F ("ReadSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }

    // DEBUG_END;
    return response;

} // ReadSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::ReadSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToRead)
{
    // DEBUG_START;

    // DEBUG_V (String ("       FileHandle: ") + String (FileHandle));
    // DEBUG_V (String ("   numBytesToRead: ") + String (NumBytesToRead));

    size_t response = 0;
    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        response = FileList[FileListIndex].file.readBytes(((char *)FileData), NumBytesToRead);
        // DEBUG_V(String("         response: ") + String(response));
    }
    else
    {
        logcon (String (F ("ReadSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }

    // DEBUG_END;
    return response;

} // ReadSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::CloseSdFile (FileId& FileHandle)
{
    // DEBUG_START;
    // DEBUG_V(String("      FileHandle: ") + String(FileHandle));

    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        if(!FileList[FileListIndex].Paused)
        {
            FileList[FileListIndex].file.close ();
        }
        FileList[FileListIndex].handle = INVALID_FILE_HANDLE;
        FileList[FileListIndex].Paused = false;
    }
    else
    {
        logcon (String (F ("CloseSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }

    FileHandle = INVALID_FILE_HANDLE;

    // DEBUG_END;

} // CloseSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::WriteSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToWrite)
{
    // DEBUG_START;

    size_t NumBytesWritten = 0;
    do // once
    {
        int FileListIndex;
        // DEBUG_V (String("Bytes to write: ") + String(NumBytesToWrite));
        if (-1 == (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            logcon (String (F ("WriteSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
            break;
        }

        NumBytesWritten = FileList[FileListIndex].file.write(FileData, NumBytesToWrite);
        FileList[FileListIndex].file.flush();
        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));

        if(NumBytesWritten != NumBytesToWrite)
        {
            logcon(String("ERROR: SD Write failed. Tried writting ") + String(NumBytesToWrite) + " bytes. Actually wrote " + String(NumBytesWritten) + " bytes.");
                    NumBytesWritten = 0;
                    break;
        }
    } while(false);

    // DEBUG_END;
    return NumBytesWritten;

} // WriteSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::WriteSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToWrite, size_t StartingPosition)
{
    size_t response = 0;
    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));
        FileList[FileListIndex].file.seek (StartingPosition, SeekSet);
        response = WriteSdFile (FileHandle, FileData, NumBytesToWrite);
    }
    else
    {
        logcon (String (F ("WriteSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }

    return response;

} // WriteSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::GetSdFileSize (const String& FileName)
{
    size_t response = 0;
    FileId Handle = INVALID_FILE_HANDLE;
    if(OpenSdFile (FileName,   FileMode::FileRead, Handle))
    {
        response = GetSdFileSize(Handle);
        CloseSdFile(Handle);
    }
    else
    {
        logcon(String("Could not open '") + FileName + "' to check size.");
    }

    return response;

} // GetSdFileSize

//-----------------------------------------------------------------------------
size_t c_FileMgr::GetSdFileSize (const FileId& FileHandle)
{
    size_t response = 0;
    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        response = FileList[FileListIndex].file.size ();
    }
    else
    {
        logcon (String (F ("GetSdFileSize::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }

    return response;

} // GetSdFileSize

//-----------------------------------------------------------------------------
void c_FileMgr::ResumeSdFile (const FileId & FileHandle)
{
    // DEBUG_START;

    do // once
    {
        int FileListIndex;
        if (-1 == (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            logcon (String (F ("ResumeSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
            break;
        }
        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));

        if(!FileList[FileListIndex].Paused)
        {
            // DEBUG_V("File is not paused. Ignore Resume request");
            break;
        }

        FileList[FileListIndex].mode = FileMode::FileAppend;
        OpenSdFile(FileList[FileListIndex].Filename,
                   FileMode::FileAppend,
                   FileList[FileListIndex].handle,
                   FileListIndex);
        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));

    } while(false);

    // DEBUG_END;
} // ResumeSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::PauseSdFile (const FileId & FileHandle)
{
    // DEBUG_START;

    do // once
    {
        int FileListIndex;
        if (-1 == (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            logcon (String (F ("PauseSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
            break;
        }

        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));

        if(FileList[FileListIndex].Paused)
        {
            // DEBUG_V("File is paused. Ignore Pause request");
            break;
        }

        FileList[FileListIndex].file.close ();
        FileList[FileListIndex].Paused = true;

    } while(false);

    // DEBUG_END;
} // PauseSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::BuildFseqList()
{
    // DEBUG_START;

    // cant perform operation
    FeedWDT ();

    do // once
    {
        if(!SdCardIsInstalled())
        {
            // DEBUG_V("No SD card installed.");
            break;
        }

        fs::File InputFile = ESP_SDFS.open ("/", "r");
        if(!InputFile)
        {
            logcon("ERROR: Could not open SD card.");
            break;
        }

        // open output file, erase old data
        fs::File OutputFile = ESP_SDFS.open ("/fseqfilelist.json", "w");
        if(!OutputFile)
        {
            logcon("ERROR: Could not save SD file list.");
            break;
        }
        OutputFile.print("{");

        uint64_t totalBytes = 0;

#ifdef ARDUINO_ARCH_ESP32
        totalBytes = ESP_SD.cardSize ();
#else
        totalBytes = ESP_SD.size64 ();
#endif
        String Entry = String("\"totalBytes\" : ") + int64String(totalBytes) + ",";
        OutputFile.print(Entry);

        uint64_t usedBytes = 0;
        uint32_t numFiles = 0;

        OutputFile.print("\"files\" : [");
       while (true)
        {
            FeedWDT();

            File entry = InputFile.openNextFile ();

            if (!entry)
            {
                // DEBUG_V("no more files to add to list");
                break;
            }

            if(entry.isDirectory())
            {
                // DEBUG_V("Skip embedded directory");
                continue;
            }

            String EntryName = String (entry.name ());
            // DEBUG_V (         "EntryName: " + EntryName);
            EntryName = EntryName.substring ((('/' == EntryName[0]) ? 1 : 0));
            // DEBUG_V ("EntryName.length(): " + String(EntryName.length ()));
            // DEBUG_V ("      entry.size(): " + String(entry.size ()));

            if ((!EntryName.isEmpty ()) &&
                (!EntryName.equals(String (F ("System Volume Information")))) &&
                (0 != entry.size () &&
                !EntryName.equals("fseqfilelist.json"))
               )
            {
                // do we need to add a seperator?
                if(numFiles)
                {
                    // DEBUG_V("Adding trailing comma");
                    OutputFile.print(",");
                }

                usedBytes += entry.size ();
                ++numFiles;

                if(IsBooting)
                {
                    logcon ("SD File: '" + EntryName + "'");
                }
                OutputFile.print(String("{\"name\" : \"") + EntryName +
                                 "\",\"date\" : " + entry.getLastWrite () +
                                 ",\"length\" : " + entry.size () + "}");
            }

            entry.close ();
        } // end while true

        // close the array and add the descriptive data
        OutputFile.print(String("], \"usedBytes\" : ") + int64String(usedBytes) + ",");
        OutputFile.print(String("\"numFiles\" : ") + String(numFiles));

        // close the data section
        OutputFile.print("}");

        OutputFile.close();
    } while(false);

    // DEBUG_END;

} // BuildFseqList

//-----------------------------------------------------------------------------
bool c_FileMgr::handleFileUpload (
    const String & filename,
    size_t index,
    uint8_t* data,
    size_t len,
    bool final,
    uint32_t totalLen)
{
    // DEBUG_START;
    bool response = false;

    // DEBUG_V (String ("filename: ") + filename);
    // DEBUG_V (String ("   index: ") + String (index));
    // DEBUG_V (String ("     len: ") + String (len));
    // DEBUG_V (String ("   final: ") + String (final));
    // DEBUG_V (String ("   total: ") + String (totalLen));

    do // once
    {
        if ((0 == index))
        {
            // DEBUG_V("New File");
            handleFileUploadNewFile (filename);
            expectedIndex = 0;
        }

        if (index != expectedIndex)
        {
            if(fsUploadFile != INVALID_FILE_HANDLE)
            {
                logcon (String(F("ERROR: Expected index: ")) + String(expectedIndex) + F(" does not match actual index: ") + String(index));

                CloseSdFile(fsUploadFile);
                DeleteSdFile (fsUploadFileName);
                fsUploadFile = INVALID_FILE_HANDLE;
                BuildFseqList();
                expectedIndex = 0;
                fsUploadFileName = emptyString;
            }
            break;
        }

        // force a write out
        PauseSdFile(fsUploadFile);
        ResumeSdFile(fsUploadFile);
        // DEBUG_V (String ("fsUploadFile: ") + String (fsUploadFile));

        // update the next expected chunk id
        expectedIndex = index + len;
        size_t bytesWritten = 0;

        if (len)
        {
            // Write data
            // DEBUG_V ("UploadWrite: " + String (len) + String (" bytes"));
            bytesWritten = WriteSdFile (fsUploadFile, data, len);
            // DEBUG_V (String ("Writing bytes: ") + String (index));
            // LOG_PORT.print (".");
        }
        // PauseSdFile(fsUploadFile);

        if(len != bytesWritten)
        {
            // DEBUG_V("Write failed. Stop transfer");
            CloseSdFile(fsUploadFile);
            DeleteSdFile (fsUploadFileName);
            fsUploadFile = INVALID_FILE_HANDLE;
            BuildFseqList();
            expectedIndex = 0;
            fsUploadFileName = emptyString;
            break;
        }
        response = true;
    } while(false);

    if ((true == final) && (fsUploadFile != INVALID_FILE_HANDLE))
    {
        uint32_t uploadTime = (uint32_t)(millis() - fsUploadStartTime) / 1000;
        CloseSdFile (fsUploadFile);
        fsUploadFile = INVALID_FILE_HANDLE;

        logcon (String (F ("Upload File: '")) + fsUploadFileName +
                F ("' Done (") + String (uploadTime) +
                F ("s). Received: ") + String(expectedIndex) +
                F(" Bytes out of ") + String(totalLen) +
                F(" bytes. FileLen: ") + GetSdFileSize(filename));

        expectedIndex = 0;
        BuildFseqList();

        // DEBUG_V(String("Expected: ") + String(totalLen));
        // DEBUG_V(String("     Got: ") + String(GetSdFileSize(fsUploadFileName)));

        fsUploadFileName = "";
    }

    // DEBUG_END;
    return response;
} // handleFileUpload

//-----------------------------------------------------------------------------
void c_FileMgr::handleFileUploadNewFile (const String & filename)
{
    // DEBUG_START;

    // save the filename
    // DEBUG_V ("UploadStart: " + filename);

    fsUploadStartTime = millis();

    // are we terminating the previous download?
    if (0 != fsUploadFileName.length ())
    {
        logcon (String (F ("Aborting Previous File Upload For: '")) + fsUploadFileName + String (F ("'")));
        CloseSdFile (fsUploadFile);
        fsUploadFileName = "";
    }

    // Set up to receive a file
    fsUploadFileName = filename;

    logcon (String (F ("Upload File: '")) + fsUploadFileName + String (F ("' Started")));

    DeleteSdFile (fsUploadFileName);

    // Open the file for writing
    OpenSdFile (fsUploadFileName, FileMode::FileWrite, fsUploadFile);

    // DEBUG_END;

} // handleFileUploadNewFile

// create a global instance of the File Manager
c_FileMgr FileMgr;
