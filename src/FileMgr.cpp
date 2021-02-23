/*
* FileMgr.cpp - Output Management class
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
            LOG_PORT.println ( String(CN_stars) + F (" Flash File system did not initialize correctly ") + CN_stars);
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

    SDFS.end ();

#ifdef ARDUINO_ARCH_ESP32
    SPI.end ();
    SPI.begin (clk_pin, miso_pin, mosi_pin, cs_pin);
#else
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
void c_FileMgr::DeleteConfigFile (const String& FileName)
{
    // DEBUG_START;

    LITTLEFS.remove (FileName);

    // DEBUG_END;

} // DeleteConfigFile

//-----------------------------------------------------------------------------
void c_FileMgr::listDir (fs::FS& fs, String dirname, uint8_t levels)
{
    // DEBUG_START;
    do // once
    {
        LOG_PORT.println (String (F ("Listing directory: ")) + dirname);

        File root = fs.open (dirname, CN_r);
        if (!root)
        {
            LOG_PORT.println (String (CN_stars) + F ("failed to open directory: ") + dirname + CN_stars);
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
bool c_FileMgr::LoadConfigFile (const String& FileName, DeserializationHandler Handler)
{
    // DEBUG_START;
    boolean retval = false;

    do // once
    {
        String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";

        // DEBUG_V ("allocate the JSON Doc");

        String RawFileData;
        if (false == ReadConfigFile (FileName, RawFileData))
        {
            LOG_PORT.println (String(CN_stars) + CfgFileMessagePrefix + F ("Could not read file.") + CN_stars);
            break;
        }

        // DEBUG_V ("Convert File to JSON document");
        size_t JsonDocSize = RawFileData.length () * 3;
        // DEBUG_V (String ("RawFileData.length: ") + String (RawFileData.length ()));
        // DEBUG_V (String (" Final JsonDocSize: ") + String (JsonDocSize));

        DynamicJsonDocument jsonDoc (JsonDocSize);
        DeserializationError error = deserializeJson (jsonDoc, (const String)RawFileData);

        // DEBUG_V ("Error Check");
        if (error)
        {
            String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
            LOG_PORT.println (CN_Heap_colon + String (ESP.getFreeHeap ()));
            LOG_PORT.println (String(CN_stars) + CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str () + CN_stars);
            LOG_PORT.println (CN_plussigns + RawFileData + CN_minussigns);
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
bool c_FileMgr::SaveConfigFile (const String& FileName, String& FileData)
{
    // DEBUG_START;

    bool Response = false;
    String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
    // DEBUG_V (FileData);

    fs::File file = LITTLEFS.open (FileName.c_str (), "w");
    if (!file)
    {
        LOG_PORT.println (String(CN_stars) + CfgFileMessagePrefix + String (F ("Could not open file for writing..")) + CN_stars);
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
bool c_FileMgr::SaveConfigFile (const String& FileName, JsonVariant& FileData)
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
bool c_FileMgr::ReadConfigFile (const String& FileName, String& FileData)
{
    // DEBUG_START;

    bool GotFileData = false;

    // DEBUG_V (String("File '") + FileName + "' is being opened.");
    fs::File file = LITTLEFS.open (FileName.c_str (), CN_r);
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
        LOG_PORT.println (String (CN_stars) + CN_Configuration_File_colon + "'" + FileName + F ("' not found.") + CN_stars);
    }

    // DEBUG_END;
    return GotFileData;
} // ReadConfigFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadConfigFile (const String& FileName, JsonDocument & FileData)
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
            String CfgFileMessagePrefix = String (CN_Configuration_File_colon) + "'" + FileName + "' ";
            LOG_PORT.println (CN_Heap_colon + String (ESP.getFreeHeap ()));
            LOG_PORT.println (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
            LOG_PORT.println (CN_plussigns + RawFileData + CN_minussigns);
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
//-----------------------------------------------------------------------------
c_FileMgr::FileId c_FileMgr::CreateFileHandle ()
{
    // DEBUG_START;

    FileId FileHandle = millis ();

    if (FileList.end () != FileList.find (FileHandle))
    {
        ++FileHandle;
    }

    // DEBUG_END;

    return FileHandle;
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

    if (SDFS.exists (FileNamePrefix+FileName))
    {
        // DEBUG_V (String ("Deleting '") + FileName + "'");
        SDFS.remove (FileNamePrefix+FileName);
    }

    // DEBUG_END;

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
            LOG_PORT.println (String(CN_stars) + F ("ERROR: Failed to allocate memory for the GetListOfSdFiles web request response.") + CN_stars);
            break;
        }

        JsonArray FileArray = ResponseJsonDoc.createNestedArray (CN_files);
        ResponseJsonDoc[F ("SdCardPresent")] = SdCardIsInstalled ();
        if (false == SdCardIsInstalled ())
        {
            break;
        }

        File dir = SDFS.open ("/", CN_r);

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

            if ((0 != EntryName.length ()) && 
                (EntryName != String (F ("System Volume Information"))) &&
                (0 != entry.size ())
               )
            {
                // DEBUG_V ("Adding File: '" + EntryName + "'");

                JsonObject CurrentFile = FileArray.createNestedObject ();
                CurrentFile[CN_name]      = EntryName;
                CurrentFile[F ("date")]   = entry.getLastWrite ();
                CurrentFile[F ("length")] = entry.size ();
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
void c_FileMgr::SaveSdFile (const String & FileName, String & FileData)
{
    do // once
    {
        FileId FileHandle = 0;
        if (false == OpenSdFile (FileName, FileMode::FileWrite, FileHandle))
        {
            LOG_PORT.println (String (F ("File Manager: Could not open '")) + FileName + F ("' for writting."));
            break;
        }

        int WriteCount = WriteSdFile (FileHandle, (byte*)FileData.c_str (), (size_t)FileData.length ());
        LOG_PORT.println (String (F ("File Manager: Wrote '")) + FileName + F ("' ") + String(WriteCount));

        CloseSdFile (FileHandle);

    } while (false);

} // SaveSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (const String & FileName, JsonVariant & FileData)
{
    String Temp;
    serializeJson (FileData, Temp);
    SaveSdFile (FileName, Temp);

} // SaveSdFile

//-----------------------------------------------------------------------------
bool c_FileMgr::OpenSdFile (const String & FileName, FileMode Mode, FileId & FileHandle)
{
    // DEBUG_START;

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
            if (false == SDFS.exists (FileNamePrefix + FileName))
            {
                LOG_PORT.println (String (F ("ERROR: Cannot open '")) + FileName + F ("' for reading. File does not exist."));
                break;
            }
        }

        // DEBUG_V ();
        FileHandle = CreateFileHandle ();
        // DEBUG_V (String("FileHandle: ") + String(FileHandle));

        FileList[FileHandle] = SDFS.open (FileNamePrefix + FileName, ReadWrite);

        // DEBUG_V ();
        if (FileMode::FileWrite == Mode)
        {
            // DEBUG_V ("Write Mode");
            FileList[FileHandle].seek (0, SeekSet);
        }
        // DEBUG_V ();

        FileIsOpen = true;

    } while (false);

    // DEBUG_END;
    return FileIsOpen;

} // OpenSdFile

//-----------------------------------------------------------------------------
bool c_FileMgr::ReadSdFile (const String & FileName, String & FileData)
{
    // DEBUG_START;

    bool GotFileData = false;
    FileId FileHandle = 0;

    // DEBUG_V (String("File '") + FileName + "' is being opened.");
    if (true == OpenSdFile (FileName, FileMode::FileRead, FileHandle))
    {
        // DEBUG_V (String("File '") + FileName + "' is open.");
        FileList[FileHandle].seek (0, SeekSet);
        FileData = FileList[FileHandle].readString ();

        CloseSdFile (FileHandle);
        GotFileData = (0 != FileData.length());

        // DEBUG_V (FileData);
    }
    else
    {
        CloseSdFile (FileHandle);
        LOG_PORT.println (String (F ("SD file: '")) + FileName + String (F ("' not found.")));
    }

    // DEBUG_END;
    return GotFileData;

} // ReadSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::ReadSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToRead, size_t StartingPosition)
{
    // DEBUG_START;

    size_t response = 0;

    // DEBUG_V (String ("      FileHandle: ") + String (FileHandle));
    // DEBUG_V (String ("  NumBytesToRead: ") + String (NumBytesToRead));
    // DEBUG_V (String ("StartingPosition: ") + String (StartingPosition));

    if (FileList.end () != FileList.find (FileHandle))
    {
        FileList[FileHandle].seek (StartingPosition, SeekSet);
        response = ReadSdFile (FileHandle, FileData, NumBytesToRead);
    }
    else
    {
        LOG_PORT.println ("FileMgr::ReadSdFile::ERROR::Invalid File Handle.");
    }

    // DEBUG_END;
    return response;

} // ReadSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::ReadSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToRead)
{
    // DEBUG_START;

    // DEBUG_V (String ("    FileHandle: ") + String (FileHandle));
    // DEBUG_V (String ("NumBytesToRead: ") + String (NumBytesToRead));

    size_t response = 0;
    if (FileList.end() != FileList.find (FileHandle))
    {
        response = FileList[FileHandle].read (FileData, NumBytesToRead);
    }
    else
    {
        LOG_PORT.println ("FileMgr::ReadSdFile::ERROR::Invalid File Handle.");
    }

    // DEBUG_END;
    return response;

} // ReadSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::CloseSdFile (const FileId& FileHandle)
{
    // DEBUG_START;

    if (FileList.end () != FileList.find (FileHandle))
    {
        FileList[FileHandle].close ();
        FileList.erase (FileHandle);
    }
    else
    {
        // LOG_PORT.println ("FileMgr::CloseSdFile::ERROR::Invalid File Handle.");
    }

    // DEBUG_END;

} // CloseSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::WriteSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToWrite)
{
    size_t response = 0;
    if (FileList.end () != FileList.find (FileHandle))
    {
        response = FileList[FileHandle].write (FileData, NumBytesToWrite);
    }

    return response;

} // WriteSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::WriteSdFile (const FileId& FileHandle, byte* FileData, size_t NumBytesToWrite, size_t StartingPosition)
{
    size_t response = 0;
    if (FileList.end () != FileList.find (FileHandle))
    {
        FileList[FileHandle].seek (StartingPosition, SeekSet);
        response = WriteSdFile (FileHandle, FileData, NumBytesToWrite);
    }

    return response;

} // WriteSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::GetSdFileSize (const FileId& FileHandle)
{
    return FileList[FileHandle].size ();

} // GetSdFileSize

//-----------------------------------------------------------------------------
void c_FileMgr::handleFileUpload (const String & filename,
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
        WriteSdFile (fsUploadFile, data, len);
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
void c_FileMgr::handleFileUploadNewFile (const String & filename)
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
