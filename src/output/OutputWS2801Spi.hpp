#pragma once
/*
* OutputWS2801Spi.h - WS2801 driver code for ESPixelStick Spi Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015 Shelby Merrick
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

#include "OutputWS2801.hpp"
#ifdef USE_WS2801

#include <driver/spi_master.h>
#include <esp_task.h>

class c_OutputWS2801Spi : public c_OutputWS2801
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputWS2801Spi (c_OutputMgr::e_OutputChannelIds OutputChannelId,
                      gpio_num_t outputGpio,
                      uart_port_t uart,
                      c_OutputMgr::e_OutputType outputType);
    virtual ~c_OutputWS2801Spi ();

    // functions to be provided by the derived class
    void    Begin ();
    void    GetConfig (ArduinoJson::JsonObject& jsonConfig);
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig);  ///< Set a new config in the driver
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    Render ();                                        ///< Call from loop(),  renders output data
    void    PauseOutput () {};
    TaskHandle_t GetTaskHandle () { return SendIntensityDataTaskHandle; }

    void DataOutputTask (void* pvParameters);
    void SendIntensityData ();

    uint32_t DataTaskcounter = 0;
    uint32_t DataCbCounter = 0;

private:
    void Shutdown ();

#define WS2801_SPI_MASTER_FREQ_1M                      (APB_CLK_FREQ/80) // 1Mhz
#define WS2801_NUM_TRANSACTIONS                 2
#define WS2801_NUM_INTENSITY_PER_TRANSACTION    128
#define WS2801_BITS_PER_INTENSITY               8
#define WS2801_SPI_HOST                         VSPI_HOST
#define WS2801_SPI_DMA_CHANNEL                  2

    uint8_t NumIntensityValuesPerInterrupt = 0;
    uint8_t NumIntensityBitsPerInterrupt = 0;
    spi_device_handle_t spi_device_handle = 0;
    gpio_num_t ClockPin = DEFAULT_WS2801_CLOCK_GPIO;

    // uint32_t FrameStartCounter = 0;
    uint32_t SendIntensityDataCounter = 0;
    // uint32_t FrameDoneCounter = 0;
    // uint32_t FrameEndISRcounter = 0;

    byte * TransactionBuffers[WS2801_NUM_TRANSACTIONS];
    spi_transaction_t Transactions[WS2801_NUM_TRANSACTIONS];
    uint8_t NextTransactionToFill = 0;

    TaskHandle_t SendIntensityDataTaskHandle = NULL;

}; // c_OutputWS2801Spi

#endif // def USE_WS2801
