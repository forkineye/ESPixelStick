#pragma once
/*
* fseq.h

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

struct FSEQVariableDataHeader
{
    uint16_t    length;
    char        type[2];
//  uint8_t     data[???];

} __attribute__ ((packed));

struct FSEQRangeEntry
{
    uint8_t Start[3];
    uint8_t Length[3];

} __attribute__ ((packed));

struct FSEQHeader
{
    uint8_t  header[4];    // FSEQ
    uint16_t dataOffset;
    uint8_t  minorVersion;
    uint8_t  majorVersion;
    uint16_t headerLen;
    uint32_t channelCount;
    uint32_t TotalNumberOfFramesInSequence;
    uint8_t  stepTime;
    uint8_t  flags;
    uint8_t  compressionType;
    uint8_t  numCompressedBlocks;
    uint8_t  numSparseRanges;
    uint8_t  flags2;
    uint64_t id;
} __attribute__ ((packed));
