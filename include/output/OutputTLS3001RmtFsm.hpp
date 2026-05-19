#pragma once
/*
* OutputTLS3001RmtFsm.h - TLS3001 driver code for ESPixelStick RMT Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2026 Shelby Merrick
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/
#include "ESPixelStick.h"
#if defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)

// forward declaration
class c_OutputTLS3001Rmt;

enum OutputTLS3001RmtFsmStates_t
{
    TLS3001Reset = 0,
    TLS3001Sync,
    TLS3001DataStart,
    TLS3001DataSend,
    TLS3001DataIdle
};

/*****************************************************************************/
/*
 *	Generic fsm base class.
 */
/*****************************************************************************/
class fsm_RMT_state
{
public:
             fsm_RMT_state() {}
    virtual ~fsm_RMT_state() {}
            void SetParent(c_OutputTLS3001Rmt *_Parent) {Parent = _Parent;};
    virtual void GetDriverName (String& Name) = 0;
public:
    c_OutputTLS3001Rmt  *Parent = nullptr;
    uint32_t NumberOfOneBitsToSend  = 0;
    uint32_t CommandCodeMask        = 0;
    uint32_t NumberOfZeroBitsToSend = 0;
    uint32_t NumberOfIdleBitsToSend = 0;

}; // fsm_RMT_state

/*****************************************************************************/
class fsm_RMT_state_SendSync : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendSync() {}
    virtual ~fsm_RMT_state_SendSync() {}
            IRAM_ATTR void Init();
            IRAM_ATTR bool ISR_GetNextBitToSend (rmt_item32_t &DataToSend);
    virtual void GetDriverName (String& Name) { Name = "TLS3001SYN"; }
private:
    const uint8_t CommandCode = 0b0001;
}; // fsm_RMT_state_SendSync

/*****************************************************************************/
class fsm_RMT_state_SendReset : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendReset() {}
    virtual ~fsm_RMT_state_SendReset() {}
            IRAM_ATTR void Init();
            IRAM_ATTR bool ISR_GetNextBitToSend (rmt_item32_t &DataToSend);
    virtual void GetDriverName (String& Name) { Name = "TLS3001RST"; }
private:
    const uint8_t CommandCode = 0b0100;
}; // fsm_RMT_state_SendReset

/*****************************************************************************/
class fsm_RMT_state_SendDataStart : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendDataStart() {}
    virtual ~fsm_RMT_state_SendDataStart() {}
            IRAM_ATTR void Init();
            IRAM_ATTR bool ISR_GetNextBitToSend (rmt_item32_t &DataToSend);
    virtual void GetDriverName (String& Name) { Name = "TLS3001STR"; }
private:
    const uint8_t CommandCode = 0b0010;

}; // fsm_RMT_state_SendDataStart

/*****************************************************************************/
class fsm_RMT_state_SendData : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendData() {}
    virtual ~fsm_RMT_state_SendData() {}
            IRAM_ATTR void Init();
            IRAM_ATTR bool ISR_GetNextBitToSend (rmt_item32_t &DataToSend);
            IRAM_ATTR bool ISR_RefreshData ();
    virtual void GetDriverName (String& Name) { Name = "TLS3001DAT"; }
private:
    uint32_t DataPattern = 0;
    uint32_t DataPatternMask = 0;
    bool MoreDataAvailable = false;
    uint32_t NumPostDataBitsToSend = 0;
}; // fsm_RMT_state_SendData

/*****************************************************************************/
class fsm_RMT_state_SendDataIdle : public fsm_RMT_state
{
public:
    fsm_RMT_state_SendDataIdle() {}
    virtual ~fsm_RMT_state_SendDataIdle() {}
            IRAM_ATTR void Init();
            IRAM_ATTR bool ISR_GetNextBitToSend (rmt_item32_t &DataToSend);
    virtual void GetDriverName (String& Name) { Name = "TLS3001IDL"; }
private:
}; // fsm_RMT_state_SendData

#endif // defined(SUPPORT_OutputProtocol_TLS3001) && defined (ARDUINO_ARCH_ESP32)
