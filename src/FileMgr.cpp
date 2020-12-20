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

#ifdef ARDUINO_ARCH_ESP8266
#else
#endif // def ARDUINO_ARCH_ESP8266

#include "FileMgr.hpp"

#include <FS.h>

#ifdef ARDUINO_ARCH_ESP32
#   include <Update.h>
#else
#   define LITTLEFS LittleFS
#endif

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
            LOG_PORT.println (F ("*** File system did not initialize correctly ***"));
            break;
        }

        LOG_PORT.println (F ("File system initialized."));

        listDir (LITTLEFS, String ("/"), 3);

    } while (false);

    // DEBUG_END;
} // begin

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

    fs::File file = LITTLEFS.open (FileName.c_str (), "w");
    if (!file)
    {
        LOG_PORT.println (CfgFileMessagePrefix + String (F ("Could not open file for writing..")));
    }
    else
    {
        file.print (FileData);
        LOG_PORT.println (CfgFileMessagePrefix + String (F ("saved.")));
        file.close ();
        Response == true;
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

    fs::File file = LITTLEFS.open (FileName.c_str (), "r");
    if (file)
    {
        // DEBUG_V ("File is open. Read it");
        file.seek (0);
        FileData = file.readString ();
        file.close ();
        GotFileData = true;
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
void c_FileMgr::DeleteSdFile (String& FileName)
{

} // DeleteSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (String& FileName, String FileData)
{

} // SaveSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::SaveSdFile (String& FileName, JsonVariant& FileData)
{

} // SaveSdFile

//-----------------------------------------------------------------------------
bool c_FileMgr::OpenSdFile (String& FileName, FileId& FileHandle)
{

} // OpenSdFile

//-----------------------------------------------------------------------------
size_t c_FileMgr::ReadSdFile (FileId& FileHandle, byte* FileData, size_t maxBytes)
{

} // ReadSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::CloseSdFile (FileId& FileHandle)
{

} // CloseSdFile

//-----------------------------------------------------------------------------
void c_FileMgr::Poll ()
{
    // DEBUG_START;


    // DEBUG_END;

} // Poll


// create a global instance of the File Manager
c_FileMgr FileMgr;
