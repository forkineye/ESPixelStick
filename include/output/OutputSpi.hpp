#pragma once
/*
* OutputSpi.h - SPI driver code for ESPixelStick Spi Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2024 Shelby Merrick
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
#ifdef SUPPORT_SPI_OUTPUT

#include "OutputPixel.hpp"
#include <driver/spi_master.h>
#include <esp_task.h>
#if defined(SUPPORT_OutputType_GRINCH)
    #include "OutputGrinch.hpp"
#endif // defined(SUPPORT_OutputType_GRINCH)

class c_OutputSpi
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputSpi ();
    virtual ~c_OutputSpi ();

    // functions to be provided by the derived class
    void    Begin (c_OutputPixel* _OutputPixel);
#if defined(SUPPORT_OutputType_GRINCH)
    void    Begin (c_OutputGrinch* _OutputSerial);
#endif // defined(SUPPORT_OutputType_GRINCH)
    bool    Poll ();                                        ///< Call from loop (),  renders output data
    TaskHandle_t GetTaskHandle () { return SendIntensityDataTaskHandle; }
    void    GetDriverName (String& Name) { Name = CN_OutputSpi; }
    void    DataOutputTask (void* pvParameters);
    void    SendIntensityData ();
    bool    SetConfig (ArduinoJson::JsonObject & jsonConfig);
    void    GetConfig (ArduinoJson::JsonObject & jsonConfig);

    uint32_t DataTaskcounter = 0;
    uint32_t DataCbCounter = 0;

private:

#define SPI_SPI_MASTER_FREQ_1M               (APB_CLK_FREQ/80) // 1Mhz
#define SPI_NUM_TRANSACTIONS                 2
#define SPI_NUM_INTENSITY_PER_TRANSACTION    128
#define SPI_BITS_PER_INTENSITY               8
#define SPI_SPI_HOST                         DEFAULT_SPI_DEVICE
#define SPI_SPI_DMA_CHANNEL                  2

    bool ISR_MoreDataToSend();
    bool ISR_GetNextIntensityToSend(uint32_t& Data);
    void StartNewFrame();

    uint8_t NumIntensityValuesPerInterrupt = 0;
    uint8_t NumIntensityBitsPerInterrupt = 0;
    spi_device_handle_t spi_device_handle = 0;

    // uint32_t FrameStartCounter = 0;
    uint32_t SendIntensityDataCounter = 0;
    // uint32_t FrameDoneCounter = 0;
    // uint32_t FrameEndISRcounter = 0;

    byte * TransactionBuffers[SPI_NUM_TRANSACTIONS];
    spi_transaction_t Transactions[SPI_NUM_TRANSACTIONS];
    uint8_t NextTransactionToFill = 0;
    TaskHandle_t SendIntensityDataTaskHandle = NULL;

#ifndef DEFAULT_SPI_CS_GPIO
#   define DEFAULT_SPI_CS_GPIO gpio_num_t(-1)
#endif // ndef DEFAULT_SPI_CS_GPIO

    gpio_num_t DataPin = DEFAULT_SPI_DATA_GPIO;
    gpio_num_t ClockPin = DEFAULT_SPI_CLOCK_GPIO;
    gpio_num_t CsPin = DEFAULT_SPI_CS_GPIO;

    c_OutputPixel* OutputPixel = nullptr;
#if defined(SUPPORT_OutputType_GRINCH)
    c_OutputGrinch * OutputGrinch = nullptr;
#endif // defined(SUPPORT_OutputType_GRINCH)

#ifndef HasBeenInitialized
    bool HasBeenInitialized = false;
#endif // ndef HasBeenInitialized

}; // c_OutputSpi

#endif // def SUPPORT_SPI_OUTPUT
