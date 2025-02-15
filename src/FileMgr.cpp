/*
* FileMgr.cpp - Output Management class
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

#include "ESPixelStick.h"
#include <Int64String.h>
#include <TimeLib.h>

#include "FileMgr.hpp"
#include "network/NetworkMgr.hpp"
#include "output/OutputMgr.hpp"
#include "UnzipFiles.hpp"

SdFs sd;
const int8_t DISABLE_CS_PIN = -1;

#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16))
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(16))
#endif  // HAS_SDIO_CLASS

oflag_t XlateFileMode[3] = { O_READ , O_WRITE | O_CREAT, O_WRITE | O_APPEND };
#ifdef SUPPORT_FTP
#include <SimpleFTPServer.h>
FtpServer   ftpSrv;

//-----------------------------------------------------------------------------
void ftp_callback(FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace)
{
    FeedWDT();
    switch (ftpOperation)
    {
        case FTP_CONNECT:
        {
            LOG_PORT.println(F("FTP: Connected!"));
            break;
        }

        case FTP_DISCONNECT:
        {
            LOG_PORT.println(F("FTP: Disconnected!"));
            break;
        }

        case FTP_FREE_SPACE_CHANGE:
        {
            FileMgr.BuildFseqList(false);
            LOG_PORT.printf("FTP: Free space change, free %u of %u! Rebuilding FSEQ file list\n", freeSpace, totalSpace);
            break;
        }

        default:
        {
            LOG_PORT.println(F("FTP: unknown callback!"));
            break;
        }
    }
};

//-----------------------------------------------------------------------------
void ftp_transferCallback(FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize)
{
    FeedWDT();
    switch (ftpOperation)
    {
        case FTP_UPLOAD_START:
        {
            LOG_PORT.println(String(F("FTP: Start Uploading '")) + name + "'");
            break;
        }

        case FTP_UPLOAD:
        {
            // LOG_PORT.printf("FTP: Upload of file %s byte %u\n", name, transferredSize);
            break;
        }

        case FTP_TRANSFER_STOP:
        {
            LOG_PORT.println(String(F("FTP: Done Uploading '")) + name + "'");
            break;
        }

        case FTP_TRANSFER_ERROR:
        {
            LOG_PORT.println(String(F("FTP: Error Uploading '")) + name + "'");
            break;
        }

        default:
        {
            LOG_PORT.println(F("FTP: Unknown Transfer Callback!"));
            break;
        }
    }

  /* FTP_UPLOAD_START = 0,
   * FTP_UPLOAD = 1,
   *
   * FTP_DOWNLOAD_START = 2,
   * FTP_DOWNLOAD = 3,
   *
   * FTP_TRANSFER_STOP = 4,
   * FTP_DOWNLOAD_STOP = 4,
   * FTP_UPLOAD_STOP = 4,
   *
   * FTP_TRANSFER_ERROR = 5,
   * FTP_DOWNLOAD_ERROR = 5,
   * FTP_UPLOAD_ERROR = 5
   */
};
#endif // def SUPPORT_FTP

//-----------------------------------------------------------------------------
static PROGMEM const char DefaultFseqResponse[] =
    "{\"SdCardPresent\" : false," \
    "\"totalBytes\" : 0," \
    "\"files\" : []," \
    "\"usedBytes\" : 0," \
    "\"numFiles\" : 0" \
    "}";

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_FileMgr::c_FileMgr ()
{
    SdAccessSemaphore = false;
} // c_FileMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_FileMgr::~c_FileMgr ()
{
    // DEBUG__START;

    // DEBUG__END;

} // ~c_FileMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_FileMgr::Begin ()
{
    // DEBUG__START;

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

#ifdef SUPPORT_UNZIP
        if(FoundZipFile)
        {
            FeedWDT();
            UnzipFiles * Unzipper = new(UnzipFiles);
            Unzipper->Run();
            delete Unzipper;
            logcon("Requesting reboot after unzipping files");
            RequestReboot(1, true);
        }
#endif // def SUPPORT_UNZIP

    } while (false);

    // DEBUG__END;
} // begin

//-----------------------------------------------------------------------------
void c_FileMgr::NetworkStateChanged (bool NewState)
{
    // DEBUG__START;
#ifdef SUPPORT_FTP
    ftpSrv.end();

    // DEBUG__V(String("       NewState: ") + String(NewState));
    // DEBUG__V(String("SdCardInstalled: ") + String(SdCardInstalled));
    // DEBUG__V(String("     FtpEnabled: ") + String(FtpEnabled));
    if(NewState && SdCardInstalled && FtpEnabled)
    {
        ftpSrv.end();
        logcon("Starting FTP server.");
        ftpSrv.begin(FtpUserName.c_str(), FtpPassword.c_str(), WelcomeString.c_str());
        ftpSrv.setCallback(ftp_callback);
        ftpSrv.setTransferCallback(ftp_transferCallback);
    }
#endif // def SUPPORT_FTP

    // DEBUG__END;
} // NetworkStateChanged

//-----------------------------------------------------------------------------
void c_FileMgr::Poll()
 {
    // xDEBUG_START;
#ifdef SUPPORT_FTP
    if(FtpEnabled)
    {
        FeedWDT();
        ftpSrv.handleFTP();
    }
#endif // def SUPPORT_FTP
    // xDEBUG_END;
 } // Poll

