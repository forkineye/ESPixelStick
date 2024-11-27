#pragma once
/*
* fseq.h

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
//-----------------------------------------------------------------------------
typedef union
{
    struct
    {
        uint8_t  header[4];  //FPPD
        uint8_t  packet_type;
        uint16_t data_len;
    } __attribute__ ((packed));
    uint8_t raw[301];
} FPPPacket;

typedef union
{
    struct
    {
        uint8_t  header[4];  //FPPD
        uint8_t  packet_type;
        uint16_t data_len;
        uint8_t  ping_version;
        uint8_t  ping_subtype;
        uint8_t  ping_hardware;
        uint16_t versionMajor;
        uint16_t versionMinor;
        uint8_t  operatingMode;
        uint8_t  ipAddress[4];
        char  hostName[65];
        char  version[41];
        char  hardwareType[41];
        char  ranges[121];
    } __attribute__ ((packed));
    uint8_t raw[301];
} FPPPingPacket;

typedef union
{
    struct
    {
        uint8_t  header[4];  //FPPD
        uint8_t  packet_type;
        uint16_t data_len;
        uint8_t sync_action;
        uint8_t sync_type;
        uint32_t frame_number;
        float  seconds_elapsed;
        char filename[250];
    } __attribute__ ((packed));
    uint8_t raw[301];
} FPPMultiSyncPacket;

// https://github.com/FalconChristmas/fpp/blob/master/docs/FSEQ_Sequence_File_Format.txt
struct FSEQRawVariableDataHeader
{
    uint8_t    length[2];
    char       type[2];
    uint8_t    data;
};

struct FSEQParsedVariableDataHeader
{
    uint16_t    length;
    char        type[2];
    String      Data;
};

struct FSEQRawRangeEntry
{
    uint8_t Start[3];
    uint8_t Length[3];

} __attribute__ ((packed));

struct FSEQParsedRangeEntry
{
    uint32_t DataOffset;
    uint32_t ChannelCount;
};

struct FSEQRawHeader
{
    uint8_t  header[4];    // PSEQ
    uint8_t  dataOffset[2];
    uint8_t  minorVersion;
    uint8_t  majorVersion;
    uint8_t  VariableHdrOffset[2];
    uint8_t  channelCount[4];
    uint8_t  TotalNumberOfFramesInSequence[4];
    uint8_t  stepTime;
    uint8_t  flags;
    uint8_t  compressionType;
    uint8_t  numCompressedBlocks;
    uint8_t  numSparseRanges;
    uint8_t  flags2;
    uint8_t  id[8];
} __attribute__ ((packed));

struct FSEQParsedHeader
{
    uint8_t  header[4];    // PSEQ
    uint16_t dataOffset;
    uint8_t  minorVersion;
    uint8_t  majorVersion;
    uint16_t VariableHdrOffset;
    uint32_t channelCount;
    uint32_t TotalNumberOfFramesInSequence;
    uint8_t  stepTime;
    uint8_t  flags;
    uint8_t  compressionType;
    uint8_t  numCompressedBlocks;
    uint8_t  numSparseRanges;
    uint8_t  flags2;
    uint64_t id;
};

inline uint64_t read64 (uint8_t* buf, int idx) {
    uint32_t r1 = (int)(buf[idx + 3]) << 24;
    r1 |= (int)(buf[idx + 2]) << 16;
    r1 |= (int)(buf[idx + 1]) << 8;
    r1 |= (int)(buf[idx]);

    uint32_t r2 = (int)(buf[idx + 7]) << 24;
    r2 |= (int)(buf[idx + 6]) << 16;
    r2 |= (int)(buf[idx + 5]) << 8;
    r2 |= (int)(buf[idx + 4]);
    uint64_t r = r2;
    r <<= 32;
    r |= r1;
    return r;
}
//-----------------------------------------------------------------------------
inline uint32_t read32 (uint8_t* buf, int idx) {
    uint32_t r = (int)(buf[idx + 3]) << 24;
    r |= (int)(buf[idx + 2]) << 16;
    r |= (int)(buf[idx + 1]) << 8;
    r |= (int)(buf[idx]);
    return r;
}

inline void write32 (uint8_t* pData, uint32_t value)
{
    pData[0] = (value >>  0) & 0xff;
    pData[1] = (value >>  8) & 0xff;
    pData[2] = (value >> 16) & 0xff;
    pData[3] = (value >> 24) & 0xff;
} // write16

//-----------------------------------------------------------------------------
inline uint32_t read24 (uint8_t* pData)
{
    return ((uint32_t)(pData[0]) |
        (uint32_t)(pData[1]) << 8 |
        (uint32_t)(pData[2]) << 16);
} // read24

//-----------------------------------------------------------------------------
inline uint16_t read16 (uint8_t* pData)
{
    return ((uint16_t)(pData[0]) |
        (uint16_t)(pData[1]) << 8);
} // read16

inline void write16 (uint8_t* pData, uint16_t value)
{
    pData[0] = value & 0xff;
    pData[1] = (value >> 8) & 0xff;
} // write16
