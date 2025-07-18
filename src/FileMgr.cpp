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
#include "input/InputMgr.hpp"
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
            InputMgr.SetOperationalState(false);
            LOG_PORT.println(F("FTP: Connected!"));
            break;
        }

        case FTP_DISCONNECT:
        {
            InputMgr.SetOperationalState(true);
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
            // size_t free = heap_caps_get_largest_free_block(0x1800);
            // LOG_PORT.printf("heap: 0x%x\n", free);
            break;
        }

        case FTP_UPLOAD:
        {
            // LOG_PORT.printf("FTP: Upload of file %s byte %u\n", name, transferredSize);
            // size_t free = heap_caps_get_largest_free_block(0x1800);
            // LOG_PORT.printf("heap: 0x%x\n", free);
            break;
        }

        case FTP_TRANSFER_STOP:
        {
            LOG_PORT.println(String(F("FTP: Done Uploading '")) + name + "'");
            // size_t free = heap_caps_get_largest_free_block(0x1800);
            // LOG_PORT.printf("heap: 0x%x\n", free);
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
///< Start up the driver and put it into a safe mode
c_FileMgr::c_FileMgr ()
{
#ifdef ARDUINO_ARCH_ESP32
    SdAccessSemaphore = xSemaphoreCreateBinary();
    UnLockSd();
#endif // def ARDUINO_ARCH_ESP32
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

        if(FoundZipFile)
        {
            FeedWDT();
        #ifdef SUPPORT_UNZIP
            UnzipFiles * Unzipper = new UnzipFiles();
            Unzipper->Run();
            delete Unzipper;
            String Reason = F("Requesting reboot after unzipping files");
            RequestReboot(Reason, 1, true);
        #endif // def SUPPORT_UNZIP
        }

    } while (false);

    // DEBUG_END;
} // begin

//-----------------------------------------------------------------------------
//    Cause the FTP operation to get re-established.
void c_FileMgr::UpdateFtp()
{
    // DEBUG_START;

    NetworkStateChanged(NetworkMgr.IsConnected());

    // DEBUG_END;
} // UpdateFtp

//-----------------------------------------------------------------------------
void c_FileMgr::NetworkStateChanged (bool NewState)
{
    // DEBUG_START;
#ifdef SUPPORT_FTP
    // DEBUG_V("Disable FTP");
    ftpSrv.end();

    // DEBUG_V(String("       NewState: ") + String(NewState));
    // DEBUG_V(String("SdCardInstalled: ") + String(SdCardInstalled));
    // DEBUG_V(String("     FtpEnabled: ") + String(FtpEnabled));
    if(NewState && SdCardInstalled && FtpEnabled)
    {
        logcon("Starting FTP server.");
        ftpSrv.setCallback(ftp_callback);
        ftpSrv.setTransferCallback(ftp_transferCallback);
        ftpSrv.begin(FtpUserName, FtpPassword, WelcomeString);
    }
    else
    {
        // DEBUG_V("Not starting FTP service");
    }
#endif // def SUPPORT_FTP

    // DEBUG_END;
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
    // DEBUG_START;

    bool ConfigChanged = false;
    JsonObject JsonDeviceConfig = json[(char*)CN_device].as<JsonObject>();
    if (JsonDeviceConfig)
    {
        // PrettyPrint (JsonDeviceConfig, "c_FileMgr");

        // DEBUG_V("miso_pin: " + String(miso_pin));
        // DEBUG_V("mosi_pin: " + String(mosi_pin));
        // DEBUG_V(" clk_pin: " + String(clk_pin));
        // DEBUG_V("  cs_pin: " + String(cs_pin));

        ConfigChanged |= setFromJSON (miso_pin,   JsonDeviceConfig, CN_miso_pin);
        ConfigChanged |= setFromJSON (mosi_pin,   JsonDeviceConfig, CN_mosi_pin);
        ConfigChanged |= setFromJSON (clk_pin,    JsonDeviceConfig, CN_clock_pin);
        ConfigChanged |= setFromJSON (cs_pin,     JsonDeviceConfig, CN_cs_pin);
        ConfigChanged |= setFromJSON (MaxSdSpeed, JsonDeviceConfig, CN_sdspeed);

        ConfigChanged |= setFromJSON (FtpUserName, JsonDeviceConfig, CN_user);
        ConfigChanged |= setFromJSON (FtpPassword, JsonDeviceConfig, CN_password);
        ConfigChanged |= setFromJSON (FtpEnabled,  JsonDeviceConfig, CN_enabled);

        // DEBUG_V("miso_pin: " + String(miso_pin));
        // DEBUG_V("mosi_pin: " + String(mosi_pin));
        // DEBUG_V(" clk_pin: " + String(clk_pin));
        // DEBUG_V("  cs_pin: " + String(cs_pin));
        // DEBUG_V("   Speed: " + String(MaxSdSpeed));

        // DEBUG_V("ConfigChanged: " + String(ConfigChanged));
    }
    else
    {
        logcon (F ("No File Manager settings found."));
    }

    // DEBUG_V (String ("ConfigChanged: ") + String (ConfigChanged));

    if (ConfigChanged)
    {
        SetSpiIoPins ();
        NetworkStateChanged(NetworkMgr.IsConnected());
    }

    // DEBUG_END;

    return ConfigChanged;

} // SetConfig

