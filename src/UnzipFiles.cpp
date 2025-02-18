/*
* UnzipFiles.cpp - Output Management class
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
#ifdef SUPPORT_UNZIP

#include "UnzipFiles.hpp"
#include "FileMgr.hpp"

//
// Callback functions needed by the unzipLIB to access a file system
// The library has built-in code for memory-to-memory transfers, but needs
// these callback functions to allow using other storage media
//
void * _OpenZipFile(const char *FileName, int32_t *size)
{
    return gUnzipFiles.OpenZipFile(FileName, size);
}

void _CloseZipFile(void *p)
{
    gUnzipFiles.CloseZipFile(p);
}

int32_t _ReadZipFile(void *p, uint8_t *buffer, int32_t length)
{
    return gUnzipFiles.ReadZipFile(p, buffer, length);
}

int32_t _SeekZipFile(void *p, int32_t position, int iType)
{
    return gUnzipFiles.SeekZipFile(p, position, iType);
} // _SeekZipFile

//-----------------------------------------------------------------------------
UnzipFiles::UnzipFiles ()
{
    // DEBUG_START;

    // use 75% and align to an even number of KB
    BufferSize = uint32_t(float(ESP.getMaxAllocHeap ()) * 0.75) & 0xfffffc00;
    // DEBUG_V(String("BufferSize: ") + String(BufferSize));

    pOutputBuffer = (uint8_t*)malloc(BufferSize);

    // DEBUG_END;

} // UnzipFiles

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
UnzipFiles::~UnzipFiles ()
{
    // DEBUG_START;

    if(pOutputBuffer)
    {
        // DEBUG_V("Release buffer");
        free(pOutputBuffer);
        pOutputBuffer = nullptr;
    }

    // DEBUG_END;

} // ~UnzipFiles

//-----------------------------------------------------------------------------
void UnzipFiles::Run()
{
    // DEBUG_START;

    String FileName = emptyString;

    do
    {
        FeedWDT();
        FileName = emptyString;
        FileMgr.FindFirstZipFile(FileName);
        if(FileName.isEmpty())
        {
            break;
        }

        ProcessZipFile(FileName);

        FileMgr.DeleteSdFile(FileName);

    } while(true);

    // DEBUG_END;
} // Run

//-----------------------------------------------------------------------------
void UnzipFiles::ProcessZipFile(String & FileName)
{
    // DEBUG_START;
    char szComment[256], szName[256];
    unz_file_info fi;

    logcon(String("Unzip file: '") + String(FileName) + "'");

    int returnCode = zip.openZIP(FileName.c_str(), _OpenZipFile, _CloseZipFile, _ReadZipFile, _SeekZipFile);
    if (returnCode == UNZ_OK)
    {
        bool IsSpecialxLightsZipFile = (-1 != FileName.indexOf(".xlz"));
        // logcon(String("Opened zip file: '") + FileName + "'");

        // Display the global comment and all of the FileNames within
        // returnCode = zip.getGlobalComment(szComment, sizeof(szComment));
        // logcon(String("Global comment: '") + String(szComment) + "'");
        logcon("Files in this archive:");
        returnCode = zip.gotoFirstFile();
        while (returnCode == UNZ_OK)
        {
            // Display all files contained in the archive
            returnCode = zip.getFileInfo(&fi, szName, sizeof(szName), NULL, 0, szComment, sizeof(szComment));
            if (returnCode == UNZ_OK)
            {
                FeedWDT();
                String SubFileName = String(szName);
                ProcessCurrentFileInZip(fi, SubFileName);
                if(IsSpecialxLightsZipFile)
                {
                    String NewName = FileName;
                    NewName.replace(".xlz", ".fseq");
                    // DEBUG_V(String("Rename '") + SubFileName + "' to '" + NewName);
                    FileMgr.RenameSdFile(SubFileName, NewName);
                    // only a single file is alowed in an xlz zip file.
                    break;
                }
            }
            returnCode = zip.gotoNextFile();
        } // while more files...
        zip.closeZIP();
        // DEBUG_V("No more files in the zip");
    }
    else
    {
        logcon("Could not open zip file.");
    }

    // DEBUG_END;
} // Run

//-----------------------------------------------------------------------------
void UnzipFiles::ProcessCurrentFileInZip(unz_file_info & fi, String & FileName)
{
    // DEBUG_START;
    // DEBUG_V(String("open Filename: ") + FileName);

    int BytesRead = 0;
    uint32_t TotalBytesWritten = 0;

    logcon(FileName +
    " - " + String(fi.compressed_size, DEC) +
    "/" + String(fi.uncompressed_size, DEC) + " Started.\n");

    do // once
    {
        int ReturnCode = zip.openCurrentFile();
        if(ReturnCode != UNZ_OK)
        {
            // DEBUG_V(String("ReturnCode: ") + String(ReturnCode));
            logcon(FileName + F(" Failed."));
            break;
        }

        c_FileMgr::FileId FileHandle;
        FileMgr.OpenSdFile(FileName, c_FileMgr::FileMode::FileWrite, FileHandle, -1);
        if(FileHandle == c_FileMgr::INVALID_FILE_HANDLE)
        {
            zip.closeCurrentFile();
            logcon(String("Could not open '") + FileName + "' for writting");
            break;
        }

        do
        {
            BytesRead = zip.readCurrentFile(pOutputBuffer, BufferSize);
            // DEBUG_V(String("BytesRead: ") + String(BytesRead));
            if(BytesRead != FileMgr.WriteSdFile(FileHandle, pOutputBuffer, BytesRead))
            {
                logcon(String(F("Failed to write data to '")) + FileName + "'");
                break;
            }
            TotalBytesWritten += BytesRead;
            LOG_PORT.println(String("\033[Fprogress: ") + String(TotalBytesWritten));
            LOG_PORT.flush();

        } while (BytesRead > 0);

        FileMgr.CloseSdFile(FileHandle);
        zip.closeCurrentFile();
        logcon(FileName + F(" - Done."));
    } while(false);

    // DEBUG_V(String("Close Filename: ") + FileName);
    // DEBUG_END;
} // ProcessCurrentFileInZip

//-----------------------------------------------------------------------------
void * UnzipFiles::OpenZipFile(const char *FileName, int32_t *size)
{
    // DEBUG_START;
    // DEBUG_V(String("  FileName: '") + String(FileName) + "'");

    c_FileMgr::FileId FileHandle = c_FileMgr::INVALID_FILE_HANDLE;
    FileMgr.OpenSdFile(FileName, c_FileMgr::FileMode::FileRead, FileHandle, -1);
    if(FileHandle == c_FileMgr::INVALID_FILE_HANDLE)
    {
        logcon(String("Could not open file for unzipping: '") + FileName + "'");
    }
    else
    {
        *size = int32_t(FileMgr.GetSdFileSize(FileHandle));
    }

    // DEBUG_V(String("FileHandle: ") + String(FileHandle));
    // DEBUG_V(String("      size: ") + String(*size));

    SeekPosition = 0;

    // DEBUG_END;
    return (void *)FileHandle;

} // OpenZipFile

//-----------------------------------------------------------------------------
void UnzipFiles::CloseZipFile(void *p)
{
    // DEBUG_START;

    // DEBUG_V(String("         p: 0x") + String(uint32_t(p), HEX));

    c_FileMgr::FileId FileHandle = (c_FileMgr::FileId)(((ZIPFILE *)p)->fHandle);
    // DEBUG_V(String("FileHandle: ") + String(FileHandle));

    FileMgr.CloseSdFile(FileHandle);
    SeekPosition = 0;

    // DEBUG_END;
} // CloseZipFile

//-----------------------------------------------------------------------------
int32_t UnzipFiles::ReadZipFile(void *p, uint8_t *buffer, int32_t length)
{
    // DEBUG_START;

    // DEBUG_V(String("         p: 0x") + String(uint32_t(p), HEX));

    c_FileMgr::FileId FileHandle = (c_FileMgr::FileId)(((ZIPFILE *)p)->fHandle);
    // DEBUG_V(String("FileHandle: ") + String(FileHandle));

    size_t BytesRead = FileMgr.ReadSdFile(FileHandle, buffer, length, SeekPosition);
    // DEBUG_V(String(" BytesRead: ") + String(BytesRead));

    SeekPosition += BytesRead;

    // DEBUG_END;
    return BytesRead;

} // ReadZipFile

//-----------------------------------------------------------------------------
int32_t UnzipFiles::SeekZipFile(void *p, int32_t position, int iType)
{
    // DEBUG_START;
    SeekPosition = position;
    // DEBUG_V(String("         p: 0x") + String(uint32_t(p), HEX));

    c_FileMgr::FileId FileHandle = (c_FileMgr::FileId)(((ZIPFILE *)p)->fHandle);
    // DEBUG_V(String("FileHandle: ") + String(FileHandle));
    // DEBUG_V(String("  Position: ") + String(position));
    // DEBUG_V(String("     iType: ") + String(iType));

    // DEBUG_END;
    return SeekPosition; // FileMgr.SeekSdFile(FileHandle, position, SeekMode(iType));
} // SeekZipFile

UnzipFiles gUnzipFiles;

#endif // def SUPPORT_UNZIP
