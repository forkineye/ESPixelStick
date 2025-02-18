#pragma once
/*
* FileMgr.hpp - Output Management class
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
#include <LittleFS.h>
#ifdef SUPPORT_SD_MMC
#   include <SD_MMC.h>
#endif // def SUPPORT_SD_MMC
#include "SdFat.h"
#include <map>
#include <vector>

#ifdef ARDUINO_ARCH_ESP32
#   ifdef SUPPORT_SD_MMC
#       define ESP_SD   SD_MMC
#	    define ESP_SDFS SD_MMC
#   else // !def SUPPORT_SD_MMC
extern SdFat sd;
#       define ESP_SD   sd
#	    define ESP_SDFS SdFile
#   endif // !def SUPPORT_SD_MMC
#else // !ARDUINO_ARCH_ESP32
#   define ESP_SD   sd
#   define ESP_SDFS SdFile
#endif // !ARDUINO_ARCH_ESP32

class c_FileMgr
{
public:
    c_FileMgr ();
    virtual ~c_FileMgr ();

    typedef uint32_t FileId;
    const static FileId INVALID_FILE_HANDLE = 0;

    void    Begin     ();
    void    Poll      ();
    void    GetConfig (JsonObject& json);
    bool    SetConfig (JsonObject& json);
    void    GetStatus (JsonObject& json);

    bool    handleFileUpload (const String & filename, size_t index, uint8_t * data, size_t len, bool final, uint32_t totalLen);
    void    AbortSdFileUpload();

    typedef std::function<void (JsonDocument& json)> DeserializationHandler;

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

    bool     SdCardIsInstalled () { return SdCardInstalled; }
    FileId   CreateSdFileHandle ();
    void     DeleteSdFile     (const String & FileName);
    void     SaveSdFile       (const String & FileName, String & FileData);
    void     SaveSdFile       (const String & FileName, JsonVariant & FileData);
    bool     OpenSdFile       (const String & FileName, FileMode Mode, FileId & FileHandle, int FileListIndex);
    uint64_t ReadSdFile     (const FileId & FileHandle, byte * FileData, uint64_t NumBytesToRead, uint64_t StartingPosition);
    bool     ReadSdFile       (const String & FileName,   String & FileData);
    bool     ReadSdFile       (const String & FileName,   JsonDocument & FileData);
    uint64_t WriteSdFileBuf   (const FileId & FileHandle, byte * FileData, uint64_t NumBytesToWrite);
    uint64_t WriteSdFile      (const FileId & FileHandle, byte * FileData, uint64_t NumBytesToWrite);
    uint64_t WriteSdFile      (const FileId & FileHandle, byte * FileData, uint64_t NumBytesToWrite, uint64_t StartingPosition);
    void     CloseSdFile      (FileId & FileHandle);
    void     GetListOfSdFiles (std::vector<String> & Response);
    uint64_t GetSdFileSize  (const String & FileName);
    uint64_t GetSdFileSize  (const FileId & FileHandle);
    void     RenameSdFile     (String & OldName, String & NewName);
    void     BuildFseqList    (bool DisplayFileNames);

    void     GetDriverName    (String& Name) { Name = "FileMgr"; }
    void     NetworkStateChanged (bool NewState);
    uint64_t GetDefaultFseqFileList (uint8_t * buffer, uint64_t maxlen);
    void     FindFirstZipFile (String &FileName);

#define FSEQFILELIST "fseqfilelist.json"
#define SD_BLOCK_SIZE 512

#if defined ARDUINO_ARCH_ESP8266
#   define MAX_SD_BUFFER_SIZE (4 * SD_BLOCK_SIZE)
#else
#   define MAX_SD_BUFFER_SIZE (14 * SD_BLOCK_SIZE)
#endif

private:
    void    SetSpiIoPins ();
    void    SetSdSpeed ();
    void    ResetSdCard ();
    void    LockSd();
    void    UnLockSd();
    bool    SeekSdFile(const FileId & FileHandle, uint64_t position, SeekMode Mode);

#   define SD_CARD_CLK_MHZ     SD_SCK_MHZ(37)  // 50 MHz SPI clock
#ifndef MaxSdTransSpeedMHz
#   define MaxSdTransSpeedMHz 200
#endif // ndef MaxSdTransSpeedMHz

    void listDir (fs::FS& fs, String dirname, uint8_t levels);
    void DescribeSdCardToUser ();
    void handleFileUploadNewFile (const String & filename);
    void printDirectory (FsFile & dir, int numTabs);

    bool     SdCardInstalled = false;
    uint8_t  miso_pin = SD_CARD_MISO_PIN;
    uint8_t  mosi_pin = SD_CARD_MOSI_PIN;
    uint8_t  clk_pin  = SD_CARD_CLK_PIN;
    uint8_t  cs_pin   = SD_CARD_CS_PIN;
    FileId   fsUploadFileHandle;
    String   fsUploadFileName;
    bool     fsUploadFileSavedIsEnabled = false;
    uint32_t fsUploadStartTime;
    String   FtpUserName = "esps";
    String   FtpPassword = "esps";
    String   WelcomeString = "ESPS V4 FTP";
    bool     FtpEnabled = true;
    uint64_t SdCardSizeMB = 0;
    uint32_t MaxSdSpeed = MaxSdTransSpeedMHz;
    bool     FoundZipFile = false;

public: struct __attribute__((__packed__, aligned(4))) CSD {
	public: union {
		public: struct __attribute__((__packed__, aligned(1))) {
			public: enum {
				CSD_VERSION_1 = 0,						// enum CSD version 1.0 - 1.1, Version 2.00/Standard Capacity
				CSD_VERSION_2 = 1,						// enum CSD cersion 2.0, Version 2.00/High Capacity and Extended Capacity
			} csd_structure : 2;						// @127-126	CSD Structure Version as on SD CSD bits 
			unsigned spec_vers : 6;						// @125-120 CSD version as on SD CSD bits
			uint8_t taac;								// @119-112	taac as on SD CSD bits
			uint8_t nsac;								// @111-104	nsac as on SD CSD bits
			uint8_t tran_speed;							// @103-96	trans_speed as on SD CSD bits
		}Decode_0;
		public: uint32_t Raw32_0;								// @127-96	Union to access 32 bits as a uint32_t		
	};
	public: union {
		public: struct __attribute__((__packed__, aligned(1))) {
			unsigned ccc : 12;							// @95-84	ccc as on SD CSD bits
			unsigned read_bl_len : 4;					// @83-80	read_bl_len on SD CSD bits
			unsigned read_bl_partial : 1;				// @79		read_bl_partial as on SD CSD bits
			unsigned write_blk_misalign : 1;			// @78		write_blk_misalign as on SD CSD bits
			unsigned read_blk_misalign : 1;				// @77		read_blk_misalign as on SD CSD bits
			unsigned dsr_imp : 1;						// @76		dsr_imp as on SD CSD bits
			unsigned c_size : 12;						// @75-64   Version 1 C_Size as on SD CSD bits
		};
		public: uint32_t Raw32_1;								// @0-31	Union to access 32 bits as a uint32_t		
	};
	public: union {
		public: struct __attribute__((__packed__, aligned(1))) {
			public: union {
				public: struct __attribute__((__packed__, aligned(1))) {
					unsigned vdd_r_curr_min : 3;		// @61-59	vdd_r_curr_min as on SD CSD bits
					unsigned vdd_r_curr_max : 3;		// @58-56	vdd_r_curr_max as on SD CSD bits
					unsigned vdd_w_curr_min : 3;		// @55-53	vdd_w_curr_min as on SD CSD bits
					unsigned vdd_w_curr_max : 3;		// @52-50	vdd_w_curr_max as on SD CSD bits
					unsigned c_size_mult : 3;			// @49-47	c_size_mult as on SD CSD bits
					unsigned reserved0 : 7;				// reserved for CSD ver 2.0 size match
				};
				unsigned ver2_c_size : 22;				// Version 2 C_Size
			};
			unsigned erase_blk_en : 1;					// @46		erase_blk_en as on SD CSD bits
			unsigned sector_size : 7;					// @45-39	sector_size as on SD CSD bits
			unsigned reserved1 : 2;						// 2 Spares bit unused
		};
		public: uint32_t Raw32_2;								// @0-31	Union to access 32 bits as a uint32_t		
	};
	public: union {
		public: struct __attribute__((__packed__, aligned(1))) {
			unsigned wp_grp_size : 7;					// @38-32	wp_grp_size as on SD CSD bits
			unsigned wp_grp_enable : 1;					// @31		wp_grp_enable as on SD CSD bits
			unsigned reserved2 : 2;						// @30-29 	Write as zero read as don't care
			unsigned r2w_factor : 3;					// @28-26	r2w_factor as on SD CSD bits
			unsigned write_bl_len : 4;					// @25-22	write_bl_len as on SD CSD bits
			unsigned write_bl_partial : 1;				// @21		write_bl_partial as on SD CSD bits
			unsigned default_ecc : 5;					// @20-16	default_ecc as on SD CSD bits
			unsigned file_format_grp : 1;				// @15		file_format_grp as on SD CSD bits
			unsigned copy : 1;							// @14		copy as on SD CSD bits
			unsigned perm_write_protect : 1;			// @13		perm_write_protect as on SD CSD bits
			unsigned tmp_write_protect : 1;				// @12		tmp_write_protect as on SD CSD bits
			public: enum {
				// FAT_PARTITION_TABLE = 0,				// enum SD card is FAT with partition table
				// FAT_NO_PARTITION_TABLE = 1,				// enum SD card is FAT with no partition table
				// FS_UNIVERSAL = 2,						// enum	SD card file system is universal
				// FS_OTHER = 3,							// enum	SD card file system is other
			} file_format : 2;							// @11-10	File format as on SD CSD bits
			unsigned ecc : 2;							// @9-8		ecc as on SD CSD bits
			unsigned reserved3 : 1;						// 1 spare bit unused
		};
		public: uint32_t Raw32_3;								// @0-31	Union to access 32 bits as a uint32_t		
	};
};
#define MaxOpenFiles 5
    struct FileListEntry_t
    {
        FileId      handle = INVALID_FILE_HANDLE;
        FsFile      fsFile;
        uint64_t    size = 0;
        int         entryId = -1;
        String      Filename = emptyString;
        FileMode    mode = FileMode::FileRead;
        bool        IsOpen = false;
        struct
        {
            byte     *DataBuffer = nullptr;
            uint64_t  size = 0;
            uint64_t  offset = 0;
        } buffer;
    };
#define DATABUFFERSIZE (5 * 1024)

    FileListEntry_t FileList[MaxOpenFiles];
    int FileListFindSdFileHandle (FileId HandleToFind);
    void InitSdFileList ();

    File        FileSendDir;
    uint32_t    LastFileSent = 0;
    uint32_t    expectedIndex = 0;

    bool SdAccessSemaphore = false;

protected:

}; // c_FileMgr

extern c_FileMgr FileMgr;