//-----------------------------------------------------------------------------
void c_FileMgr::GetConfig (JsonObject& json)
{
    // DEBUG_START;

    JsonWrite(json, CN_miso_pin,  miso_pin);
    JsonWrite(json, CN_mosi_pin,  mosi_pin);
    JsonWrite(json, CN_clock_pin, clk_pin);
    JsonWrite(json, CN_cs_pin,    cs_pin);
    JsonWrite(json, CN_sdspeed,   MaxSdSpeed);

    JsonWrite(json, CN_user,      FtpUserName);
    JsonWrite(json, CN_password,  FtpPassword);
    JsonWrite(json, CN_enabled,   FtpEnabled);

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
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtDisable();
#endif // def ARDUINO_ARCH_ESP8266

#if defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)
    if (SdCardInstalled)
    {
        // DEBUG_V("Terminate current SD session");
        ESP_SD.end ();
    }

    FsDateTime::setCallback(dateTime);
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
        ResetGpio(gpio_num_t(cs_pin));
        pinMode(cs_pin, OUTPUT);
#       ifdef USE_MISO_PULLUP
        // DEBUG_V("USE_MISO_PULLUP");
        // on some hardware MISO is missing a required pull-up resistor, use internal pull-up.
        ResetGpio(gpio_num_t(mosi_pin));
        pinMode(miso_pin, INPUT_PULLUP);
#       else
        // DEBUG_V();
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
        // DEBUG_V();
        if (!ESP_SD.begin (SdSpiConfig(cs_pin, SHARED_SPI, SD_SCK_MHZ(16))))
#endif // !def SUPPORT_SD_MMC
        {
            // DEBUG_V();
            logcon(String(F("No SD card installed")));
            SdCardInstalled = false;
            // DEBUG_V();
            if(nullptr == ESP_SD.card())
            {
                logcon(F("SD 'Card' setup failed."));
            }
            else if (ESP_SD.card()->errorCode())
            {
                logcon(String(F("SD initialization failed - code: ")) + String(ESP_SD.card()->errorCode()));
                // DEBUG_V(String(F("SD initialization failed - data: ")) + String(ESP_SD.card()->errorData()));
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
            // DEBUG_V("SdCardInstalled = true");
            SdCardInstalled = true;
            // DEBUG_V();
            SetSdSpeed ();

            // DEBUG_V(String("SdCardSizeMB: ") + String(SdCardSizeMB));

            DescribeSdCardToUser ();
            // DEBUG_V();
        }

        BuildFseqList(true);
        // DEBUG_V();
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

#ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtEnable(uint32_t(0));
#endif // def ARDUINO_ARCH_ESP8266
    // DEBUG_END;

} // SetSpiIoPins

//-----------------------------------------------------------------------------
void c_FileMgr::SetSdSpeed ()
{
    // DEBUG_START;
#if defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)
    if (SdCardInstalled)
    {
        // DEBUG_V();
        csd_t csd;
        uint8_t tran_speed = 0;

        CSD MyCsd;
        // DEBUG_V();
        ESP_SD.card()->readCSD(&csd);
        memcpy (&MyCsd, &csd.csd[0], sizeof(csd.csd));
        // DEBUG_V(String("TRAN Speed: 0x") + String(MyCsd.Decode_0.tran_speed,HEX));
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
        // DEBUG_V();
        FinalTranSpeedMHz = min(FinalTranSpeedMHz, MaxSdSpeed);
        SPI.setFrequency(FinalTranSpeedMHz * 1024 * 1024);
        logcon(String("Set SD speed to ") + String(FinalTranSpeedMHz) + "Mhz");

        SdCardSizeMB = 0.000512 * csd.capacity();
        // DEBUG_V(String("SdCardSizeMB: ") + String(SdCardSizeMB));
    }
#endif // defined (SUPPORT_SD) || defined(SUPPORT_SD_MMC)

    // DEBUG_END;

} // SetSdSpeed

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
void c_FileMgr::DeleteFlashFile (String FileName)
{
    // DEBUG_START;

    ConnrectFilename(FileName);

    if(LittleFS.exists(FileName)) { LittleFS.remove (FileName); }

    if(!FileName.equals(CN_fseqfilelist))
    {
        BuildFseqList(false);
    }

    // DEBUG_END;

} // DeleteConfigFile

//-----------------------------------------------------------------------------
void c_FileMgr::RenameFlashFile (String OldName, String NewName)
{
     // DEBUG_START;

    ConnrectFilename(OldName);
    ConnrectFilename(NewName);

     // DEBUG_V(String("OldName: ") + OldName);
     // DEBUG_V(String("NewName: ") + NewName);

    DeleteFlashFile(NewName);
    if(!LittleFS.rename(OldName, NewName))
    {
        logcon(String(CN_stars) + F("Could not rename '") + OldName + F("' to '") + NewName + F("'") + CN_stars);
    }

     // DEBUG_END;
} // RenameFlashFile

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

        JsonDocument jsonDoc;
        jsonDoc.to<JsonObject>();

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
    	    //  xDEBUG_V (String (" getMaxFreeBlockSize: ") + String (ESP.getMaxFreeBlockSize ()));
        	// xDEBUG_V (String ("           file.size: ") + String (file.size ()));
            break;
        }

        // DEBUG_V ();
        logcon (CfgFileMessagePrefix + String (F ("loaded.")));

        // DEBUG_V ();
        Handler (jsonDoc);

        // DEBUG_V();
        retval = true;

    } while (false);

    // DEBUG_END;
    return retval;

} // LoadFlashFile

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
bool c_FileMgr::SaveFlashFile(const String & FileName, uint32_t index, uint8_t *data, uint32_t len, bool final)
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
    LockSd();
    bool FileExists = ESP_SD.exists (FileName);
    UnLockSd();
    if (FileExists)
    {
        // DEBUG_V (String ("Deleting '") + FileName + "'");
        LockSd();
        ESP_SD.remove (FileName);
        UnLockSd();
    }
    BuildFseqList(false);

    // DEBUG_END;

} // DeleteSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::DescribeSdCardToUser ()
{
    // DEBUG_START;

    logcon (String (F ("SD Card Size: ")) + int64String (SdCardSizeMB) + "MB");
/*
    // DEBUG_V("Open Root");
    FsFile root;
    ESP_SD.chdir();
    root.open("/", O_READ);
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

         // DEBUG_FILE_HANDLE (FileHandle);
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
            memset(FileList[FileListIndex].Filename, 0x0, sizeof(FileList[FileListIndex].Filename));
            strncpy(FileList[FileListIndex].Filename, FileName.c_str(), min(uint(sizeof(FileList[FileListIndex].Filename) - 1), FileName.length()));
            // DEBUG_V(String("Got file handle: ") + String(FileHandle));
            LockSd();
            FileList[FileListIndex].IsOpen = FileList[FileListIndex].fsFile.open(FileList[FileListIndex].Filename, XlateFileMode[Mode]);
            UnLockSd();

            // DEBUG_V(String("ReadWrite: ") + String(XlateFileMode[Mode]));
            // xDEBUG_V(String("File Size: ") + String64(FileList[FileListIndex].fsFile.size()));
            FileList[FileListIndex].mode = Mode;

            // DEBUG_V("Open return");
            if (!FileList[FileListIndex].IsOpen)
            {
                logcon(String(F("ERROR: Could not open '")) + FileName + F("'."));
                // release the file list entry
                 // DEBUG_FILE_HANDLE (FileHandle);
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

         // DEBUG_FILE_HANDLE (FileHandle);
        CloseSdFile (FileHandle);
        GotFileData = (0 != FileData.length());

        // DEBUG_V (FileData);
    }
    else
    {
         // DEBUG_FILE_HANDLE (FileHandle);
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
         // DEBUG_FILE_HANDLE (FileHandle);
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
        UnLockSd();
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
         // DEBUG_FILE_HANDLE (Handle);
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
            BuildDefaultFseqList();
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
        JsonDocument jsonDoc;
        jsonDoc.to<JsonObject>();

        // open output file, erase old data
        ESP_SD.chdir(); // Set to sd root
        // DEBUG_V();
        JsonWrite(jsonDoc, "totalBytes", SdCardSizeMB * 1024 * 1024);

        uint64_t usedBytes = 0;
        uint32_t numFiles = 0;

        JsonArray jsonDocFileList = jsonDoc["files"].to<JsonArray> ();
        uint32_t FileIndex = 0;
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
            // DEBUG_V (         "EntryName: " + EntryName);
            // DEBUG_V ("EntryName.length(): " + String(EntryName.length ()));
            // DEBUG_V ("      entry.size(): " + int64String(CurrentEntry.size ()));

            if ((!EntryName.isEmpty ()) &&
                (0 != CurrentEntry.size ())
               )
            {
                String LowerEntryName = EntryName;
                LowerEntryName.toLowerCase();
                // is this a zipped file?
                if((-1 != LowerEntryName.indexOf(F(".zip"))) ||
                   (-1 != LowerEntryName.indexOf(F(".xlz")))
                   )
                {
                    FoundZipFile = true;
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
                // DEBUG_V(String("Date: ") + String(Date));
                // DEBUG_V(String("Year: ") + String(FS_YEAR(Date)));
                // DEBUG_V(String("Day: ") + String(FS_DAY(Date)));
                // DEBUG_V(String("Month: ") + String(FS_MONTH(Date)));

                // DEBUG_V(String("Time: ") + String(Time));
                // DEBUG_V(String("Hours: ") + String(FS_HOUR(Time)));
                // DEBUG_V(String("Minutes: ") + String(FS_MINUTE(Time)));
                // DEBUG_V(String("Seconds: ") + String(FS_SECOND(Time)));

                tmElements_t tm;
                tm.Year = FS_YEAR(Date) - 1970;
                tm.Month = FS_MONTH(Date);
                tm.Day = FS_DAY(Date);
                tm.Hour = FS_HOUR(Time);
                tm.Minute = FS_MINUTE(Time);
                tm.Second = FS_SECOND(Time);

                // DEBUG_V(String("tm: ") + String(time_t(makeTime(tm))));
                jsonDocFileList[FileIndex]["name"] = EntryName;
                jsonDocFileList[FileIndex]["date"] = makeTime(tm);
                jsonDocFileList[FileIndex]["length"] = CurrentEntry.size ();
                ++FileIndex;
            }
            else
            {
                // DEBUG_V("Skipping File");
            }
            CurrentEntry.close();
        } // end while true

        InputFile.close();
        UnLockSd();

        // close the array and add the descriptive data
        JsonWrite(jsonDoc, "usedBytes", usedBytes);
        JsonWrite(jsonDoc, "numFiles", numFiles);
        JsonWrite(jsonDoc, "SdCardPresent", true);
        // PrettyPrint (jsonDoc, String ("FSEQ File List"));
        SaveFlashFile(String(CN_fseqfilelist) + F(".json"), jsonDoc);
    } while(false);

    // String Temp;
    // ReadFlashFile(FSEQFILELIST, Temp);
    // xDEBUG_V(Temp);

    // DEBUG_END;

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
            // DEBUG_V (         "EntryName: " + EntryName);
            // DEBUG_V ("EntryName.length(): " + String(EntryName.length ()));
            // DEBUG_V ("      entry.size(): " + int64String(CurrentEntry.size ()));

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
                // DEBUG_V("Skipping File");
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

    // try to keep a reboot from causing a corrupted file
    DelayReboot(10000);

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
            // LOG_PORT.println(".");
        }

        if (index != expectedIndex)
        {
            if(fsUploadFileHandle != INVALID_FILE_HANDLE)
            {
                logcon (String(F("ERROR: Expected index: ")) + String(expectedIndex) + F(" does not match actual index: ") + String(index));

                // DEBUG_FILE_HANDLE (fsUploadFileHandle);
                // DEBUG_V(String("fsUploadFileName: ") + String(fsUploadFileName));
                CloseSdFile (fsUploadFileHandle);
                // DEBUG_V(String("fsUploadFileName: ") + String(fsUploadFileName));
                DeleteSdFile (fsUploadFileName);
                delay(100);
                BuildFseqList(false);
                expectedIndex = 0;
                memset(fsUploadFileName, 0x0, sizeof(fsUploadFileName));
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
#ifdef ARDUINO_ARCH_ESP32
            LOG_PORT.println(String("\033[Fprogress: ") + String(expectedIndex) + ", heap: " + String(heap_caps_get_largest_free_block(0x1800)));
#else
            LOG_PORT.println(String("\033[Fprogress: ") + String(expectedIndex) + ", heap: " + String(ESP.getFreeHeap ()));
#endif // def ARDUINO_ARCH_ESP32
            LOG_PORT.flush();
        }
        // PauseSdFile(fsUploadFile);

        if(len != bytesWritten)
        {
            // DEBUG_V("Write failed. Stop transfer");
             // DEBUG_FILE_HANDLE (fsUploadFileHandle);
            CloseSdFile(fsUploadFileHandle);
            DeleteSdFile (fsUploadFileName);
            expectedIndex = 0;
            fsUploadFileName[0] = '\0';
            break;
        }
        response = true;
    } while(false);

    if ((true == final) && (fsUploadFileHandle != INVALID_FILE_HANDLE))
    {
        // DEBUG_V(String("fsUploadFileName: ") + String(fsUploadFileName));
        // cause the remainder in the buffer to be written.
        WriteSdFileBuf (fsUploadFileHandle, data, 0);
        uint32_t uploadTime = (uint32_t)(millis() - fsUploadStartTime) / 1000;
        FeedWDT();
         // DEBUG_FILE_HANDLE (fsUploadFileHandle);
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
        String temp = String(fsUploadFileName);
        temp.toLowerCase();
        if(temp.indexOf(".xlz"))
        {
            String reason = F("Reboot after receiving a compressed file");
            RequestReboot(reason, 100000);
        }

        memset(fsUploadFileName, 0x0, sizeof(fsUploadFileName));
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
    // DEBUG_V(String("filename: ") + String(filename));

    // are we terminating the previous download?
    if (0 != strlen(fsUploadFileName))
    {
        logcon (String (F ("Aborting Previous File Upload For: '")) + fsUploadFileName + String (F ("'")));
         // DEBUG_FILE_HANDLE (fsUploadFileHandle);
        CloseSdFile (fsUploadFileHandle);
    }

    // Set up to receive a file
    memset(fsUploadFileName, 0x0, sizeof(fsUploadFileName));
    strncpy(fsUploadFileName, filename.c_str(), min(uint(sizeof(fsUploadFileName) - 1), filename.length()));

    logcon (String (F ("Upload File: '")) + fsUploadFileName + String (F ("' Started")));

    DeleteSdFile (fsUploadFileName);

    // Open the file for writing
    // DEBUG_V(String("fsUploadFileName: ") + String(fsUploadFileName));
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
void c_FileMgr::BuildDefaultFseqList ()
{
    // DEBUG_START;

    JsonDocument jsonDoc;
    jsonDoc.to<JsonObject>();
    JsonWrite(jsonDoc, "SdCardPresent", false);
    JsonWrite(jsonDoc, "totalBytes", 0);
    JsonWrite(jsonDoc, "usedBytes", 0);
    JsonWrite(jsonDoc, "numFiles", 0);
    jsonDoc["files"].to<JsonArray> ();
    SaveFlashFile(String(CN_fseqfilelist) + F(".json"), jsonDoc);

    // DEBUG_END;

    return;
} // BuildDefaultFseqList

//-----------------------------------------------------------------------------
bool c_FileMgr::SeekSdFile(const FileId & FileHandle, uint64_t position, SeekMode Mode)
{
    // DEBUG_START;

    // DEBUG_V(String("FileHandle: ") + String(FileHandle));
    // DEBUG_V(String("  position: ") + String(position));
    // DEBUG_V(String("      Mode: ") + String(uint32_t(Mode)));

    bool response = false;
    int FileListIndex;
    do // once
    {
        if (-1 == (FileListIndex = FileListFindSdFileHandle (FileHandle)))
        {
            logcon (String (F ("SeekSdFile::ERROR::Invalid File Handle: ")) + String (FileHandle));
            break;
        }

        LockSd();
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
        UnLockSd();
    } while(false);

    // DEBUG_END;
    return response;

} // SeekSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::LockSd()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP32
    xSemaphoreTake( SdAccessSemaphore, TickType_t(-1) );
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;
} // LockSd

//-----------------------------------------------------------------------------
void c_FileMgr::UnLockSd()
{
    // DEBUG_START;
#ifdef ARDUINO_ARCH_ESP32
    xSemaphoreGive( SdAccessSemaphore );
#endif // def ARDUINO_ARCH_ESP32

    // DEBUG_END;
} // UnLockSd

//-----------------------------------------------------------------------------
void c_FileMgr::AbortSdFileUpload()
{
    // DEBUG_START;

    if(fsUploadFileHandle != INVALID_FILE_HANDLE)
    {
        // DEBUG_FILE_HANDLE (fsUploadFileHandle);
        CloseSdFile(fsUploadFileHandle);
    }

    // DEBUG_END;
} // AbortSdFileUpload

// create a global instance of the File Manager
c_FileMgr FileMgr;
