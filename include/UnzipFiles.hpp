#pragma once
/*
* UnzipFiles.hpp
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

#include "ESPixelStick.h"
#include <unzipLIB.h>

class UnzipFiles
{
public:
    UnzipFiles ();
    virtual ~UnzipFiles ();

    void    Run();

    void *  OpenZipFile(const char *filename, int32_t *size);
    void    CloseZipFile(void *p);
    int32_t ReadZipFile(void *p, uint8_t *buffer, int32_t length);
    int32_t SeekZipFile(void *p, int32_t position, int iType);
    void    GetDriverName(String & value) {value = "Unzip";}
    void    ProcessZipFile(String & FileName);
    void    ProcessCurrentFileInZip(unz_file_info & fi, String & Name);

private:
    UNZIP       zip; // statically allocate the UNZIP structure (41K)
    uint8_t     *pOutputBuffer = nullptr;
    uint32_t    BufferSize = 0;
    int32_t     SeekPosition = 0;

protected:

}; // UnzipFiles

extern UnzipFiles gUnzipFiles;

#endif // def SUPPORT_UNZIP
