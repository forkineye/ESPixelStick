/*
* OutputMgr.cpp - Output Management class
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
*   This is a factory class used to manage the output port. It creates and deletes
*   the output channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "ESPixelStick.h"
#include "FileMgr.hpp"
#include <TimeLib.h>

//-----------------------------------------------------------------------------
// bring in driver definitions
#include "output/OutputDisabled.hpp"
#include "output/OutputAPA102Spi.hpp"
#include "output/OutputGECEUart.hpp"
#include "output/OutputGECERmt.hpp"
#include "output/OutputGrinchSpi.hpp"
#include "output/OutputRelay.hpp"
#include "output/OutputSerialUart.hpp"
#include "output/OutputSerialRmt.hpp"
#include "output/OutputServoPCA9685.hpp"
#include "output/OutputTM1814Rmt.hpp"
#include "output/OutputTM1814Uart.hpp"
#include "output/OutputUCS1903Rmt.hpp"
#include "output/OutputUCS1903Uart.hpp"
#include "output/OutputWS2801Spi.hpp"
#include "output/OutputWS2811Rmt.hpp"
#include "output/OutputWS2811Uart.hpp"
#include "output/OutputGS8208Uart.hpp"
#include "output/OutputGS8208Rmt.hpp"
#include "output/OutputUCS8903Uart.hpp"
#include "output/OutputUCS8903Rmt.hpp"
// needs to be last
#include "output/OutputMgr.hpp"

#include "input/InputMgr.hpp"

#ifndef DEFAULT_RELAY_GPIO
    #define DEFAULT_RELAY_GPIO      gpio_num_t::GPIO_NUM_1
#endif // ndef DEFAULT_RELAY_GPIO

//-----------------------------------------------------------------------------
// Local Data definitions
//-----------------------------------------------------------------------------
struct OutputTypeXlateMap_t
{
    c_OutputMgr::e_OutputType id;
    String name;
};

static const OutputTypeXlateMap_t OutputTypeXlateMap[c_OutputMgr::e_OutputType::OutputType_End] =
{
#ifdef SUPPORT_OutputType_APA102
        {c_OutputMgr::e_OutputType::OutputType_APA102, "APA102"},
#endif // def SUPPORT_OutputType_APA102

#ifdef SUPPORT_OutputType_DMX
        {c_OutputMgr::e_OutputType::OutputType_DMX, "DMX"},
#endif // def SUPPORT_OutputType_DMX

#ifdef SUPPORT_OutputType_GECE
        {c_OutputMgr::e_OutputType::OutputType_GECE, "GECE"},
#endif // def SUPPORT_OutputType_GECE

#ifdef SUPPORT_OutputType_GRINCH
        {c_OutputMgr::e_OutputType::OutputType_GRINCH, "Grinch"},
#endif // def SUPPORT_OutputType_GRINCH

#ifdef SUPPORT_OutputType_GS8208
        {c_OutputMgr::e_OutputType::OutputType_GS8208, "GS8208"},
#endif // def SUPPORT_OutputType_GS8208

#ifdef SUPPORT_OutputType_Renard
        {c_OutputMgr::e_OutputType::OutputType_Renard, "Renard"},
#endif // def SUPPORT_OutputType_Renard

#ifdef SUPPORT_OutputType_Serial
        {c_OutputMgr::e_OutputType::OutputType_Serial, "Serial"},
#endif // def SUPPORT_OutputType_Serial

#ifdef SUPPORT_OutputType_TM1814
        {c_OutputMgr::e_OutputType::OutputType_TM1814, "TM1814"},
#endif // def SUPPORT_OutputType_TM1814

#ifdef SUPPORT_OutputType_UCS1903
        {c_OutputMgr::e_OutputType::OutputType_UCS1903, "UCS1903"},
#endif // def SUPPORT_OutputType_UCS1903

#ifdef SUPPORT_OutputType_UCS8903
        {c_OutputMgr::e_OutputType::OutputType_UCS8903, "UCS8903"},
#endif // def SUPPORT_OutputType_UCS8903

#ifdef SUPPORT_OutputType_WS2801
        {c_OutputMgr::e_OutputType::OutputType_WS2801, "WS2801"},
#endif // def SUPPORT_OutputType_WS2801

#ifdef SUPPORT_OutputType_WS2811
        {c_OutputMgr::e_OutputType::OutputType_WS2811, "WS2811"},
#endif // def SUPPORT_OutputType_WS2811

#ifdef SUPPORT_OutputType_Relay
        {c_OutputMgr::e_OutputType::OutputType_Relay, "Relay"},
#endif // def SUPPORT_OutputType_Servo_PCA9685
#ifdef SUPPORT_OutputType_Servo_PCA9685
        {c_OutputMgr::e_OutputType::OutputType_Servo_PCA9685, "Servo_PCA9685"},
#endif // def SUPPORT_OutputType_Servo_PCA9685

        {c_OutputMgr::e_OutputType::OutputType_Disabled, "Disabled"},
};

//-----------------------------------------------------------------------------
struct OutputChannelIdToGpioAndPortEntry_t
{
    gpio_num_t                  GpioPin;
    uart_port_t                 PortId;
    c_OutputMgr::OM_PortType_t  PortType;
};

//-----------------------------------------------------------------------------
static const OutputChannelIdToGpioAndPortEntry_t OutputChannelIdToGpioAndPort[] =
{
#ifdef DEFAULT_UART_0_GPIO
    {DEFAULT_UART_0_GPIO, UART_NUM_0, c_OutputMgr::OM_PortType_t::Uart},
#endif // def DEFAULT_UART_0_GPIO

#ifdef DEFAULT_UART_1_GPIO
    {DEFAULT_UART_1_GPIO, UART_NUM_1, c_OutputMgr::OM_PortType_t::Uart},
#endif // def DEFAULT_UART_1_GPIO

#ifdef DEFAULT_UART_2_GPIO
    {DEFAULT_UART_2_GPIO, UART_NUM_2, c_OutputMgr::OM_PortType_t::Uart},
#endif // def DEFAULT_UART_2_GPIO

    // RMT ports
#ifdef DEFAULT_RMT_0_GPIO
    {DEFAULT_RMT_0_GPIO, uart_port_t(0), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_0_GPIO

#ifdef DEFAULT_RMT_1_GPIO
    {DEFAULT_RMT_1_GPIO, uart_port_t(1), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_1_GPIO

#ifdef DEFAULT_RMT_2_GPIO
    {DEFAULT_RMT_2_GPIO, uart_port_t(2), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_2_GPIO

#ifdef DEFAULT_RMT_3_GPIO
    {DEFAULT_RMT_3_GPIO, uart_port_t(3), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_3_GPIO

#ifdef DEFAULT_RMT_4_GPIO
    {DEFAULT_RMT_4_GPIO, uart_port_t(4), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_4_GPIO

#ifdef DEFAULT_RMT_5_GPIO
    {DEFAULT_RMT_5_GPIO, uart_port_t(5), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_5_GPIO

#ifdef DEFAULT_RMT_6_GPIO
    {DEFAULT_RMT_6_GPIO, uart_port_t(6), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_6_GPIO

#ifdef DEFAULT_RMT_7_GPIO
    {DEFAULT_RMT_7_GPIO, uart_port_t(7), c_OutputMgr::OM_PortType_t::Rmt},
#endif // def DEFAULT_RMT_7_GPIO

#ifdef SUPPORT_SPI_OUTPUT
    {DEFAULT_SPI_DATA_GPIO, uart_port_t(-1), c_OutputMgr::OM_PortType_t::Spi},
#endif

#if defined(SUPPORT_OutputType_Relay) || defined(SUPPORT_OutputType_Servo_PCA9685)
    {DEFAULT_RELAY_GPIO, uart_port_t(-1), c_OutputMgr::OM_PortType_t::Relay},
#endif // defined(SUPPORT_OutputType_Relay) || defined(SUPPORT_OutputType_Servo_PCA9685)

};

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_OutputMgr::c_OutputMgr ()
{
    ConfigFileName = String ("/") + String (CN_output_config) + CN_Dotjson;

    // clear the input data buffer
    memset ((char*)&OutputBuffer[0], 0, sizeof (OutputBuffer));

} // c_OutputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_OutputMgr::~c_OutputMgr()
{
    // DEBUG_START;

    // delete pOutputInstances;
    for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
    {
        // the drivers will put the hardware in a safe state
        delete CurrentOutput.pOutputChannelDriver;
    }
    // DEBUG_END;

} // ~c_OutputMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_OutputMgr::Begin ()
{
    // DEBUG_START;
    // DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));

    // IsBooting = false;
    // FileMgr.DeleteConfigFile(ConfigFileName);

    do // once
    {
        // prevent recalls
        if (true == HasBeenInitialized)
        {
            break;
        }

        if (0 == OutputChannelId_End)
        {
            logcon("ERROR: No compiled output Channels defined. Rebooting");
            RequestReboot(100000);
            break;
        }

        HasBeenInitialized = true;

#ifdef LED_FLASH_GPIO
        ResetGpio(LED_FLASH_GPIO);
        pinMode (LED_FLASH_GPIO, OUTPUT);
        digitalWrite (LED_FLASH_GPIO, LED_FLASH_OFF);
#endif // def LED_FLASH_GPIO

        // make sure the pointers are set up properly
        uint8_t index = 0;
        for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
        {
            // DEBUG_V(String("init index: ") + String(index) + " Start");
            CurrentOutput.DriverId = e_OutputChannelIds(index++);
            InstantiateNewOutputChannel(CurrentOutput, e_OutputType::OutputType_Disabled);
            // DEBUG_V(String("init index: ") + String(index) + " Done");
        }

        // DEBUG_V("load up the configuration from the saved file. This also starts the drivers");
        LoadConfig();

        // CreateNewConfig();

        // Preset the output memory
        memset((void*)&OutputBuffer[0], 0x00, sizeof(OutputBuffer));

    } while (false);

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_OutputMgr::CreateJsonConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    // PrettyPrint (jsonConfig, String ("jsonConfig"));

    // add OM config parameters
    // DEBUG_V ();

    // add the channels header
    JsonObject OutputMgrChannelsData = jsonConfig[(char*)CN_channels];
    if (!OutputMgrChannelsData)
    {
        // DEBUG_V ();
        OutputMgrChannelsData = jsonConfig[(char*)CN_channels].to<JsonObject> ();
    }

    // add the channel configurations
    // DEBUG_V ("For Each Output Channel");
    for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
    {
        // DEBUG_V (String("Create Section in Config file for the output channel: '") + CurrentOutput.pOutputChannelDriver->GetOutputChannelId() + "'");
        // create a record for this channel
        String sChannelId = String(CurrentOutput.pOutputChannelDriver->GetOutputChannelId());
        JsonObject ChannelConfigData = OutputMgrChannelsData[sChannelId];
        if (!ChannelConfigData)
        {
            // DEBUG_V (String ("add our channel section header. Chan: ") + sChannelId);
            ChannelConfigData = OutputMgrChannelsData[sChannelId].to<JsonObject> ();
        }

        // save the name as the selected channel type
        JsonWrite(ChannelConfigData, CN_type, int(CurrentOutput.pOutputChannelDriver->GetOutputType()));

        String DriverTypeId = String(int(CurrentOutput.pOutputChannelDriver->GetOutputType()));
        JsonObject ChannelConfigByTypeData = ChannelConfigData[String (DriverTypeId)];
        if (!ChannelConfigByTypeData)
        {
            // DEBUG_V (String("Add Channel Type Data for chan: ") + sChannelId + " Type: " + DriverTypeId);
            ChannelConfigByTypeData = ChannelConfigData[DriverTypeId].to<JsonObject> ();
        }

        // DEBUG_V ();
        // PrettyPrint (ChannelConfigByTypeData, String ("jsonConfig"));

        // ask the channel to add its data to the record
        // DEBUG_V ("Add the output channel configuration for type: " + DriverTypeId);

        // Populate the driver name
        String DriverName = "";
        CurrentOutput.pOutputChannelDriver->GetDriverName(DriverName);
        // DEBUG_V (String ("DriverName: ") + DriverName);

        JsonWrite(ChannelConfigByTypeData, CN_type, DriverName);

        // DEBUG_V ();
        // PrettyPrint (ChannelConfigByTypeData, String ("jsonConfig"));
        // DEBUG_V ();

        CurrentOutput.pOutputChannelDriver->GetConfig(ChannelConfigByTypeData);

        // DEBUG_V ();
        // PrettyPrint (ChannelConfigByTypeData, String ("jsonConfig"));
        // DEBUG_V ();

    } // for each output channel

    // DEBUG_V ();
    // PrettyPrint (jsonConfig, String ("jsonConfig"));

    // smile. Your done
    // DEBUG_END;
} // CreateJsonConfig

//-----------------------------------------------------------------------------
/*
    The running config is made from a composite of running and not instantiated
    objects. To create a complete config we need to start each output type on
    each output channel and collect the configuration at each stage.
*/
void c_OutputMgr::CreateNewConfig ()
{
    // DEBUG_START;

    // DEBUG_V (String (("--- WARNING: Creating a new Output Manager configuration Data set ---")));

    BuildingNewConfig = true;

    // create a place to save the config
    JsonDocument JsonConfigDoc;
    // DEBUG_V ();

    // DEBUG_V("Create a new output config structure.");
    JsonObject JsonConfig = JsonConfigDoc[(char*)CN_output_config].to<JsonObject> ();
    // DEBUG_V ();

    JsonWrite(JsonConfig, CN_cfgver,      CurrentConfigVersion);
    JsonWrite(JsonConfig, CN_MaxChannels, sizeof(OutputBuffer));

    // DEBUG_V("Collect the all ports disabled config first");
    CreateJsonConfig (JsonConfig);

    // DEBUG_V ("for each output type");
    for (auto CurrentOutputType : OutputTypeXlateMap)
    {
        // DEBUG_V (String ("instantiate output type: ") + String (CurrentOutputType.name));
        // DEBUG_V ("for each interface");
        for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
        {
            // DEBUG_V (String("DriverId: ") + String(CurrentOutput.DriverId));
            InstantiateNewOutputChannel(CurrentOutput, CurrentOutputType.id, false);

            // DEBUG_V ();
        } // end for each interface

        if(JsonConfigDoc.overflowed())
        {
            logcon("Error: Config size is too small. Cannot create an output config with the current settings.")
            break;
        }

        // PrettyPrint(JsonConfig, "Intermediate in OutputMgr");

        // DEBUG_V ("collect the config data for this output type");
        CreateJsonConfig (JsonConfig);
        // DEBUG_V (String("       Heap: ") + String(ESP.getFreeHeap()));
        // DEBUG_V (String(" overflowed: ") + String(JsonConfigDoc.overflowed()));
        // DEBUG_V (String("memoryUsage: ") + String(JsonConfigDoc.memoryUsage()));
        // PrettyPrint(JsonConfig, "Final Port in OutputMgr");

        // DEBUG_V ();
    } // end for each output type

    // DEBUG_V ("leave the outputs disabled");
    for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
    {
        InstantiateNewOutputChannel(CurrentOutput, e_OutputType::OutputType_Disabled);
    }// end for each interface

    // PrettyPrint(JsonConfig, "Complete OutputMgr");

    // DEBUG_V ("Outputs Are disabled");

    CreateJsonConfig (JsonConfig);

    // DEBUG_V (String ("       Heap: ") + String (ESP.getFreeHeap ()));
    // DEBUG_V (String (" overflowed: ") + String (JsonConfigDoc.overflowed()));
    // DEBUG_V (String ("memoryUsage: ") + String (JsonConfigDoc.memoryUsage()));

    SetConfig(JsonConfigDoc);

    // DEBUG_V (String (("--- WARNING: Creating a new Output Manager configuration Data set - Done ---")));

    BuildingNewConfig = true;

    // DEBUG_END;

} // CreateNewConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::GetConfig (String & Response)
{
    // DEBUG_START;

    FileMgr.ReadFlashFile (ConfigFileName, Response);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::GetConfig (byte * Response, uint32_t maxlen )
{
    // DEBUG_START;

    FileMgr.ReadFlashFile (ConfigFileName, Response, maxlen);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

#if defined(ARDUINO_ARCH_ESP32)
    // jsonStatus["PollCount"] = PollCount;
#endif // defined(ARDUINO_ARCH_ESP32)

    JsonArray OutputStatus = jsonStatus[(char*)CN_output].to<JsonArray> ();
    for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
    {
        // DEBUG_V ();
        JsonObject channelStatus = OutputStatus.add<JsonObject> ();
        CurrentOutput.pOutputChannelDriver->GetStatus(channelStatus);
        // DEBUG_V ();
    }

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
/* Create an instance of the desired output type in the desired channel
*
* WARNING:  This function assumes there is a driver running in the identified
*           out channel. These must be set up and started when the manager is
*           started.
*
    needs
        channel ID
        channel type
    returns
        nothing
*/
void c_OutputMgr::InstantiateNewOutputChannel(DriverInfo_t & CurrentOutput, e_OutputType NewOutputChannelType, bool StartDriver)
{
    // DEBUG_START;
    // BuildingNewConfig = false;
    // IsBooting = false;
    do // once
    {
        // is there an existing driver?
        if (nullptr != CurrentOutput.pOutputChannelDriver)
        {
            // DEBUG_V (String("OutputChannelDrivers[uint(CurrentOutputChannel.DriverId)]->GetOutputType () '") + String(OutputChannelDrivers[uint(CurrentOutput.DriverId)].pOutputChannelDriver->GetOutputType()) + String("'"));
            // DEBUG_V (String ("NewOutputChannelType '") + int(NewOutputChannelType) + "'");

            // DEBUG_V ("does the driver need to change?");
            if (CurrentOutput.pOutputChannelDriver->GetOutputType() == NewOutputChannelType)
            {
                // DEBUG_V ("nothing to change");
                break;
            }

            String DriverName;
            CurrentOutput.pOutputChannelDriver->GetDriverName(DriverName);
            if (!IsBooting)
            {
                logcon(String(MN_12) + DriverName + MN_13 + String(CurrentOutput.DriverId));
            }

            delete CurrentOutput.pOutputChannelDriver;
            CurrentOutput.pOutputChannelDriver = nullptr;
            // DEBUG_V ();
        } // end there is an existing driver

        // DEBUG_V ();

        // get the new data and UART info
        CurrentOutput.GpioPin   = OutputChannelIdToGpioAndPort[CurrentOutput.DriverId].GpioPin;
        CurrentOutput.PortType  = OutputChannelIdToGpioAndPort[CurrentOutput.DriverId].PortType;
        CurrentOutput.PortId    = OutputChannelIdToGpioAndPort[CurrentOutput.DriverId].PortId;

        // give the other tasks a chance to run
        delay(100);

        // DEBUG_V (String("DriverId: ") + String(CurrentOutput.DriverId));
        // DEBUG_V (String(" GpioPin: ") + String(CurrentOutput.PortId));
        // DEBUG_V (String("PortType: ") + String(CurrentOutput.PortType));
        // DEBUG_V (String("  PortId: ") + String(CurrentOutput.PortId));

        switch (NewOutputChannelType)
        {
            case e_OutputType::OutputType_Disabled:
            {
                // logcon (CN_stars + String (CN_Disabled) + MN_06 + CurrentOutput.DriverId + "'. " + CN_stars);
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }

#if defined(SUPPORT_OutputType_DMX)
            case e_OutputType::OutputType_DMX:
            {
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (CN_DMX) + MN_06 + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputSerialUart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_DMX);
                    // DEBUG_V ();
                    break;
                }

                #if defined(ARDUINO_ARCH_ESP32)
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (CN_DMX) + MN_06 + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputSerialRmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_DMX);
                    // DEBUG_V ();
                    break;
                }
                #endif // defined(ARDUINO_ARCH_ESP32)

                // DEBUG_V ();
                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_DMX + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }
#endif // defined(SUPPORT_OutputType_DMX)

#if defined(SUPPORT_OutputType_GECE)
            case e_OutputType::OutputType_GECE:
            {
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (CN_GECE) + MN_06 + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputGECEUart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_GECE);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                #if defined(ARDUINO_ARCH_ESP32)
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (CN_GECE) + MN_06 + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputGECERmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_GECE);
                    // DEBUG_V ();
                    break;
                }
                #endif // defined(ARDUINO_ARCH_ESP32)

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_GECE + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // defined(SUPPORT_OutputType_GECE)

