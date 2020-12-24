/*
* FileMgr.cpp - Output Management class
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
#include <Int64String.h>

#ifdef ARDUINO_ARCH_ESP8266
#else
#endif // def ARDUINO_ARCH_ESP8266

#include "FileMgr.hpp"

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
        if (!LITTLEFS.begin ())
        {
            LOG_PORT.println (F ("*** Flash File system did not initialize correctly ***"));
        }
        else
        {
            LOG_PORT.println (F ("Flash File system initialized."));
            listDir (LITTLEFS, String ("/"), 3);
        }

#ifdef ARDUINO_ARCH_ESP32
        SPI.begin (clk_pin, miso_pin, mosi_pin, cs_pin);

        if (!SD.begin (cs_pin))
#else
        SDFSConfig cfg (SD_CARD_CS_PIN, SD_CARD_CLK_MHZ);
        SDFS.setConfig (cfg);

        if (!SDFS.begin ())
#endif
        {
            LOG_PORT.println (String (F ("No SD card installed")));
            break;
        }

        SdCardInstalled = true;

        DescribeSdCardToUser ();

    } while (false);

    // DEBUG_END;
} // begin

//-----------------------------------------------------------------------------
void c_FileMgr::SetSpiIoPins (uint8_t miso, uint8_t mosi, uint8_t clock, uint8_t cs)
{
    miso_pin = miso;
    mosi_pin = mosi;
    clk_pin  = clock;
    cs_pin   = cs;


#ifdef ARDUINO_ARCH_ESP32
    SPI.end ();
    SPI.begin (clk_pin, miso_pin, mosi_pin, cs_pin);

    SD.end ();
#else
    SDFS.end ();

    SDFSConfig sdcfg;
    SPISettings spicfg;

    SDFSConfig cfg (cs_pin, SD_SCK_MHZ (80));
    SDFS.setConfig (cfg);
#endif

    if (!SDFS.begin ())
    {
        LOG_PORT.println (String (F ("File Manager: No SD card")));
    }

} // SetSpiIoPins

//-----------------------------------------------------------------------------
void c_FileMgr::DeleteConfigFile (String& FileName)
{
    // DEBUG_START;

    LITTLEFS.remove (FileName);

    // DEBUG_END;

} // DeleteConfigFile

//-----------------------------------------------------------------------------
void c_FileMgr::listDir (fs::FS& fs, String dirname, uint8_t levels)
{
    do // once
    {
        LOG_PORT.println (String (F ("Listing directory: ")) + dirname);

        File root = fs.open (dirname, "r");
        if (!root)
        {
            LOG_PORT.println (String (F ("failed to open directory: ")) + dirname);
            break;
        }

        if (!root.isDirectory ())
        {
            LOG_PORT.println (String (F ("Is not a directory: ")) + dirname);
            break;
        }

        File MyFile = root.openNextFile ();

        while (MyFile)
        {
            if (MyFile.isDirectory ())
            {
                if (levels)
                {
                    listDir (fs, MyFile.name (), levels - 1);
                }
            }
            else
            {
                LOG_PORT.println ("'" + String (MyFile.name ()) + "': \t'" + String (MyFile.size ()) + "'");
            }
            MyFile = root.openNextFile ();
        }

    } while (false);

} // listDir

//-----------------------------------------------------------------------------
bool c_FileMgr::LoadConfigFile (String& FileName, DeserializationHandler Handler)
{
    return LoadConfigFile (FileName, Handler, CONFIG_MAX_SIZE);

} // LoadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::LoadConfigFile (String& FileName, DeserializationHandler Handler, size_t JsonDocSize)
{
    // DEBUG_START;
    boolean retval = false;

    do // once
    {
        String CfgFileMessagePrefix = String (F ("Configuration file: '")) + FileName + "' ";

        // DEBUG_V ("allocate the JSON Doc");
        DynamicJsonDocument jsonDoc (JsonDocSize);

        if (false == ReadConfigFile (FileName, jsonDoc))
        {
            LOG_PORT.println (CfgFileMessagePrefix + String (F ("Could not read file.")));
            break;
        }

        // DEBUG_V ("");
        jsonDoc.garbageCollect ();

        LOG_PORT.println (CfgFileMessagePrefix + String (F ("loaded.")));

        Handler (jsonDoc);
        retval = true;

    } while (false);

    // DEBUG_END;
    return retval;

} // LoadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveConfigFile (String& FileName, String& FileData)
{
    // DEBUG_START;

    bool Response = false;
    String CfgFileMessagePrefix = String (F ("Configuration file: '")) + FileName + "' ";
    // DEBUG_V (FileData);

    fs::File file = LITTLEFS.open (FileName.c_str (), "w");
    if (!file)
    {
        LOG_PORT.println (CfgFileMessagePrefix + String (F ("Could not open file for writing..")));
    }
    else
    {
        file.seek (0, SeekSet);
        file.print (FileData);
        LOG_PORT.println (CfgFileMessagePrefix + String (F ("saved.")));
        file.close ();
        Response = true;
    }

    // DEBUG_END;
    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::SaveConfigFile (String& FileName, JsonVariant& FileData)
{
    // DEBUG_START;
    bool Response = false;
    String ConvertedFileData;

    serializeJson (FileData, ConvertedFileData);
    Response = SaveConfigFile (FileName, ConvertedFileData);

    // DEBUG_END;

    return Response;
} // SaveConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadConfigFile (String& FileName, String& FileData)
{
    // DEBUG_START;

    bool GotFileData = false;

    // DEBUG_V (String("File '") + FileName + "' is being opened.");
    fs::File file = LITTLEFS.open (FileName.c_str (), "r");
    if (file)
    {
        // DEBUG_V (String("File '") + FileName + "' is open.");
        file.seek (0, SeekSet);
        FileData = file.readString ();
        file.close ();
        GotFileData = true;

        // DEBUG_V (FileData);
    }
    else
    {
        LOG_PORT.println (String (F ("Configuration file: '")) + FileName + String (F ("' not found.")));
    }

    // DEBUG_END;
    return GotFileData;
} // ReadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadConfigFile (String& FileName, JsonDocument& FileData)
{
    // DEBUG_START;
    bool GotFileData = false;

    do // once
    {
        String RawFileData;
        if (false == ReadConfigFile (FileName, RawFileData))
        {
            // DEBUG_V ("Failed to read file");
            break;
        }

        // did we actually get any data
        if (0 == RawFileData.length ())
        {
            // DEBUG_V ("File is empty");
            // nope, no data
            false;
        }

        // DEBUG_V ("Convert File to JSON document");
        DeserializationError error = deserializeJson (FileData, (const String)RawFileData);

        // DEBUG_V ("Error Check");
        if (error)
        {
            String CfgFileMessagePrefix = String (F ("Configuration file: '")) + FileName + "' ";
            LOG_PORT.println (String (F ("Heap:")) + String (ESP.getFreeHeap ()));
            LOG_PORT.println (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            LOG_PORT.println (String (F ("++++")) + RawFileData + String (F ("----")));
            break;
        }

        // DEBUG_V ("Got config data");
        GotFileData = true;

    } while (false);

    // DEBUG_END;
    return GotFileData;

} // ReadConfigFile

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void c_FileMgr::DeleteSdFile (String& FileName)
{
    SDFS.remove (FileName);
} // DeleteSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::DescribeSdCardToUser ()
{
    // DEBUG_START;

    String BaseMessage = F ("*** Found SD Card ***");

#ifdef ARDUINO_ARCH_ESP32
    uint64_t cardSize = SD.cardSize () / (1024 * 1024);
    LOG_PORT.println (String (F ("SD Card Size: ")) + int64String (cardSize) + "MB");

    File root = SD.open ("/");
#else
    FSInfo64 fsinfo;
    SDFS.info64 (fsinfo);

    LOG_PORT.printf_P (PSTR ("SD Card Size: %lluMB (%lluMB used)\n"),
        (uint64_t)(fsinfo.totalBytes / (1024 * 1024)),
        (uint64_t)(fsinfo.usedBytes  / (1024 * 1024)));

    Dir root = SDFS.openDir ("/");
#endif // def ARDUINO_ARCH_ESP32

    printDirectory (root, 0);

    // DEBUG_END;

} // DescribeSdCardToUser

//-----------------------------------------------------------------------------
void c_FileMgr::GetListOfSdFiles (String & Response)
{
    // DEBUG_START;

    DynamicJsonDocument ResponseJsonDoc (2048);

    do // once
    {
        if (0 == ResponseJsonDoc.capacity ())
        {
            LOG_PORT.println (F ("ERROR: Failed to allocate memory for the GetListOfSdFiles web request response."));
            break;
        }

        JsonArray FileArray = ResponseJsonDoc.createNestedArray (F ("files"));
        ResponseJsonDoc["SdCardPresent"] = SdCardIsInstalled ();
        if (false == SdCardIsInstalled ())
        {
            break;
        }

        File dir = SDFS.open ("/", "r");

        while (true)
        {
            File entry = dir.openNextFile ();

            if (!entry)
            {
                // no more files
                break;
            }

            String EntryName = String (entry.name ());
            EntryName = EntryName.substring ((('/' == EntryName[0]) ? 1 : 0));
            // DEBUG_V ("EntryName: " + EntryName);
            // DEBUG_V ("EntryName.length(): " + String(EntryName.length ()));

            if ((0 != EntryName.length ()) && (EntryName != String (F ("System Volume Information"))))
            {
                // DEBUG_V ("Adding File: '" + EntryName + "'");

                JsonObject CurrentFile = FileArray.createNestedObject ();
                CurrentFile[F ("name")] = EntryName;
            }

            entry.close ();
        }

    } while (false);

    serializeJson (ResponseJsonDoc, Response);
    // DEBUG_V (Response);

    // DEBUG_END;

} // GetListOfFiles

//-----------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP32
void c_FileMgr::printDirectory (File dir, int numTabs)
{
    while (true)
    {
        File entry = dir.openNextFile ();

        if (!entry)
        {
            // no more files
            break;
        }
        for (uint8_t i = 0; i < numTabs; i++)
        {
            LOG_PORT.print ('\t');
        }
        Serial.print (entry.name ());
        if (entry.isDirectory ())
        {
            LOG_PORT.println ("/");
            printDirectory (entry, numTabs + 1);
        }
        else
        {
            // files have sizes, directories do not
            LOG_PORT.print ("\t\t");
            LOG_PORT.println (entry.size (), DEC);
        }
        entry.close ();
    }
#else
void c_FileMgr::printDirectory (Dir dir, int numTabs)
{
    while (dir.next ())
    {
        LOG_PORT.printf_P (PSTR ("%s - %u\n"), dir.fileName ().c_str (), dir.fileSize ());
    }
#endif // def ARDUINO_ARCH_ESP32
} // printDirectory

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (String& FileName, String FileData)
{
    do // once
    {
        FileId FileHandle = 0;
        if (false == OpenSdFile (FileName, FileMode::FileWrite, FileHandle))
        {
            LOG_PORT.println (String (F ("File Manager: Could not open '")) + FileName + F("' for writting."));
            break;
        }

        int WriteCount = WriteSdFile (FileHandle, (byte*)FileData.c_str (), (size_t)FileData.length ());
        LOG_PORT.println (String (F ("File Manager: Wrote '")) + FileName + F ("' ") + String(WriteCount));

        CloseSdFile (FileHandle);

    } while (false);

} // SaveSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (String& FileName, JsonVariant& FileData)
{
    String Temp;
    serializeJson (FileData, Temp);
    SaveSdFile (FileName, Temp);

} // SaveSdFile

//-----------------------------------------------------------------------------
bool c_FileMgr::OpenSdFile (String & FileName, FileMode Mode, FileId & FileHandle)
{
    bool FileIsOpen = false;
    FileHandle = 0;
    char ReadWrite[2] = { XlateFileMode[Mode], 0 };

    do // once
    {
        if (!SdCardInstalled)
        {
            // no SD card is installed
            break;
        }

        if (!FileName.startsWith ("/"))
        {
            FileName = "/" + FileName;
        }

        if (FileMode::FileRead == Mode)
        {
            if (false == SDFS.exists (FileName))
            {
                LOG_PORT.println (String (F ("ERROR: Cannot open '")) + FileName + F("' for reading. File does not exist."));
                break;
            }
        }

        FileHandle = millis ();
        FileList[FileHandle] = SDFS.open (FileName, ReadWrite);

        if (FileMode::FileWrite == Mode)
        {
            FileList[FileHandle].seek (0, SeekSet);
        }

        FileIsOpen = true;

    } while (false);

    return FileIsOpen;

} // OpenSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::ReadSdFile (FileId& FileHandle, byte* FileData, size_t NumBytesToRead, size_t StartingPosition)
{
    FileList[FileHandle].seek (StartingPosition, SeekSet);
    return ReadSdFile (FileHandle, FileData, NumBytesToRead);

} // ReadSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::ReadSdFile (FileId& FileHandle, byte* FileData, size_t NumBytesToRead)
{
    return FileList[FileHandle].read (FileData, NumBytesToRead);
} // ReadSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::CloseSdFile (FileId& FileHandle)
{
    FileList[FileHandle].close ();
    FileList.erase(FileHandle);

} // CloseSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::WriteSdFile (FileId& FileHandle, byte* FileData, size_t NumBytesToWrite)
{
    return FileList[FileHandle].write (FileData, NumBytesToWrite);
} // WriteSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::WriteSdFile (FileId& FileHandle, byte* FileData, size_t NumBytesToWrite, size_t StartingPosition)
{
    FileList[FileHandle].seek(StartingPosition, SeekSet);
    return WriteSdFile (FileHandle, FileData, NumBytesToWrite);

} // WriteSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::GetSdFileSize (FileId& FileHandle)
{
    return FileList[FileHandle].size ();

} // GetSdFileSize

//-----------------------------------------------------------------------------
void c_FileMgr::handleFileUpload (String filename,
    size_t index,
    uint8_t* data,
    size_t len,
    bool final)
{
    // DEBUG_START;
    if (0 == index)
    {
        handleFileUploadNewFile (filename);
    }

    if ((0 != len) && (0 != fsUploadFileName.length ()))
    {
        // Write data
        // DEBUG_V ("UploadWrite: " + String (len) + String (" bytes"));
        FileMgr.WriteSdFile (fsUploadFile, data, len);
        // LOG_PORT.print (String ("Writting bytes: ") + String (index) + '\r');
        LOG_PORT.print (".");
    }

    if ((true == final) && (0 != fsUploadFileName.length ()))
    {
        LOG_PORT.println ("");
        // DEBUG_V ("UploadEnd: " + String(index + len) + String(" bytes"));
        LOG_PORT.println (String (F ("Upload File: '")) + fsUploadFileName + String (F ("' Done")));

        FileMgr.CloseSdFile (fsUploadFile);
        fsUploadFileName = "";
    }

    // DEBUG_END;
} // handleFileUpload

//-----------------------------------------------------------------------------
void c_FileMgr::handleFileUploadNewFile (String & filename)
{
    // DEBUG_START;

    // save the filename
    // DEBUG_V ("UploadStart: " + filename);

    // are we terminating the previous download?
    if (0 != fsUploadFileName.length ())
    {
        LOG_PORT.println (String (F ("Aborting Previous File Upload For: '")) + fsUploadFileName + String (F ("'")));
        FileMgr.CloseSdFile (fsUploadFile);
        fsUploadFileName = "";
    }

    // Set up to receive a file
    fsUploadFileName = filename;

    LOG_PORT.println (String (F ("Upload File: '")) + fsUploadFileName + String (F ("' Started")));

    FileMgr.DeleteSdFile (fsUploadFileName);

    // Open the file for writing
    FileMgr.OpenSdFile (fsUploadFileName, FileMode::FileWrite, fsUploadFile);

    // DEBUG_END;

} // handleFileUploadNewFile

//-----------------------------------------------------------------------------
void c_FileMgr::Poll ()
{
    // DEBUG_START;


    // DEBUG_END;

} // Poll

// create a global instance of the File Manager
c_FileMgr FileMgr;