//-----------------------------------------------------------------------------
bool c_FileMgr::SetConfig (JsonObject & json)
{
    // DEBUG__START;

    bool ConfigChanged = false;
    JsonObject JsonDeviceConfig = json[(char*)CN_device].as<JsonObject>();
    if (JsonDeviceConfig)
    {
        // PrettyPrint (JsonDeviceConfig, "c_FileMgr");

        // DEBUG__V("miso_pin: " + String(miso_pin));
        // DEBUG__V("mosi_pin: " + String(mosi_pin));
        // DEBUG__V(" clk_pin: " + String(clk_pin));
        // DEBUG__V("  cs_pin: " + String(cs_pin));

        ConfigChanged |= setFromJSON (miso_pin,   JsonDeviceConfig, CN_miso_pin);
        ConfigChanged |= setFromJSON (mosi_pin,   JsonDeviceConfig, CN_mosi_pin);
        ConfigChanged |= setFromJSON (clk_pin,    JsonDeviceConfig, CN_clock_pin);
        ConfigChanged |= setFromJSON (cs_pin,     JsonDeviceConfig, CN_cs_pin);
        ConfigChanged |= setFromJSON (MaxSdSpeed, JsonDeviceConfig, CN_sdspeed);

        ConfigChanged |= setFromJSON (FtpUserName, JsonDeviceConfig, CN_user);
        ConfigChanged |= setFromJSON (FtpPassword, JsonDeviceConfig, CN_password);
        ConfigChanged |= setFromJSON (FtpEnabled,  JsonDeviceConfig, CN_enabled);

        // DEBUG__V("miso_pin: " + String(miso_pin));
        // DEBUG__V("mosi_pin: " + String(mosi_pin));
        // DEBUG__V(" clk_pin: " + String(clk_pin));
        // DEBUG__V("  cs_pin: " + String(cs_pin));
        // DEBUG__V("   Speed: " + String(MaxSdSpeed));

        // DEBUG__V("ConfigChanged: " + String(ConfigChanged));
    }
    else
    {
        logcon (F ("No File Manager settings found."));
    }

    // DEBUG__V (String ("ConfigChanged: ") + String (ConfigChanged));

    if (ConfigChanged)
    {
        SetSpiIoPins ();
        NetworkStateChanged(NetworkMgr.IsConnected());
    }

    // DEBUG__END;

    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_FileMgr::GetConfig (JsonObject& json)
{
    // DEBUG__START;

    JsonWrite(json, CN_miso_pin,  miso_pin);
    JsonWrite(json, CN_mosi_pin,  mosi_pin);
    JsonWrite(json, CN_clock_pin, clk_pin);
    JsonWrite(json, CN_cs_pin,    cs_pin);
    JsonWrite(json, CN_sdspeed,   MaxSdSpeed);

    JsonWrite(json, CN_user,      FtpUserName);
    JsonWrite(json, CN_password,  FtpPassword);
    JsonWrite(json, CN_enabled,   FtpEnabled);

    // DEBUG__END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_FileMgr::GetStatus (JsonObject& json)
{
    // DEBUG__START;

#ifdef ARDUINO_ARCH_ESP32
    json[F ("size")] = LittleFS.totalBytes ();
    json[F ("used")] = LittleFS.usedBytes ();
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG__END;

} // GetConfig

//------------------------------------------------------------------------------
// Call back for file timestamps.  Only called for file create and sync().
void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10)
{
    // LOG_PORT.println("DateTime Start");

    tmElements_t tm;
    breakTime(now(), tm);

    // LOG_PORT.println(String("time_t:") + String(now()));
    // LOG_PORT.println(String("Year Raw:") + String(tm.Year));
    // LOG_PORT.println(String("Year Adj:") + String(tm.Year + 1970));
    // LOG_PORT.println(String("Month:") + String(tm.Month));
    // LOG_PORT.println(String("Day:") + String(tm.Day));

    // Return date using FS_DATE macro to format fields.
    *date = FS_DATE(tm.Year+1970, tm.Month, tm.Day);
    // LOG_PORT.println(String("date:") + String(*date));

    // Return time using FS_TIME macro to format fields.
    *time = FS_TIME(tm.Hour, tm.Minute, tm.Second);
    // LOG_PORT.println(String("time:") + String(*time));

    // Return low time bits in units of 10 ms, 0 <= ms10 <= 199.
    *ms10 = 0;
    // LOG_PORT.println("DateTime End");
}

//-----------------------------------------------------------------------------
void c_FileMgr::SetSpiIoPins ()
{
    // DEBUG__START;
#if defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)
    if (SdCardInstalled)
    {
        // DEBUG__V("Terminate current SD session");
        ESP_SD.end ();
    }

    FsDateTime::setCallback(dateTime);
#ifdef ARDUINO_ARCH_ESP32
    // DEBUG__V();
    try
#endif // def ARDUINO_ARCH_ESP32
    {
#ifdef SUPPORT_SD_MMC
        // DEBUG__V (String ("  Data 0: ") + String (SD_CARD_DATA_0));
        // DEBUG__V (String ("  Data 1: ") + String (SD_CARD_DATA_1));
        // DEBUG__V (String ("  Data 2: ") + String (SD_CARD_DATA_2));
        // DEBUG__V (String ("  Data 3: ") + String (SD_CARD_DATA_3));

        pinMode(SD_CARD_DATA_0, PULLUP);
        pinMode(SD_CARD_DATA_1, PULLUP);
        pinMode(SD_CARD_DATA_2, PULLUP);
        pinMode(SD_CARD_DATA_3, PULLUP);

        if(!ESP_SD.begin())
#else // ! SUPPORT_SD_MMC
#   ifdef ARDUINO_ARCH_ESP32
        // DEBUG__V (String ("miso_pin: ") + String (miso_pin));
        // DEBUG__V (String ("mosi_pin: ") + String (mosi_pin));
        // DEBUG__V (String (" clk_pin: ") + String (clk_pin));
        // DEBUG__V (String ("  cs_pin: ") + String (cs_pin));

        // DEBUG__V();
        SPI.end ();
        // DEBUG__V();
        ResetGpio(gpio_num_t(cs_pin));
        pinMode(cs_pin, OUTPUT);
#       ifdef USE_MISO_PULLUP
        // DEBUG__V("USE_MISO_PULLUP");
        // on some hardware MISO is missing a required pull-up resistor, use internal pull-up.
        ResetGpio(gpio_num_t(mosi_pin));
        pinMode(miso_pin, INPUT_PULLUP);
#       else
        // DEBUG__V();
        ResetGpio(gpio_num_t(miso_pin));
        pinMode(miso_pin, INPUT);
#       endif // def USE_MISO_PULLUP
        ResetGpio(gpio_num_t(clk_pin));
        ResetGpio(gpio_num_t(miso_pin));
        ResetGpio(gpio_num_t(cs_pin));
        SPI.begin (clk_pin, miso_pin, mosi_pin, cs_pin); // uses HSPI by default
#   else // ESP8266
        SPI.end ();
        SPI.begin ();
#   endif // ! ARDUINO_ARCH_ESP32
        // DEBUG__V();
        if (!ESP_SD.begin (SdSpiConfig(cs_pin, SHARED_SPI, SD_SCK_MHZ(16))))
#endif // !def SUPPORT_SD_MMC
        {
            // DEBUG__V();
            logcon(String(F("No SD card installed")));
            SdCardInstalled = false;
            // DEBUG__V();
            if(nullptr == ESP_SD.card())
            {
                logcon(F("SD 'Card' setup failed."));
            }
            else if (ESP_SD.card()->errorCode())
            {
                logcon(String(F("SD initialization failed - code: ")) + String(ESP_SD.card()->errorCode()));
                // DEBUG__V(String(F("SD initialization failed - data: ")) + String(ESP_SD.card()->errorData()));
                printSdErrorText(&Serial, ESP_SD.card()->errorCode()); LOG_PORT.println("");
            }
            else if (ESP_SD.vol()->fatType() == 0)
            {
                logcon(F("SD Can't find a valid FAT16/FAT32 partition."));
            }
            else
            {
                logcon(F("SD Can't determine error type"));
            }
        }
        else
        {
            // DEBUG__V();
            SdCardInstalled = true;
            // DEBUG__V();
            SetSdSpeed ();

            // DEBUG__V(String("SdCardSizeMB: ") + String(SdCardSizeMB));

            DescribeSdCardToUser ();
            // DEBUG__V();
            BuildFseqList(true);
            // DEBUG__V();
        }
    }
#ifdef ARDUINO_ARCH_ESP32
    catch (const std::exception &e)
    {
        logcon (String (F ("ERROR: Could not init the SD Card: "))+ e.what());
        SdCardInstalled = false;
    }
#endif // def ARDUINO_ARCH_ESP32

    // SdFile::dateTimeCallback(dateTime);

#else // no sd defined
    SdCardInstalled = false;
#endif // defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)

    // DEBUG__END;

} // SetSpiIoPins

//-----------------------------------------------------------------------------
void c_FileMgr::SetSdSpeed ()
{
    // DEBUG__START;
#if defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)
    if (SdCardInstalled)
    {
        // DEBUG__V();
        csd_t csd;
        uint8_t tran_speed = 0;

        CSD MyCsd;
        // DEBUG__V();
        ESP_SD.card()->readCSD(&csd);
        memcpy (&MyCsd, &csd.csd[0], sizeof(csd.csd));
        // DEBUG__V(String("TRAN Speed: 0x") + String(MyCsd.Decode_0.tran_speed,HEX));
        tran_speed = MyCsd.Decode_0.tran_speed;
        uint32_t FinalTranSpeedMHz = MaxSdSpeed;

        switch(tran_speed)
        {
            case 0x32:
            {
                FinalTranSpeedMHz = 25;
                break;
            }
            case 0x5A:
            {
                FinalTranSpeedMHz = 50;
                break;
            }
            case 0x0B:
            {
                FinalTranSpeedMHz = 100;
                break;
            }
            case 0x2B:
            {
                FinalTranSpeedMHz = 200;
                break;
            }
            default:
            {
                FinalTranSpeedMHz = 25;
            }
        }
        // DEBUG__V();
        FinalTranSpeedMHz = min(FinalTranSpeedMHz, MaxSdSpeed);
        SPI.setFrequency(FinalTranSpeedMHz * 1024 * 1024);
        logcon(String("Set SD speed to ") + String(FinalTranSpeedMHz) + "Mhz");

        SdCardSizeMB = 0.000512 * csd.capacity();
        // DEBUG__V(String("SdCardSizeMB: ") + String(SdCardSizeMB));
    }
#endif // defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)

    // DEBUG__END;

} // SetSdSpeed

//-----------------------------------------------------------------------------
/*
    On occasion, the SD card is left in an unknown state that causes the SD card
    to appear to not be installed. A power cycle fixes the issue. Clocking the
    bus also causes any incomplete transaction to get cleared.
*/
void c_FileMgr::ResetSdCard()
{
    // DEBUG__START;

    // send 0xff bytes until we get 0xff back
    byte ResetValue = 0x00;
    digitalWrite(cs_pin, LOW);
    SPI.beginTransaction(SPISettings());
    // DEBUG__V();
    uint32_t retry = 2000;
    while((0xff != ResetValue) && (--retry))
    {
        delay(1);
        ResetValue = SPI.transfer(0xff);
    }
    SPI.endTransaction();
    digitalWrite(cs_pin, HIGH);

    // DEBUG__END;
} // ResetSdCard

//-----------------------------------------------------------------------------
void c_FileMgr::DeleteFlashFile (const String& FileName)
{
    // DEBUG__START;

    LittleFS.remove (FileName);

    // DEBUG__END;

} // DeleteConfigFile

//-----------------------------------------------------------------------------
void c_FileMgr::listDir (fs::FS& fs, String dirname, uint8_t levels)
{
    // DEBUG__START;
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
    // DEBUG__START;

    bool retval = false;

    do // once
    {
        String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";

        // DEBUG__V ("allocate the JSON Doc");
/*
        String RawFileData;
        if (false == ReadConfigFile (FileName, RawFileData))
        {
            logcon (String(CN_stars) + CfgFileMessagePrefix + F ("Could not read file.") + CN_stars);
            break;
        }
        logcon(RawFileData);
*/
        // DEBUG__V();
        fs::File file = LittleFS.open (FileName.c_str (), "r");
        // DEBUG__V();
        if (!file)
        {
            if (!IsBooting)
            {
                logcon (String (CN_stars) + CfgFileMessagePrefix + String (F (" Could not open file for reading ")) + CN_stars);
            }
            break;
        }

        JsonDocument jsonDoc;

        // DEBUG__V ("Convert File to JSON document");
        DeserializationError error = deserializeJson (jsonDoc, file);
        file.close ();

        // DEBUG__V ("Error Check");
        if (error)
        {
            // logcon (CN_Heap_colon + String (ESP.getMaxFreeBlockSize ()));
            logcon (String(CN_stars) + CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str () + CN_stars);
            // logcon (CN_plussigns + RawFileData + CN_minussigns);
	        // DEBUG__V (String ("                heap: ") + String (ESP.getFreeHeap ()));
    	    //  xDEBUG_V (String (" getMaxFreeBlockSize: ") + String (ESP.getMaxFreeBlockSize ()));
        	// xDEBUG_V (String ("           file.size: ") + String (file.size ()));
            break;
        }

        // DEBUG__V ();
        logcon (CfgFileMessagePrefix + String (F ("loaded.")));

        // DEBUG__V ();
        Handler (jsonDoc);

        // DEBUG__V();
        retval = true;

    } while (false);

    // DEBUG__END;
    return retval;

} // LoadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile (const String& FileName, String& FileData)
{
    // DEBUG__START;

    bool Response = SaveFlashFile (FileName, FileData.c_str ());

    // DEBUG__END;
    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile (const String& FileName, const char * FileData)
{
    // DEBUG__START;

    bool Response = false;
    String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
    // DEBUG__V (FileData);

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

    // DEBUG__END;
    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile(const String &FileName, JsonDocument &FileData)
{
    // DEBUG__START;
    bool Response = false;

    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";
    // DEBUG__V(String("CfgFileMessagePrefix: ") + CfgFileMessagePrefix);

    // serializeJson(FileData, LOG_PORT);
    // DEBUG__V("");

    // delay(100);
    // DEBUG__V("");

    fs::File file = LittleFS.open(FileName.c_str(), "w");
    // DEBUG__V("");

    if (!file)
    {
        logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Could not open file for writing..")) + CN_stars);
    }
    else
    {
        // DEBUG__V("");
        file.seek(0, SeekSet);
        // DEBUG__V("");

        size_t NumBytesSaved = serializeJson(FileData, file);
        // DEBUG__V("");

        file.close();

        logcon(CfgFileMessagePrefix + String(F("saved ")) + String(NumBytesSaved) + F(" bytes."));

        Response = true;
    }

    // DEBUG__END;

    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveFlashFile(const String FileName, uint32_t index, uint8_t *data, uint32_t len, bool final)
{
    // DEBUG__START;
    bool Response = false;

    String CfgFileMessagePrefix = String(CN_Configuration_File_colon) + "'" + FileName + "' ";
    // DEBUG__V(String("CfgFileMessagePrefix: ") + CfgFileMessagePrefix);
    // DEBUG__V(String("            FileName: ") + FileName);
    // DEBUG__V(String("               index: ") + String(index));
    // DEBUG__V(String("                 len: ") + String(len));
    // DEBUG__V(String("               final: ") + String(final));

    fs::File file;

    if(0 == index)
    {
        file = LittleFS.open(FileName.c_str(), "w");
    }
    else
    {
        file = LittleFS.open(FileName.c_str(), "a");
    }
    // DEBUG__V("");

    if (!file)
    {
        logcon(String(CN_stars) + CfgFileMessagePrefix + String(F("Could not open file for writing..")) + CN_stars);
    }
    else
    {
        // DEBUG__V("");
        file.seek(int(index), SeekMode::SeekSet);
        // DEBUG__V("");

        size_t NumBytesSaved = file.write(data, size_t(len));
        // DEBUG__V("");

        file.close();

        logcon(CfgFileMessagePrefix + String(F("saved ")) + String(NumBytesSaved) + F(" bytes."));

        Response = true;
    }

    // DEBUG__END;

    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadFlashFile (const String& FileName, String& FileData)
{
    // DEBUG__START;

    bool GotFileData = false;
    String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";

    // DEBUG__V (String("File '") + FileName + "' is being opened.");
    fs::File file = LittleFS.open (FileName.c_str (), CN_r);
    if (file)
    {
        // Suppress this for now, may add it back later
        //logcon (CfgFileMessagePrefix + String (F ("reading ")) + String (file.size()) + F (" bytes."));

        // DEBUG__V (String("File '") + FileName + "' is open.");
        file.seek (0, SeekSet);
        FileData = file.readString ();
        file.close ();
        GotFileData = true;

        // DEBUG__V (FileData);
    }
    else
    {
        logcon (String (CN_stars) + CN_Configuration_File_colon + "'" + FileName + F ("' not found.") + CN_stars);
    }

    // DEBUG__END;
    return GotFileData;
} // ReadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadFlashFile (const String& FileName, JsonDocument & FileData)
{
    // DEBUG__START;
    bool GotFileData = false;

    do // once
    {
        String RawFileData;
        if (false == ReadFlashFile (FileName, RawFileData))
        {
            // DEBUG__V ("Failed to read file");
            break;
        }

        // did we actually get any data
        if (0 == RawFileData.length ())
        {
            // DEBUG__V ("File is empty");
            // nope, no data
            GotFileData = false;
            break;
        }

        // DEBUG__V ("Convert File to JSON document");
        DeserializationError error = deserializeJson (FileData, (const String)RawFileData);

        // DEBUG__V ("Error Check");
        if (error)
        {
            String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
            logcon (CN_Heap_colon + String (ESP.getFreeHeap ()));
            logcon (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            logcon (CN_plussigns + RawFileData + CN_minussigns);
            break;
        }

        // DEBUG__V ("Got config data");
        GotFileData = true;

    } while (false);

    // DEBUG__END;
    return GotFileData;

} // ReadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadFlashFile (const String & FileName, byte * FileData, size_t maxlen)
{
    // DEBUG_START;
    bool GotFileData = false;

    do // once
    {
        // DEBUG__V (String("File '") + FileName + "' is being opened.");
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

        // DEBUG__V (String("File '") + FileName + "' is open.");
        file.seek (0, SeekSet);
        file.read (FileData, file.size());
        file.close ();

        GotFileData = true;

        // DEBUG_V (FileData);

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
    // DEBUG__START;

    int index = 0;
    for (auto& currentFileListEntry : FileList)
    {
        currentFileListEntry.handle  = INVALID_FILE_HANDLE;
        currentFileListEntry.entryId = index++;
    }

    // DEBUG__END;

} // InitFileList

//-----------------------------------------------------------------------------
int c_FileMgr::FileListFindSdFileHandle (FileId HandleToFind)
{
    // DEBUG__START;

    int response = -1;
    // DEBUG__V (String ("HandleToFind: ") + String (HandleToFind));

    if(INVALID_FILE_HANDLE != HandleToFind)
    {
        for (auto & currentFileListEntry : FileList)
        {
            // DEBUG__V (String ("currentFileListEntry.handle: ")  + String (currentFileListEntry.handle));
            // DEBUG__V (String ("currentFileListEntry.entryId: ") + String (currentFileListEntry.entryId));

            if (currentFileListEntry.handle == HandleToFind)
            {
                response = currentFileListEntry.entryId;
                break;
            }
        }
    }

    // DEBUG__END;

    return response;
} // FileListFindSdFileHandle

//-----------------------------------------------------------------------------
c_FileMgr::FileId c_FileMgr::CreateSdFileHandle ()
{
    // DEBUG__START;

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
            // DEBUG__V(String("handle: ") + currentFileListEntry.handle);
            break;
        }
    }

    if (INVALID_FILE_HANDLE == response)
    {
        logcon (String (CN_stars) + F (" Could not allocate another file handle ") + CN_stars);
    }
    // DEBUG__V (String ("FileHandle: ") + String (FileHandle));

    // DEBUG__END;

    return response;
} // CreateFileHandle

//-----------------------------------------------------------------------------
void c_FileMgr::DeleteSdFile (const String & FileName)
{
    // DEBUG_START;
    LockSd();
    bool FileExists = ESP_SD.exists (FileName);
    UnLockSd();
    if (FileExists)
    {
        // DEBUG_V (String ("Deleting '") + FileName + "'");
        LockSd();
        ESP_SD.remove (FileName);
        UnLockSd();
        if(!FileName.equals(FSEQFILELIST))
        {
            BuildFseqList(false);
        }
    }

    // DEBUG_END;

} // DeleteSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::DescribeSdCardToUser ()
{
    // DEBUG_START;

    logcon (String (F ("SD Card Size: ")) + int64String (SdCardSizeMB) + "MB");
/*
    // DEBUG__V("Open Root");
    FsFile root;
    ESP_SD.chdir();
    root.open("/", O_READ);
    printDirectory (root, 0);
    root.close();
    // DEBUG__V("Close Root");
*/
    // DEBUG_END;

} // DescribeSdCardToUser

//-----------------------------------------------------------------------------
void c_FileMgr::GetListOfSdFiles (std::vector<String> & Response)
{
    // DEBUG_START;

    char entryName[256];
    FsFile Entry;

    Response.clear();
    LockSd();
    do // once
    {
        if (false == SdCardIsInstalled ())
        {
            // DEBUG_V("No SD Card installed");
            break;
        }

        FsFile dir;
        ESP_SD.chdir(); // Set to sd root
        if(!dir.open ("/", O_READ))
        {
            logcon("ERROR:GetListOfSdFiles: Could not open root dir");
            break;
        }

        while (Entry.openNext (&dir, O_READ))
        {
            if(!Entry.isFile() || Entry.isHidden())
            {
                // not a file we are looking for
                continue;
            }

            memset(entryName, 0x0, sizeof(entryName));
            Entry.getName(entryName, sizeof(entryName)-1);
            String EntryName = String (entryName);
            EntryName = EntryName.substring ((('/' == EntryName[0]) ? 1 : 0));
            // DEBUG_V ("EntryName: '" + EntryName + "'");
            // DEBUG_V ("EntryName.length(): " + String(EntryName.length ()));

            if ((!EntryName.isEmpty ()) &&
                (!EntryName.equals(String (F ("System Volume Information")))) &&
                (0 != Entry.size ())
               )
            {
                // DEBUG_V ("Adding File: '" + EntryName + "'");
                Response.push_back(EntryName);
            }

            Entry.close ();
        }

        dir.close();
    } while (false);
    UnLockSd();

    // DEBUG_END;
} // GetListOfSdFiles

//-----------------------------------------------------------------------------
void c_FileMgr::printDirectory (FsFile & dir, int numTabs)
{
    // DEBUG_START;

    char entryName[256];
    FsFile Entry;
    String tabs = emptyString;
    tabs.reserve(2 * (numTabs + 1));
    for (uint8_t i = 0; i < numTabs; i++)
    {
        tabs = tabs + "  ";
    }

    while (Entry.openNext(&dir, O_READ))
    {
        FeedWDT();

        memset(entryName, 0x0, sizeof(entryName));
        Entry.getName(entryName, sizeof(entryName)-1);
        // DEBUG_V(String("entryName: '") + String(entryName) + "'");

        if (Entry.isDirectory ())
        {
            logcon (tabs + F ("> ") + entryName);
            printDirectory (dir, numTabs + 1);
            Entry.close();
            continue;
        }

        if(!Entry.isHidden())
        {
            // files have sizes, directories do not
            logcon (tabs + entryName + F (" - ") + int64String(Entry.size ()));
        }
        Entry.close();
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
        if (false == OpenSdFile (FileName, FileMode::FileWrite, FileHandle, -1))
        {
            logcon (String (F ("Could not open '")) + FileName + F ("' for writting."));
            break;
        }

        int WriteCount = WriteSdFile (FileHandle, (byte*)FileData.c_str (), (uint64_t)FileData.length ());
        logcon (String (F ("Wrote '")) + FileName + F ("' ") + String(WriteCount));

        CloseSdFile (FileHandle);

    } while (false);

    // DEBUG_END;

} // SaveSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (const String & FileName, JsonVariant & FileData)
{
    // DEBUG_START;
    String Temp;
    serializeJson (FileData, Temp);
    SaveSdFile (FileName, Temp);
    // DEBUG_END;
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

        // DEBUG_V (String("FileName: '") + FileName + "'");

        if (FileMode::FileRead == Mode)
        {
            // DEBUG_V (String("Read mode"));
            if (false == ESP_SD.exists (FileName))
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

        // DEBUG_V("did we get an index");
        if (-1 != FileListIndex)
        {
            // DEBUG_V(String("Valid FileListIndex: ") + String(FileListIndex));
            FileList[FileListIndex].Filename = FileName;
            // DEBUG_V(String("Got file handle: ") + String(FileHandle));
            LockSd();
            FileList[FileListIndex].IsOpen = FileList[FileListIndex].fsFile.open(FileList[FileListIndex].Filename.c_str(), XlateFileMode[Mode]);
            UnLockSd();

            // DEBUG_V(String("ReadWrite: ") + String(XlateFileMode[Mode]));
            // xDEBUG_V(String("File Size: ") + String64(FileList[FileListIndex].fsFile.size()));
            FileList[FileListIndex].mode = Mode;

            // DEBUG_V("Open return");
            if (!FileList[FileListIndex].IsOpen)
            {
                logcon(String(F("ERROR: Could not open '")) + FileName + F("'."));
                // release the file list entry
                CloseSdFile(FileHandle);
                break;
            }
            // DEBUG_V("");

            LockSd();
            FileList[FileListIndex].size = FileList[FileListIndex].fsFile.size();
            UnLockSd();
            // DEBUG_V(String(FileList[FileListIndex].Filename) + " - " + String(FileList[FileListIndex].size));

            if (FileMode::FileWrite == Mode)
            {
                // DEBUG_V ("Write Mode");
                LockSd();
                FileList[FileListIndex].fsFile.seek (0);
                UnLockSd();
            }
            // DEBUG_V ();

            FileIsOpen = true;
            // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));
        }
        else
        {
            // DEBUG_V("Could not get a file list index");
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
    if (true == OpenSdFile (FileName, FileMode::FileRead, FileHandle, -1))
    {
        // DEBUG_V (String("File '") + FileName + "' is open.");
        int FileListIndex;
        if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            LockSd();
            FileList[FileListIndex].fsFile.seek (0);
            FileData = FileList[FileListIndex].fsFile.readString ();
            UnLockSd();
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
    if (true == OpenSdFile (FileName, FileMode::FileRead, FileHandle, -1))
    {
        // DEBUG_V (String("File '") + FileName + "' is open.");
        int FileListIndex;
        if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            LockSd();
            FileList[FileListIndex].fsFile.seek (0);
            String RawFileData = FileList[FileListIndex].fsFile.readString ();
            UnLockSd();

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
uint64_t c_FileMgr::ReadSdFile (const FileId& FileHandle, byte* FileData, uint64_t NumBytesToRead, uint64_t StartingPosition)
{
    // DEBUG_START;

    uint64_t response = 0;

    // DEBUG_V (String ("       FileHandle: ") + String (FileHandle));
    // DEBUG_V (String ("   NumBytesToRead: ") + String (NumBytesToRead));
    // DEBUG_V (String (" StartingPosition: ") + String (StartingPosition));

    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        uint64_t BytesRemaining = uint64_t(FileList[FileListIndex].size - StartingPosition);
        uint64_t ActualBytesToRead = min(NumBytesToRead, BytesRemaining);
        // DEBUG_V(String("   BytesRemaining: ") + String(BytesRemaining));
        // DEBUG_V(String("ActualBytesToRead: ") + String(ActualBytesToRead));
        LockSd();
        FileList[FileListIndex].fsFile.seek(StartingPosition);
        response = FileList[FileListIndex].fsFile.readBytes(FileData, ActualBytesToRead);
        UnLockSd();
        // DEBUG_V(String("         response: ") + String64(response));
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
        // DEBUG_V("Close the file");
        LockSd();
        FileList[FileListIndex].fsFile.close ();
        UnLockSd();

        FileList[FileListIndex].IsOpen = false;
        FileList[FileListIndex].handle = INVALID_FILE_HANDLE;

        if (nullptr != FileList[FileListIndex].buffer.DataBuffer)
        {
            if(FileList[FileListIndex].buffer.DataBuffer != OutputMgr.GetBufferAddress())
            {
                // only free the buffer if it is not malloc'd
                free(FileList[FileListIndex].buffer.DataBuffer);
            }
        }
        FileList[FileListIndex].buffer.DataBuffer = nullptr;
        FileList[FileListIndex].buffer.size = 0;
        FileList[FileListIndex].buffer.offset = 0;
    }
    else
    {
        logcon (String (F ("CloseSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }

    FileHandle = INVALID_FILE_HANDLE;

    // DEBUG_END;

} // CloseSdFile

//-----------------------------------------------------------------------------
uint64_t c_FileMgr::WriteSdFile (const FileId& FileHandle, byte* FileData, uint64_t NumBytesToWrite)
{
    // DEBUG_START;

    uint64_t NumBytesWritten = 0;
    do // once
    {
        int FileListIndex;
        // DEBUG_V (String("Bytes to write: ") + String(NumBytesToWrite));
        if (-1 == (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            // DEBUG_V (String("FileHandle: ") + String(FileHandle));
            logcon (String (F ("WriteSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
            break;
        }

        delay(10);
        FeedWDT();
        LockSd();
        NumBytesWritten = FileList[FileListIndex].fsFile.write(FileData, NumBytesToWrite);
        FileList[FileListIndex].fsFile.flush();
        UnLockSd();
        FeedWDT();
        delay(10);
        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));

        if(NumBytesWritten != NumBytesToWrite)
        {
            logcon(String(F("ERROR: SD Write failed. Tried writting ")) + String(NumBytesToWrite) + F(" bytes. Actually wrote ") + String(NumBytesWritten) + F(" bytes."));
                    NumBytesWritten = 0;
                    break;
        }
    } while(false);

    // DEBUG_END;
    return NumBytesWritten;

} // WriteSdFile

//-----------------------------------------------------------------------------
/*
    Specialized function that buffers data until the buffer is
    full and then writes out the buffer as a single operation.
*/
uint64_t c_FileMgr::WriteSdFileBuf (const FileId& FileHandle, byte* FileData, uint64_t NumBytesInSourceBuffer)
{
    // DEBUG_START;

    uint64_t NumBytesWrittenToDestBuffer = 0;
    bool ForceWriteToSD = (0 == NumBytesInSourceBuffer);
    do // once
    {
        int FileListIndex;
        // DEBUG_V (String("    NumBytesInSourceBuffer: ") + String(NumBytesInSourceBuffer));
        // DEBUG_V (String("            ForceWriteToSD: ") + String(ForceWriteToSD));
        if (-1 == (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            logcon (String (F ("WriteSdFileBuf::ERROR::Invalid File Handle: ")) + String (FileHandle));
            break;
        }

        // are we using a buffer in front of the SD card?
        if(nullptr == FileList[FileListIndex].buffer.DataBuffer)
        {
            delay(10);
            FeedWDT();

            // DEBUG_V("Not using buffers");
            LockSd();
            NumBytesWrittenToDestBuffer = FileList[FileListIndex].fsFile.write(FileData, NumBytesInSourceBuffer);
            FileList[FileListIndex].fsFile.flush();
            UnLockSd();

            delay(20);
            FeedWDT();
        }
        else // buffered mode
        {
            // DEBUG_V("Using buffers");
            // DEBUG_V(String("     NumBytesInSourceBuffer: ") + String(NumBytesInSourceBuffer));
            uint64_t SpaceRemaining = FileList[FileListIndex].buffer.size - FileList[FileListIndex].buffer.offset;
            // DEBUG_V(String("             SpaceRemaining: ") + String(SpaceRemaining));

            if(ForceWriteToSD ||
                  (NumBytesInSourceBuffer &&
                  (SpaceRemaining < NumBytesInSourceBuffer)))
            {
                // DEBUG_V("Write out the buffer");
                // delay(20);
                // DEBUG_V(String("       BytesToBeWrittenToSD: ") + String(FileList[FileListIndex].buffer.offset));
                FeedWDT();
                LockSd();
                uint64_t WroteToSdSize = FileList[FileListIndex].fsFile.write(FileList[FileListIndex].buffer.DataBuffer, FileList[FileListIndex].buffer.offset);
                FileList[FileListIndex].fsFile.flush();
                UnLockSd();
                delay(30);
                FeedWDT();
                // DEBUG_V(String("                     offset: ") + String(FileList[FileListIndex].buffer.offset));
                // DEBUG_V(String("              WroteToSdSize: ") + String(WroteToSdSize));
                if(FileList[FileListIndex].buffer.offset != WroteToSdSize)
                {
                    logcon (String("WriteSdFileBuf:ERROR:SD Write Failed. Tried to write: ") +
                            String(FileList[FileListIndex].buffer.offset) +
                            " bytes. Actually wrote: " + String(WroteToSdSize))
                    NumBytesWrittenToDestBuffer = 0;
                    break;
                } // end write failed
                // reset the buffer
                FileList[FileListIndex].buffer.offset = 0;
                // DEBUG_V(String("                 New offset: ") + String(FileList[FileListIndex].buffer.offset));
                ForceWriteToSD = false;
            }

            if(NumBytesInSourceBuffer)
            {
                // DEBUG_V(String("                    Writing ") + String(NumBytesInSourceBuffer) + " bytes to the buffer");
                // DEBUG_V(String("                     offset: ") + String(FileList[FileListIndex].buffer.offset));
                memcpy(&(FileList[FileListIndex].buffer.DataBuffer[FileList[FileListIndex].buffer.offset]), FileData, NumBytesInSourceBuffer);

                FileList[FileListIndex].buffer.offset += NumBytesInSourceBuffer;
                NumBytesWrittenToDestBuffer += NumBytesInSourceBuffer;
                NumBytesInSourceBuffer -= NumBytesInSourceBuffer;
                // DEBUG_V(String("NumBytesWrittenToDestBuffer: ") + String(NumBytesWrittenToDestBuffer));
                // DEBUG_V(String("     NumBytesInSourceBuffer: ") + String(NumBytesInSourceBuffer));
                // DEBUG_V(String("                 New offset: ") + String(FileList[FileListIndex].buffer.offset));

            }; // End write to buffer
        }
        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));

    } while(false);

    // DEBUG_END;
    return NumBytesWrittenToDestBuffer;

} // WriteSdFile

//-----------------------------------------------------------------------------
uint64_t c_FileMgr::WriteSdFile (const FileId& FileHandle, byte* FileData, uint64_t NumBytesToWrite, uint64_t StartingPosition)
{
    uint64_t response = 0;
    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        // DEBUG_V (String (" FileHandle: ") + String (FileHandle));
        // DEBUG_V (String ("File.Handle: ") + String (FileList[FileListIndex].handle));
        LockSd();
        FileList[FileListIndex].fsFile.seek (StartingPosition);
        LockSd();
        response = WriteSdFile (FileHandle, FileData, NumBytesToWrite, true);
    }
    else
    {
        logcon (String (F ("WriteSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }

    return response;

} // WriteSdFile

//-----------------------------------------------------------------------------
uint64_t c_FileMgr::GetSdFileSize (const String& FileName)
{
    // DEBUG_START;
    uint64_t response = 0;
    FileId Handle = INVALID_FILE_HANDLE;
    if(OpenSdFile (FileName,   FileMode::FileRead, Handle, -1))
    {
        response = GetSdFileSize(Handle);
        CloseSdFile(Handle);
    }
    else
    {
        logcon(String(F("Could not open '")) + FileName + F("' to check size."));
    }
    // DEBUG_END;
    return response;

} // GetSdFileSize

//-----------------------------------------------------------------------------
uint64_t c_FileMgr::GetSdFileSize (const FileId& FileHandle)
{
    // DEBUG_START;

    uint64_t response = 0;
    int FileListIndex;
    if (-1 != (FileListIndex = FileListFindSdFileHandle (FileHandle)))
    {
        LockSd();
        response = FileList[FileListIndex].fsFile.size ();
        UnLockSd();
    }
    else
    {
        logcon (String (F ("GetSdFileSize::ERROR::Invalid File Handle: ")) + String (FileHandle));
    }
    // DEBUG_V(String("response: ") + String(response));

    // DEBUG_END;
    return response;

} // GetSdFileSize

//-----------------------------------------------------------------------------
void c_FileMgr::RenameSdFile(String & OldName, String & NewName)
{
    // DEBUG_START;
    // DEBUG_V(String("OldName: '") + OldName + "'");
    // DEBUG_V(String("NewName: '") + NewName + "'");

    LockSd();
    // only do this in the root dir
    ESP_SD.chdir();
    // rename does not work if the target already exists
    ESP_SD.remove(NewName);
    if(!ESP_SD.rename(OldName, NewName))
    {
        logcon(String(CN_stars) + F("Could not rename '") + OldName + F("' to '") + NewName + F("'") + CN_stars);
    }
    UnLockSd();
    // DEBUG_END;
} // RenameSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::BuildFseqList(bool DisplayFileNames)
{
    // DEBUG_START;
    char entryName [256];

    do // once
    {
        if(!SdCardIsInstalled())
        {
            // DEBUG_V("No SD card installed.");
            break;
        }

        FeedWDT();

        LockSd();
        FsFile InputFile;
        ESP_SD.chdir(); // Set to sd root
        if(!InputFile.open ("/", O_READ))
        {
            UnLockSd();
            logcon(F("ERROR: Could not open SD card for Reading FSEQ List."));
            break;
        }

        // open output file, erase old data
        ESP_SD.chdir(); // Set to sd root
        UnLockSd();
        DeleteSdFile(String(F(FSEQFILELIST)));
        LockSd();
        FsFile OutputFile;
        if(!OutputFile.open (String(F(FSEQFILELIST)).c_str(), O_WRITE | O_CREAT))
        {
            InputFile.close();
            UnLockSd();
            logcon(F("ERROR: Could not open SD card for writting FSEQ List."));
            break;
        }
        OutputFile.print("{");
        // LOG_PORT.print("{");
        // DEBUG_V();

        String Entry = String(F("\"totalBytes\" : ")) + int64String(SdCardSizeMB * 1024 * 1024) + ",";
        OutputFile.print(Entry);
        // LOG_PORT.print(Entry);

        uint64_t usedBytes = 0;
        uint32_t numFiles = 0;

        OutputFile.print("\"files\" : [");
        // LOG_PORT.print("\"files\" : [");
        FsFile CurrentEntry;
        while (CurrentEntry.openNext (&InputFile, O_READ))
        {
            // DEBUG_V("Process a file entry");
            FeedWDT();

            if(CurrentEntry.isDirectory() || CurrentEntry.isHidden())
            {
                // DEBUG_V("Skip embedded directory and hidden files");
                CurrentEntry.close();
                continue;
            }

            memset(entryName, 0x0, sizeof(entryName));
            CurrentEntry.getName (entryName, sizeof(entryName)-1);
            String EntryName = String (entryName);
            // DEBUG__V (         "EntryName: " + EntryName);
            // DEBUG__V ("EntryName.length(): " + String(EntryName.length ()));
            // DEBUG__V ("      entry.size(): " + int64String(CurrentEntry.size ()));

            if ((!EntryName.isEmpty ()) &&
//                (!EntryName.equals(String (F ("System Volume Information")))) &&
                (0 != CurrentEntry.size () &&
                !EntryName.equals(FSEQFILELIST))
               )
            {
                // is this a zipped file?
                if((-1 != EntryName.indexOf(F(".zip"))) ||
                   (-1 != EntryName.indexOf(F(".ZIP"))) ||
                   (-1 != EntryName.indexOf(F(".xlz")))
                   )
                {
                    FoundZipFile = true;
                }

                // do we need to add a separator?
                if(numFiles)
                {
                    // DEBUG__V("Adding trailing comma");
                    OutputFile.print(",");
                    // LOG_PORT.print(",");
                }

                usedBytes += CurrentEntry.size ();
                ++numFiles;

                if(DisplayFileNames)
                {
                    logcon (String(F("SD File: '")) + EntryName + "'   " + String(CurrentEntry.size ()));
                }
                uint16_t Date;
                uint16_t Time;
                CurrentEntry.getCreateDateTime (&Date, &Time);
                // DEBUG__V(String("Date: ") + String(Date));
                // DEBUG__V(String("Year: ") + String(FS_YEAR(Date)));
                // DEBUG__V(String("Day: ") + String(FS_DAY(Date)));
                // DEBUG__V(String("Month: ") + String(FS_MONTH(Date)));

                // DEBUG__V(String("Time: ") + String(Time));
                // DEBUG__V(String("Hours: ") + String(FS_HOUR(Time)));
                // DEBUG__V(String("Minutes: ") + String(FS_MINUTE(Time)));
                // DEBUG__V(String("Seconds: ") + String(FS_SECOND(Time)));

                tmElements_t tm;
                tm.Year = FS_YEAR(Date) - 1970;
                tm.Month = FS_MONTH(Date);
                tm.Day = FS_DAY(Date);
                tm.Hour = FS_HOUR(Time);
                tm.Minute = FS_MINUTE(Time);
                tm.Second = FS_SECOND(Time);

                // DEBUG__V(String("tm: ") + String(time_t(makeTime(tm))));

                OutputFile.print(String("{\"name\" : \"") + EntryName +
                                 "\",\"date\" : " + String(makeTime(tm)) +
                                 ",\"length\" : " + int64String(CurrentEntry.size ()) + "}");
                /*LOG_PORT.print(String("{\"name\" : \"") + EntryName +
                                 "\",\"date\" : " + String(Date) +
                                 ",\"length\" : " + int64String(CurrentEntry.size ()) + "}");
                */
            }
            else
            {
                // DEBUG__V("Skipping File");
            }
            CurrentEntry.close();
        } // end while true

        // close the array and add the descriptive data
        OutputFile.print(String("], \"usedBytes\" : ") + int64String(usedBytes));
        // LOG_PORT.print(String("], \"usedBytes\" : ") + int64String(usedBytes));
        OutputFile.print(String(", \"numFiles\" : ") + String(numFiles));
        OutputFile.print(String(", \"SdCardPresent\" : true"));

        // close the data section
        OutputFile.print("}");
        // LOG_PORT.println("}");

        OutputFile.close();
        InputFile.close();
        UnLockSd();
    } while(false);

    // String Temp;
    // ReadSdFile(FSEQFILELIST, Temp);
    // xDEBUG_V(Temp);

    // DEBUG__END;

} // BuildFseqList

//-----------------------------------------------------------------------------
void c_FileMgr::FindFirstZipFile(String &FileName)
{
    // DEBUG_START;
    char entryName [256];

    do // once
    {
        if(!SdCardIsInstalled())
        {
            // DEBUG_V("No SD card installed.");
            break;
        }

        FeedWDT();
        LockSd();
        FsFile InputFile;
        ESP_SD.chdir(); // Set to sd root
        if(!InputFile.open ("/", O_READ))
        {
            UnLockSd();
            logcon(F("ERROR: Could not open SD card for Reading FSEQ List."));
            break;
        }

        // open output file, erase old data
        ESP_SD.chdir(); // Set to sd root

        FsFile CurrentEntry;
        while (CurrentEntry.openNext (&InputFile, O_READ))
        {
            // DEBUG__V("Process a file entry");
            FeedWDT();

            if(CurrentEntry.isDirectory() || CurrentEntry.isHidden())
            {
                // DEBUG__V("Skip embedded directory and hidden files");
                CurrentEntry.close();
                continue;
            }

            memset(entryName, 0x0, sizeof(entryName));
            CurrentEntry.getName (entryName, sizeof(entryName)-1);
            String EntryName = String (entryName);
            // DEBUG__V (         "EntryName: " + EntryName);
            // DEBUG__V ("EntryName.length(): " + String(EntryName.length ()));
            // DEBUG__V ("      entry.size(): " + int64String(CurrentEntry.size ()));

            // is this a zipped file?
            if((-1 != EntryName.indexOf(F(".zip"))) ||
               (-1 != EntryName.indexOf(F(".ZIP"))) ||
               (-1 != EntryName.indexOf(F(".xlz"))))
            {
                FileName = EntryName;
                CurrentEntry.close();
                InputFile.close();
                break;
            }
            else
            {
                // DEBUG__V("Skipping File");
            }
            CurrentEntry.close();
        } // end while true

        InputFile.close();
        UnLockSd();
    } while(false);

    // DEBUG_END;

} // FindFirstZipFile

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
        FeedWDT();
        if ((0 == index))
        {
            // DEBUG_V("New File");
            handleFileUploadNewFile (filename);
            expectedIndex = 0;
            LOG_PORT.println(".");
        }

        if (index != expectedIndex)
        {
            if(fsUploadFileHandle != INVALID_FILE_HANDLE)
            {
                logcon (String(F("ERROR: Expected index: ")) + String(expectedIndex) + F(" does not match actual index: ") + String(index));

                CloseSdFile (fsUploadFileHandle);
                DeleteSdFile (fsUploadFileName);
                delay(100);
                BuildFseqList(false);
                expectedIndex = 0;
                fsUploadFileName = emptyString;
            }
            break;
        }

        // DEBUG_V (String ("fsUploadFileName: ") + String (fsUploadFileName));

        // update the next expected chunk id
        expectedIndex = index + len;
        size_t bytesWritten = 0;

        if (len)
        {
            // Write data
            // DEBUG_V ("UploadWrite: " + String (len) + String (" bytes"));
            bytesWritten = WriteSdFileBuf (fsUploadFileHandle, data, len);
            // DEBUG_V (String ("Writing bytes: ") + String (index));
            LOG_PORT.println(String("\033[Fprogress: ") + String(expectedIndex) + ", heap: " + String(ESP.getFreeHeap ()));
            LOG_PORT.flush();
        }
        // PauseSdFile(fsUploadFile);

        if(len != bytesWritten)
        {
            // DEBUG_V("Write failed. Stop transfer");
            CloseSdFile(fsUploadFileHandle);
            DeleteSdFile (fsUploadFileName);
            expectedIndex = 0;
            fsUploadFileName = emptyString;
            break;
        }
        response = true;
    } while(false);

    if ((true == final) && (fsUploadFileHandle != INVALID_FILE_HANDLE))
    {
        // cause the remainder in the buffer to be written.
        WriteSdFileBuf (fsUploadFileHandle, data, 0);
        uint32_t uploadTime = (uint32_t)(millis() - fsUploadStartTime) / 1000;
        FeedWDT();
        CloseSdFile (fsUploadFileHandle);

        logcon (String (F ("Upload File: '")) + fsUploadFileName +
                F ("' Done (") + String (uploadTime) +
                F ("s). Received: ") + String(expectedIndex) +
                F(" Bytes out of ") + String(totalLen) +
                F(" bytes. FileLen: ") + GetSdFileSize(filename));

        FeedWDT();
        expectedIndex = 0;

        delay(100);
        BuildFseqList(false);

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
    // DEBUG_V ("UploadStart: " + filename);

    fsUploadStartTime = millis();

    // are we terminating the previous download?
    if (0 != fsUploadFileName.length ())
    {
        logcon (String (F ("Aborting Previous File Upload For: '")) + fsUploadFileName + String (F ("'")));
        CloseSdFile (fsUploadFileHandle);
        fsUploadFileName = "";
    }

    // Set up to receive a file
    fsUploadFileName = filename;

    logcon (String (F ("Upload File: '")) + fsUploadFileName + String (F ("' Started")));

    DeleteSdFile (fsUploadFileName);

    // Open the file for writing
    OpenSdFile (fsUploadFileName, FileMode::FileWrite, fsUploadFileHandle, -1 /*first access*/);
    int FileListIndex;
    if (-1 == (FileListIndex = FileListFindSdFileHandle (fsUploadFileHandle)))
    {
        logcon (String (F ("WriteSdFileBuf::ERROR::Invalid File Handle: ")) + String (fsUploadFileHandle));
    }
    else
    {
        // DEBUG_V("Use the output buffer as a data buffer");
        FileList[FileListIndex].buffer.offset = 0;
        FileList[FileListIndex].buffer.size = min(uint32_t(OutputMgr.GetBufferSize() & ~(SD_BLOCK_SIZE - 1)), uint32_t(MAX_SD_BUFFER_SIZE));
        FileList[FileListIndex].buffer.DataBuffer = OutputMgr.GetBufferAddress();
        // DEBUG_V(String("Buffer Size: ") + String(FileList[FileListIndex].buffer.size));
    }

    // DEBUG_END;

} // handleFileUploadNewFile

//-----------------------------------------------------------------------------
uint64_t c_FileMgr::GetDefaultFseqFileList(uint8_t * buffer, uint64_t maxlen)
{
    // DEBUG_START;

    memset(buffer, 0x0, maxlen);
    strcpy_P((char*)&buffer[0], DefaultFseqResponse);

    // DEBUG__V(String("buffer: ") + String((char*)buffer));
    // DEBUG_END;

    return strlen((char*)&buffer[0]);
} // GetDefaultFseqFileList

//-----------------------------------------------------------------------------
bool c_FileMgr::SeekSdFile(const FileId & FileHandle, uint64_t position, SeekMode Mode)
{
    // DEBUG_START;

    // DEBUG_V(String("FileHandle: ") + String(FileHandle));
    // DEBUG_V(String("  position: ") + String(position));
    // DEBUG_V(String("      Mode: ") + String(uint32_t(Mode)));

    bool response = false;
    int FileListIndex;
    LockSd();
    do // once
    {
        if (-1 == (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            logcon (String (F ("SeekSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
            break;
        }

        switch(Mode)
        {
            case SeekMode::SeekSet:
            {
                response = FileList[FileListIndex].fsFile.seek (position);
                break;
            }
            case SeekMode::SeekEnd:
            {
                uint64_t EndPosition = FileList[FileListIndex].fsFile.size();
                response = FileList[FileListIndex].fsFile.seek (EndPosition - position);
                break;
            }
            case SeekMode::SeekCur:
            {
                uint64_t CurrentPosition = FileList[FileListIndex].fsFile.position();
                response = FileList[FileListIndex].fsFile.seek (CurrentPosition + position);
                break;
            }
            default:
            {
                logcon("Procedural error. Cannot set seek value");
                break;
            }
        } // end switch mode
    } while(false);
    UnLockSd();

    // DEBUG_END;
    return response;

} // SeekSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::LockSd()
{
    // DEBUG_START;
    // DEBUG_V(String("SdAccessSemaphore: ") + SdAccessSemaphore);

    // wait to get access to the SD card
    while(true == SdAccessSemaphore)
    {
        delay(10);
    }
    SdAccessSemaphore = true;

    // DEBUG_END;
} // LockSd

//-----------------------------------------------------------------------------
void c_FileMgr::UnLockSd()
{
    // DEBUG_START;

    // DEBUG_V(String("SdAccessSemaphore: ") + SdAccessSemaphore);
    SdAccessSemaphore = false;
    // DEBUG_END;
} // UnLockSd

//-----------------------------------------------------------------------------
void c_FileMgr::AbortSdFileUpload()
{
    // DEBUG_START;

    do // once
    {
        if(fsUploadFileHandle == INVALID_FILE_HANDLE)
        {
            // DEBUG_V("No File Transfer in progress");
            break;
        }

        CloseSdFile(fsUploadFileHandle);

    } while(false);

    // DEBUG_END;
} // AbortSdFileUpload

// create a global instance of the File Manager
c_FileMgr FileMgr;