#ifdef SUPPORT_OutputType_Serial
            case e_OutputType::OutputType_Serial:
            {
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting Generic Serial for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputSerialUart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Serial);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                #if defined(ARDUINO_ARCH_ESP32)
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting Generic Serial for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputSerialRmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Serial);
                    // DEBUG_V ();
                    break;
                }
                #endif // defined(ARDUINO_ARCH_ESP32)
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Generic Serial for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // def SUPPORT_OutputType_Serial

#ifdef SUPPORT_OutputType_Renard
            case e_OutputType::OutputType_Renard:
            {
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting Renard for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputSerialUart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Renard);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                #if defined(ARDUINO_ARCH_ESP32)
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting Renard for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputSerialRmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Renard);
                    // DEBUG_V ();
                    break;
                }
                #endif // defined(ARDUINO_ARCH_ESP32)
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Renard for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // def SUPPORT_OutputType_Renard

#ifdef SUPPORT_OutputType_Relay
            case e_OutputType::OutputType_Relay:
            {
                if (CurrentOutput.PortType == OM_PortType_t::Relay)
                {
                    // logcon (CN_stars + String (F (" Starting RELAY for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputRelay(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Relay);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start RELAY for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // def SUPPORT_OutputType_Relay

#ifdef SUPPORT_OutputType_Servo_PCA9685
            case e_OutputType::OutputType_Servo_PCA9685:
            {
                if (CurrentOutput.PortType == OM_PortType_t::Relay)
                {
                    // logcon (CN_stars + String (F (" Starting Servo PCA9685 for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputServoPCA9685(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Servo_PCA9685);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Servo PCA9685 for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }
#endif // SUPPORT_OutputType_Servo_PCA9685

#ifdef SUPPORT_OutputType_WS2811
            case e_OutputType::OutputType_WS2811:
            {
                // DEBUG_V ("OutputType_WS2811");
                if (OM_IS_UART)
                {
                    // DEBUG_V ("UART");
                    // logcon (CN_stars + String (F (" Starting WS2811 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputWS2811Uart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_WS2811);
                    // DEBUG_V ();
                    break;
                }

                #if defined(ARDUINO_ARCH_ESP32)
                if (OM_IS_RMT)
                {
                    // DEBUG_V ("RMT");
                    // logcon (CN_stars + String (F (" Starting WS2811 RMT for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputWS2811Rmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_WS2811);
                    // DEBUG_V ();
                    break;
                }
                #endif // defined(ARDUINO_ARCH_ESP32)

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start WS2811 for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // def SUPPORT_OutputType_WS2811

#ifdef SUPPORT_OutputType_UCS1903
            case e_OutputType::OutputType_UCS1903:
            {
                // DEBUG_V ();
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting UCS1903 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputUCS1903Uart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_UCS1903);
                    // DEBUG_V ();
                    break;
                }

                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String ((" Starting UCS1903 RMT for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputUCS1903Rmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_UCS1903);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_UCS1903 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }
#endif // def SUPPORT_OutputType_UCS1903

#ifdef SUPPORT_OutputType_TM1814
            case e_OutputType::OutputType_TM1814:
            {
                // DEBUG_V ();
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String ((" Starting TM1814 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputTM1814Uart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_TM1814);
                    // DEBUG_V ();
                    break;
                }

                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String ((" Starting TM1814 RMT for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputTM1814Rmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_TM1814);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_TM1814 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }
#endif // def SUPPORT_OutputType_TM1814

#ifdef SUPPORT_OutputType_GRINCH
            case e_OutputType::OutputType_GRINCH:
            {
                if (CurrentOutput.PortType == OM_PortType_t::Spi)
                {
                    // logcon (CN_stars + String ((" Starting GRINCH SPI for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputGrinchSpi(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_GRINCH);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + F("Grinch") + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // def SUPPORT_OutputType_GRINCH

#ifdef SUPPORT_OutputType_WS2801
            case e_OutputType::OutputType_WS2801:
            {
                if (CurrentOutput.PortType == OM_PortType_t::Spi)
                {
                    // logcon (CN_stars + String ((" Starting WS2811 RMT for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputWS2801Spi(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_WS2801);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_WS2801 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // def SUPPORT_OutputType_WS2801

#ifdef SUPPORT_OutputType_APA102
            case e_OutputType::OutputType_APA102:
            {
                if (CurrentOutput.PortType == OM_PortType_t::Spi)
                {
                    // logcon (CN_stars + String ((" Starting WS2811 RMT for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputAPA102Spi(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_APA102);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_WS2801 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();

                break;
            }
#endif // def SUPPORT_OutputType_APA102

#ifdef SUPPORT_OutputType_GS8208
            case e_OutputType::OutputType_GS8208:
            {
                // DEBUG_V ();
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String ((" Starting GS8208 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputGS8208Uart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_GS8208);
                    // DEBUG_V ();
                    break;
                }

                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String ((" Starting GS8208 RMT for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputGS8208Rmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_GS8208);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_GS8208 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }
#endif // def SUPPORT_OutputType_GS8208

#ifdef SUPPORT_OutputType_UCS8903
            case e_OutputType::OutputType_UCS8903:
            {
                // DEBUG_V ();
                if (OM_IS_UART)
                {
                    // DEBUG_V (CN_stars + String((" Starting UCS8903 UART for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputUCS8903Uart(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_UCS8903);
                    // DEBUG_V ();
                    break;
                }

                if (OM_IS_RMT)
                {
                    // DEBUG_V (CN_stars + String((" Starting UCS8903 RMT for channel '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    CurrentOutput.pOutputChannelDriver = new c_OutputUCS8903Rmt(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_UCS8903);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_UCS8903 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }
#endif // def SUPPORT_OutputType_UCS8903

            default:
            {
                if (!IsBooting)
                {
                    logcon(CN_stars + String(MN_09) + String(NewOutputChannelType) + MN_10 + CurrentOutput.DriverId + MN_11 + CN_stars);
                }
                CurrentOutput.pOutputChannelDriver = new c_OutputDisabled(CurrentOutput.DriverId, CurrentOutput.GpioPin,  CurrentOutput.PortId, OutputType_Disabled);
                // DEBUG_V ();
                break;
            }
        } // end switch (NewChannelType)

        // DEBUG_V ();
        // DEBUG_V (String("pOutputChannelDriver: 0x") + String(uint32_t(CurrentOutput.pOutputChannelDriver),HEX));
        // DEBUG_V (String("heap: 0x") + String(uint32_t(ESP.getMaxFreeBlockSize()),HEX));

        String sDriverName;
        CurrentOutput.pOutputChannelDriver->GetDriverName(sDriverName);
        // DEBUG_V (String("Driver Name: ") + sDriverName);
        if (!IsBooting)
        {
            logcon("'" + sDriverName + MN_14 + String(CurrentOutput.DriverId));
        }
        if (StartDriver)
        {
            // DEBUG_V ("Starting Driver");
            CurrentOutput.pOutputChannelDriver->Begin();
        }

    } while (false);

    // DEBUG_END;

} // InstantiateNewOutputChannel

//-----------------------------------------------------------------------------
/* Load and process the current configuration
*
*   needs
*       Nothing
*   returns
*       Nothing
*/
void c_OutputMgr::LoadConfig ()
{
    // DEBUG_START;

    ConfigLoadNeeded = NO_CONFIG_NEEDED;
    ConfigInProgress = true;

    // try to load and process the config file
    if (!FileMgr.LoadFlashFile(ConfigFileName, [this](JsonDocument &JsonConfigDoc)
        {
            // PrettyPrint(JsonConfigDoc, "OM Load Config");

            // DEBUG_V ();
            // PrettyPrint(JsonConfig, "OM Load Config");
            // DEBUG_V ();
            this->ProcessJsonConfig(JsonConfigDoc);
            // DEBUG_V ();
        }))
    {
        if (IsBooting)
        {
            // on boot, create a config file with default values
            // DEBUG_V ();
            CreateNewConfig ();
        }
        else
        {
            logcon(CN_stars + String(MN_15) + CN_stars);
            RequestReboot(100000);
        }
    }

    ConfigInProgress = false;

    // DEBUG_END;

} // LoadConfig

//-----------------------------------------------------------------------------
bool c_OutputMgr::FindJsonChannelConfig (JsonDocument& jsonConfig,
                                         e_OutputChannelIds ChanId,
                                         e_OutputType Type,
                                         JsonObject& ChanConfig)
{
    // DEBUG_START;
    bool Response = false;
    // DEBUG_V ();
    // DEBUG_V(String("ChanId: ") + String(ChanId));
    // DEBUG_V(String("  Type: ") + String(Type));

    // PrettyPrint(jsonConfig, "FindJsonChannelConfig");

    do // once
    {
        JsonObject OutputChannelMgrData = jsonConfig[(char*)CN_output_config];
        if (!OutputChannelMgrData)
        {
            logcon(String(MN_16) + MN_18);
            break;
        }
        // DEBUG_V ();

        uint8_t TempVersion = !CurrentConfigVersion;
        setFromJSON (TempVersion, OutputChannelMgrData, CN_cfgver);

        // DEBUG_V (String ("TempVersion: ") + String (TempVersion));
        // DEBUG_V (String ("CurrentConfigVersion: ") + String (CurrentConfigVersion));
        // PrettyPrint (OutputChannelMgrData, "Output Config");
        if (TempVersion != CurrentConfigVersion)
        {
            logcon (MN_17);
            // break;
        }

        // do we have a channel configuration array?
        JsonObject OutputChannelArray = OutputChannelMgrData[(char*)CN_channels];
        if (!OutputChannelArray)
        {
            // if not, flag an error and stop processing
            logcon(String(MN_16) + MN_18);
            break;
        }
        // DEBUG_V ();

        // get access to the channel config
        ChanConfig = OutputChannelArray[String(ChanId)];
        if (!ChanConfig)
        {
            // if not, flag an error and stop processing
            logcon(String(MN_16) + ChanId + MN_18);
            break;
        }
        // PrettyPrint(ChanConfig, "ProcessJson Channel Config");

        // do we need to go deeper into the config?
        if(e_OutputType::OutputType_End == Type)
        {
            // DEBUG_V("only looking for the overall channel config");
            Response = true;
            break;
        }
        // go deeper and get the config for the specific channel type
        // PrettyPrint(ChanConfig, "ProcessJson Channel Config");

        // DEBUG_V ();
        JsonObject OutputChannelConfig = OutputChannelArray[String(ChanId)];
        // PrettyPrint (OutputChannelConfig, "Output Channel Config");

        // do we have a configuration for the channel type?
        ChanConfig = OutputChannelConfig[String (Type)];
        if (!ChanConfig)
        {
            // Not found
            logcon(String(MN_16) + ChanId + MN_18);
            break;
        }

        // DEBUG_V ();
        // PrettyPrint(OutputChannelConfig, "ProcessJson Channel Config");

        // all went well
        Response = true;

    } while (false);

    // DEBUG_END;
    return Response;

} // FindChannelJsonConfig

//-----------------------------------------------------------------------------
bool c_OutputMgr::ProcessJsonConfig (JsonDocument& jsonConfig)
{
    // DEBUG_START;
    bool Response = false;

    // DEBUG_V ();

    // PrettyPrint(jsonConfig, "ProcessJsonConfig");

    do // once
    {
        // for each output channel
        for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
        {
            JsonObject OutputChannelConfig;
            if(!FindJsonChannelConfig (jsonConfig, CurrentOutput.DriverId, e_OutputType::OutputType_End, OutputChannelConfig))
            {
                // DEBUG_V(String("cant find config for channel: ") + String(CurrentOutput.DriverId));
                continue;
            }
            // DEBUG_V ();

            // PrettyPrint(OutputChannelConfig, "ProcessJson Channel Config");

            // set a default value for channel type
            uint32_t ChannelType = uint32_t (OutputType_End);
            setFromJSON (ChannelType, OutputChannelConfig, CN_type);
            // DEBUG_V ();

            // is it a valid / supported channel type
            if (ChannelType >= uint32_t (OutputType_End))
            {
                // DEBUG_V (String("OutputType_End: ") + String(OutputType_End));
                // if not, flag an error and move on to the next channel
                logcon(String(MN_19) + ChannelType + MN_20 + CurrentOutput.DriverId + MN_03);
                InstantiateNewOutputChannel(CurrentOutput, e_OutputType::OutputType_Disabled);
                continue;
            }
            // DEBUG_V ();

            // do we have a configuration for the channel type?
            JsonObject OutputChannelDriverConfig = OutputChannelConfig[String (ChannelType)];
            if (!OutputChannelDriverConfig)
            {
                // if not, flag an error and stop processing
                logcon(String(MN_16) + CurrentOutput.DriverId + MN_18);
                InstantiateNewOutputChannel(CurrentOutput, e_OutputType::OutputType_Disabled);
                continue;
            }
            // DEBUG_V ();
            // PrettyPrint(OutputChannelDriverConfig, "ProcessJson Channel Driver Config before driver create");
            // DEBUG_V ();

            // make sure the proper output type is running
            InstantiateNewOutputChannel(CurrentOutput, e_OutputType(ChannelType));

            // DEBUG_V ();
            // PrettyPrint(OutputChannelDriverConfig, "ProcessJson Channel Driver Config");
            // DEBUG_V ();

            // send the config to the driver. At this level we have no idea what is in it
            CurrentOutput.pOutputChannelDriver->SetConfig(OutputChannelDriverConfig);
            // DEBUG_V ();

        } // end for each channel

        // all went well
        Response = true;

    } while (false);

    // did we get a valid config?
    if (false == Response)
    {
        // save the current config since it is the best we have.
        // DEBUG_V ();
        CreateNewConfig ();
    }

    // DEBUG_V ();
    UpdateDisplayBufferReferences ();
    // DEBUG_V ();

    SetSerialUart();

    // DEBUG_END;
    return Response;

} // ProcessJsonConfig

//-----------------------------------------------------------------------------
/*
*   Save the current configuration to the FS
*
*   needs
*       pointer to the config data array
*   returns
*       Nothing
*/
void c_OutputMgr::SetConfig (const char * ConfigData)
{
    // DEBUG_START;

    // DEBUG_V (String ("ConfigData: ") + ConfigData);

    if (true == FileMgr.SaveFlashFile (ConfigFileName, ConfigData))
    {
        ScheduleLoadConfig();
    } // end we got a config and it was good
    else
    {
        logcon (CN_stars + String (MN_21) + CN_stars);
    }

    // DEBUG_END;

} // SaveConfig

//-----------------------------------------------------------------------------
/*
 *   Save the current configuration to the FS
 *
 *   needs
 *       Reference to the JSON Document
 *   returns
 *       Nothing
 */
void c_OutputMgr::SetConfig(ArduinoJson::JsonDocument & ConfigData)
{
    // DEBUG_START;

    // serializeJson(ConfigData, LOG_PORT);
    // DEBUG_V ();

    if (true == FileMgr.SaveFlashFile(ConfigFileName, ConfigData))
    {
        ScheduleLoadConfig();
    } // end we got a config and it was good
    else
    {
        logcon(CN_stars + String(MN_21) + CN_stars);
    }

    // DEBUG_END;

} // SaveConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::SetSerialUart()
{
    // DEBUG_START;

    bool NeedToTurnOffConsole = false;

    // DEBUG_V(String("ConsoleTxGpio: ") + String(ConsoleTxGpio));
    // DEBUG_V(String("ConsoleRxGpio: ") + String(ConsoleRxGpio));

    for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
    {
        // DEBUG_V();
        if(e_OutputType::OutputType_Disabled != CurrentOutput.pOutputChannelDriver->GetOutputType())
        {
            // DEBUG_V (String("Output GPIO: ") + String(CurrentOutput.pOutputChannelDriver->GetOutputGpio()));

            NeedToTurnOffConsole |= CurrentOutput.pOutputChannelDriver->ValidateGpio(ConsoleTxGpio, ConsoleRxGpio);
        }
    } // end for each channel

    // DEBUG_V(String("NeedToTurnOffConsole: ") + String(NeedToTurnOffConsole));
    // DEBUG_V(String(" ConsoleUartIsActive: ") + String(ConsoleUartIsActive));
    if(NeedToTurnOffConsole && ConsoleUartIsActive)
    {
        logcon ("Found an Output that uses a Serial console GPIO. Turning off Serial console output.");
        LOG_PORT.flush();
        // Serial.end();
        ConsoleUartIsActive = false;
    }
    else if(!NeedToTurnOffConsole && !ConsoleUartIsActive)
    {
        Serial.end();
        Serial.begin(115200);
        ConsoleUartIsActive = true;
        // DEBUG_V("Turn ON Console");
    }
    else
    {
        // DEBUG_V("Leave Console Alone");
    }

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_OutputMgr::Poll()
{
    // //DEBUG_START;

#ifdef LED_FLASH_GPIO
    ResetGpio(LED_FLASH_GPIO);
    pinMode (LED_FLASH_GPIO, OUTPUT);
    digitalWrite (LED_FLASH_GPIO, LED_FLASH_OFF);
#endif // def LED_FLASH_GPIO

    // do we need to save the current config?
    if (NO_CONFIG_NEEDED != ConfigLoadNeeded)
    {
        if(abs(now() - ConfigLoadNeeded) > LOAD_CONFIG_DELAY)
        {
            LoadConfig ();
        }
    } // done need to save the current config

    if ((false == OutputIsPaused) && (false == ConfigInProgress) && (false == RebootInProgress()) )
    {
        // //DEBUG_V();
        for (DriverInfo_t & OutputChannel : OutputChannelDrivers)
        {
            // //DEBUG_V("Poll a channel");
            OutputChannel.pOutputChannelDriver->Poll ();
        }
    }

    // //DEBUG_END;
} // Poll

//-----------------------------------------------------------------------------
void c_OutputMgr::UpdateDisplayBufferReferences (void)
{
    // DEBUG_START;

    /* Buffer vs virtual buffer.
        Pixels have a concept called groups. A group of pixels uses
        a single tupple of input channel data and replicates that
        data into N positions in the output buffer. For non pixel
        channels or for pixels with a group size of 1, the virtual
        buffer and the output buffers are the same.
        The virtual buffer size is the one we give to the input
        processing engine
    */
    uint32_t OutputBufferOffset     = 0;    // offset into the raw data in the output buffer
    uint32_t OutputChannelOffset    = 0;    // Virtual channel offset to the output buffer.

    // DEBUG_V (String ("        BufferSize: ") + String (sizeof(OutputBuffer)));
    // DEBUG_V (String ("OutputBufferOffset: ") + String (OutputBufferOffset));

    for (DriverInfo_t & OutputChannel : OutputChannelDrivers)
    {
        // String DriverName;
        // OutputChannel.pOutputChannelDriver->GetDriverName(DriverName);
        // DEBUG_V(String("Name: ") + DriverName);
        // DEBUG_V(String("PortId: ") + String(OutputChannel.pOutputChannelDriver->GetOutputChannelId()) );

        OutputChannel.OutputBufferStartingOffset = OutputBufferOffset;
        OutputChannel.OutputChannelStartingOffset = OutputChannelOffset;
        OutputChannel.pOutputChannelDriver->SetOutputBufferAddress(&OutputBuffer[OutputBufferOffset]);

        uint32_t OutputBufferDataBytesNeeded        = OutputChannel.pOutputChannelDriver->GetNumOutputBufferBytesNeeded ();
        uint32_t VirtualOutputBufferDataBytesNeeded = OutputChannel.pOutputChannelDriver->GetNumOutputBufferChannelsServiced ();

        uint32_t AvailableChannels = sizeof(OutputBuffer) - OutputBufferOffset;

        if (AvailableChannels < OutputBufferDataBytesNeeded)
        {
            logcon (MN_22 + String (OutputBufferDataBytesNeeded));
            // DEBUG_V (String ("    ChannelsNeeded: ") + String (ChannelsNeeded));
            // DEBUG_V (String (" AvailableChannels: ") + String (AvailableChannels));
            // DEBUG_V (String ("ChannelsToAllocate: ") + String (ChannelsToAllocate));
            break;
        }

        // DEBUG_V (String ("    ChannelsNeeded: ") + String (OutputBufferDataBytesNeeded));
        // DEBUG_V (String (" AvailableChannels: ") + String (AvailableChannels));

        OutputBufferOffset += OutputBufferDataBytesNeeded;
        OutputChannel.OutputBufferDataSize  = OutputBufferDataBytesNeeded;
        OutputChannel.OutputBufferEndOffset = OutputBufferOffset - 1;
        OutputChannel.pOutputChannelDriver->SetOutputBufferSize (OutputBufferDataBytesNeeded);

        OutputChannelOffset += VirtualOutputBufferDataBytesNeeded;
        OutputChannel.OutputChannelSize      = VirtualOutputBufferDataBytesNeeded;
        OutputChannel.OutputChannelEndOffset = OutputChannelOffset;

        // DEBUG_V (String("OutputChannel.GetBufferUsedSize: ") + String(OutputChannel.pOutputChannelDriver->GetBufferUsedSize()));
        // DEBUG_V (String ("OutputBufferOffset: ") + String(OutputBufferOffset));
    }

    // DEBUG_V (String ("   TotalBufferSize: ") + String (OutputBufferOffset));
    UsedBufferSize = OutputBufferOffset;
    // DEBUG_V (String ("       OutputBuffer: 0x") + String (uint32_t (OutputBuffer), HEX));
    // DEBUG_V (String ("     UsedBufferSize: ") + String (uint32_t (UsedBufferSize)));
    InputMgr.SetBufferInfo (UsedBufferSize);

    // DEBUG_END;

} // UpdateDisplayBufferReferences

//-----------------------------------------------------------------------------
void c_OutputMgr::RelayUpdate (uint8_t RelayId, String & NewValue, String & Response)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V(String("e_OutputType::OutputType_Relay: ") + String(e_OutputType::OutputType_Relay));
        Response = F("Relay Output Not configured");
#ifdef SUPPORT_OutputType_Relay
        for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
        {
            // DEBUG_V(String("DriverId: ") + String(CurrentOutput.DriverId));
            // DEBUG_V(String("PortType: ") + String(CurrentOutput.pOutputChannelDriver->GetOutputType()));
            if(e_OutputType::OutputType_Relay == CurrentOutput.pOutputChannelDriver->GetOutputType())
            {
                ((c_OutputRelay*)CurrentOutput.pOutputChannelDriver)->RelayUpdate(RelayId, NewValue, Response);
                break;
            }
        }
#endif // def SUPPORT_OutputType_Relay

    } while(false);

    // DEBUG_END;
} // RelayUpdate

//-----------------------------------------------------------------------------
void c_OutputMgr::PauseOutputs(bool PauseTheOutput)
{
    // DEBUG_START;
    // DEBUG_V(String("PauseTheOutput: ") + String(PauseTheOutput));

    OutputIsPaused = PauseTheOutput;

    for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
    {
        CurrentOutput.pOutputChannelDriver->PauseOutput(PauseTheOutput);
    }

    // DEBUG_END;
} // PauseOutputs

//-----------------------------------------------------------------------------
void c_OutputMgr::WriteChannelData(uint32_t StartChannelId, uint32_t ChannelCount, uint8_t *pSourceData)
{
    // DEBUG_START;

    do // once
    {
        if(OutputIsPaused)
        {
            // DEBUG_V("Ignore the write request");
            break;
        }
        if (((StartChannelId + ChannelCount) > UsedBufferSize) || (0 == ChannelCount))
        {
            // DEBUG_V (String("ERROR: Invalid parameters"));
            // DEBUG_V (String("StartChannelId: ") + String(StartChannelId));
            // DEBUG_V (String("  ChannelCount: ") + String(ChannelCount));
            // DEBUG_V (String("UsedBufferSize: ") + String(UsedBufferSize));
            break;
        }

        // DEBUG_V (String("&OutputBuffer[StartChannelId]: 0x") + String(uint(&OutputBuffer[StartChannelId]), HEX));
        uint32_t EndChannelId = StartChannelId + ChannelCount;
        // Serial.print('1');
        for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
        {
            // Serial.print('2');
            // does this output handle this block of data?
            if (StartChannelId < CurrentOutput.OutputChannelStartingOffset)
            {
                // we have gone beyond where we can put this data.
                // Serial.print('3');
                break;
            }

            if (StartChannelId > CurrentOutput.OutputChannelEndOffset)
            {
                // move to the next driver
                // Serial.print('4');
                continue;
            }
            // Serial.print('5');

            uint32_t lastChannelToSet = min(EndChannelId, CurrentOutput.OutputChannelEndOffset);
            uint32_t ChannelsToSet = lastChannelToSet - StartChannelId;
            uint32_t RelativeStartChannelId = StartChannelId - CurrentOutput.OutputChannelStartingOffset;
            // DEBUG_V (String("               StartChannelId: 0x") + String(StartChannelId, HEX));
            // DEBUG_V (String("                 EndChannelId: 0x") + String(EndChannelId, HEX));
            // DEBUG_V (String("             lastChannelToSet: 0x") + String(lastChannelToSet, HEX));
            // DEBUG_V (String("                ChannelsToSet: 0x") + String(ChannelsToSet, HEX));
            if (ChannelsToSet)
            {
                CurrentOutput.pOutputChannelDriver->WriteChannelData(RelativeStartChannelId, ChannelsToSet, pSourceData);
            }
            StartChannelId += ChannelsToSet;
            pSourceData += ChannelsToSet;
            // memcpy(&OutputBuffer[StartChannelId], pSourceData, ChannelCount);
        }

    } while (false);
    // DEBUG_END;

} // WriteChannelData

//-----------------------------------------------------------------------------
void c_OutputMgr::ReadChannelData(uint32_t StartChannelId, uint32_t ChannelCount, byte *pTargetData)
{
    // DEBUG_START;

    do // once
    {
        if(OutputIsPaused)
        {
            // DEBUG_V("Ignore the read request");
            break;
        }
        if ((StartChannelId + ChannelCount) > UsedBufferSize)
        {
            // DEBUG_V (String("ERROR: Invalid parameters"));
            // DEBUG_V (String("StartChannelId: ") + String(StartChannelId, HEX));
            // DEBUG_V (String("  ChannelCount: ") + String(ChannelCount));
            // DEBUG_V (String("UsedBufferSize: ") + String(UsedBufferSize));
            break;
        }
        // DEBUG_V (String("&OutputBuffer[StartChannelId]: 0x") + String(uint(&OutputBuffer[StartChannelId]), HEX));
        uint32_t EndChannelId = StartChannelId + ChannelCount;
        // Serial.print('1');
        for (DriverInfo_t & CurrentOutput : OutputChannelDrivers)
        {
            // Serial.print('2');
            // does this output handle this block of data?
            if (StartChannelId < CurrentOutput.OutputChannelStartingOffset)
            {
                // we have gone beyond where we can put this data.
                // Serial.print('3');
                break;
            }

            if (StartChannelId > CurrentOutput.OutputChannelEndOffset)
            {
                // move to the next driver
                // Serial.print('4');
                continue;
            }
            // Serial.print('5');

            uint32_t lastChannelToSet = min(EndChannelId, CurrentOutput.OutputChannelEndOffset);
            uint32_t ChannelsToSet = lastChannelToSet - StartChannelId;
            uint32_t RelativeStartChannelId = StartChannelId - CurrentOutput.OutputChannelStartingOffset;
            // DEBUG_V (String("               StartChannelId: 0x") + String(StartChannelId, HEX));
            // DEBUG_V (String("                 EndChannelId: 0x") + String(EndChannelId, HEX));
            // DEBUG_V (String("             lastChannelToSet: 0x") + String(lastChannelToSet, HEX));
            // DEBUG_V (String("                ChannelsToSet: 0x") + String(ChannelsToSet, HEX));
            CurrentOutput.pOutputChannelDriver->ReadChannelData(RelativeStartChannelId, ChannelsToSet, pTargetData);
            StartChannelId += ChannelsToSet;
            pTargetData += ChannelsToSet;
            // memcpy(&OutputBuffer[StartChannelId], pTargetData, ChannelCount);
        }

    } while (false);
    // DEBUG_END;

} // ReadChannelData

//-----------------------------------------------------------------------------
void c_OutputMgr::ClearBuffer()
{
    // DEBUG_START;

    memset(GetBufferAddress(), 0x00, OutputMgr.GetBufferSize());

    // DEBUG_END;

} // ClearBuffer

// create a global instance of the output channel factory
c_OutputMgr OutputMgr;
