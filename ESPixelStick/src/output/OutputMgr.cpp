/*
* OutputMgr.cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
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
*   This is a factory class used to manage the output port. It creates and deletes
*   the output channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "../ESPixelStick.h"
#include "../FileMgr.hpp"

//-----------------------------------------------------------------------------
// bring in driver definitions
#include "OutputDisabled.hpp"
#include "OutputAPA102Spi.hpp"
#include "OutputGECE.hpp"
#include "OutputRelay.hpp"
#include "OutputSerialUart.hpp"
#include "OutputSerialRmt.hpp"
#include "OutputServoPCA9685.hpp"
#include "OutputTM1814Rmt.hpp"
#include "OutputTM1814Uart.hpp"
#include "OutputUCS1903Rmt.hpp"
#include "OutputUCS1903Uart.hpp"
#include "OutputWS2801Spi.hpp"
#include "OutputWS2811Rmt.hpp"
#include "OutputWS2811Uart.hpp"
#include "OutputGS8208Uart.hpp"
#include "OutputGS8208Rmt.hpp"
#include "OutputUCS8903Uart.hpp"
#include "OutputUCS8903Rmt.hpp"
// needs to be last
#include "OutputMgr.hpp"

#include "../input/InputMgr.hpp"

//-----------------------------------------------------------------------------
// Local Data definitions
//-----------------------------------------------------------------------------
typedef struct
{
    c_OutputMgr::e_OutputType id;
    String name;
}OutputTypeXlateMap_t;

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
typedef struct
{
    gpio_num_t dataPin;
    uart_port_t UartId;
} OutputChannelIdToGpioAndPortEntry_t;

//-----------------------------------------------------------------------------
static const OutputChannelIdToGpioAndPortEntry_t OutputChannelIdToGpioAndPort[] =
    {
#ifdef DEFAULT_UART_1_GPIO
        {DEFAULT_UART_1_GPIO, UART_NUM_1},
#endif // def DEFAULT_UART_1_GPIO

#ifdef DEFAULT_UART_2_GPIO
        {DEFAULT_UART_2_GPIO, UART_NUM_2},
#endif // def DEFAULT_UART_2_GPIO

    // RMT ports
#ifdef DEFAULT_RMT_0_GPIO
        {DEFAULT_RMT_0_GPIO, uart_port_t(0)},
#endif // def DEFAULT_RMT_0_GPIO

#ifdef DEFAULT_RMT_1_GPIO
        {DEFAULT_RMT_1_GPIO, uart_port_t(1)},
#endif // def DEFAULT_RMT_1_GPIO

#ifdef DEFAULT_RMT_2_GPIO
        {DEFAULT_RMT_2_GPIO, uart_port_t(2)},
#endif // def DEFAULT_RMT_2_GPIO

#ifdef DEFAULT_RMT_3_GPIO
        {DEFAULT_RMT_3_GPIO, uart_port_t(3)},
#endif // def DEFAULT_RMT_3_GPIO

#ifdef DEFAULT_RMT_4_GPIO
        {DEFAULT_RMT_4_GPIO, uart_port_t(4)},
#endif // def DEFAULT_RMT_4_GPIO

#ifdef DEFAULT_RMT_5_GPIO
        {DEFAULT_RMT_5_GPIO, uart_port_t(5)},
#endif // def DEFAULT_RMT_5_GPIO

#ifdef DEFAULT_RMT_6_GPIO
        {DEFAULT_RMT_6_GPIO, uart_port_t(6)},
#endif // def DEFAULT_RMT_6_GPIO

#ifdef DEFAULT_RMT_7_GPIO
        {DEFAULT_RMT_7_GPIO, uart_port_t(7)},
#endif // def DEFAULT_RMT_7_GPIO

#ifdef SUPPORT_SPI_OUTPUT
        {DEFAULT_SPI_DATA_GPIO, uart_port_t(-1)},
#endif

#ifdef DEFAULT_RELAY_GPIO
        {DEFAULT_RELAY_GPIO, uart_port_t(-1)},
#endif // def DEFAULT_RELAY_GPIO

};

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_OutputMgr::c_OutputMgr ()
{
    ConfigFileName = String (F ("/")) + String (CN_output_config) + F (".json");

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
            logcon("ERROR: No output Channels defined. Rebooting");
            reboot = true;
            break;
        }

        HasBeenInitialized = true;

#ifdef LED_FLASH_GPIO
        pinMode (LED_FLASH_GPIO, OUTPUT);
        digitalWrite (LED_FLASH_GPIO, LED_FLASH_OFF);
#endif // def LED_FLASH_GPIO

        // make sure the pointers are set up properly
        uint8_t index = 0;
        for (DriverInfo_t &CurrentOutputChannelDriver : OutputChannelDrivers)
        {
            CurrentOutputChannelDriver.DriverId = e_OutputChannelIds(index++);
            InstantiateNewOutputChannel(CurrentOutputChannelDriver, e_OutputType::OutputType_Disabled);
            // DEBUG_V ("");
        }

        // load up the configuration from the saved file. This also starts the drivers
        LoadConfig ();

        // CreateNewConfig ();
    } while (false);

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_OutputMgr::CreateJsonConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    // extern void PrettyPrint (JsonObject&, String);
    // PrettyPrint (jsonConfig, String ("jsonConfig"));

    // add OM config parameters
    // DEBUG_V ("");
    
    // add the channels header
    JsonObject OutputMgrChannelsData;
    if (true == jsonConfig.containsKey (CN_channels))
    {
        // DEBUG_V ("");
        OutputMgrChannelsData = jsonConfig[CN_channels];
    }
    else
    {
        // add our section header
        // DEBUG_V ("");
        OutputMgrChannelsData = jsonConfig.createNestedObject (CN_channels);
    }

    // add the channel configurations
    // DEBUG_V ("For Each Output Channel");
    for (auto & CurrentChannel : OutputChannelDrivers)
    {
        // DEBUG_V (String("Create Section in Config file for the output channel: '") + CurrentChannel->GetOutputChannelId() + "'");
        // create a record for this channel
        JsonObject ChannelConfigData;
        String sChannelId = String(CurrentChannel.pOutputChannelDriver->GetOutputChannelId());
        if (true == OutputMgrChannelsData.containsKey (sChannelId))
        {
            // DEBUG_V ("");
            ChannelConfigData = OutputMgrChannelsData[sChannelId];
        }
        else
        {
            // add our section header
            // DEBUG_V ("");
            ChannelConfigData = OutputMgrChannelsData.createNestedObject (sChannelId);
        }

        // save the name as the selected channel type
        ChannelConfigData[CN_type] = int(CurrentChannel.pOutputChannelDriver->GetOutputType());

        String DriverTypeId = String(int(CurrentChannel.pOutputChannelDriver->GetOutputType()));
        JsonObject ChannelConfigByTypeData;
        if (true == ChannelConfigData.containsKey (String (DriverTypeId)))
        {
            ChannelConfigByTypeData = ChannelConfigData[DriverTypeId];
            // DEBUG_V ("");
        }
        else
        {
            // add our section header
            // DEBUG_V ("");
            ChannelConfigByTypeData = ChannelConfigData.createNestedObject (DriverTypeId);
        }

        // DEBUG_V ("");
        // PrettyPrint (ChannelConfigByTypeData, String ("jsonConfig"));

        // ask the channel to add its data to the record
        // DEBUG_V ("Add the output channel configuration for type: " + DriverTypeId);

        // Populate the driver name
        String DriverName = "";
        CurrentChannel.pOutputChannelDriver->GetDriverName(DriverName);
        // DEBUG_V (String ("DriverName: ") + DriverName);

        ChannelConfigByTypeData[CN_type] = DriverName;

        // DEBUG_V ("");
        // PrettyPrint (ChannelConfigByTypeData, String ("jsonConfig"));
        // DEBUG_V ("");

        CurrentChannel.pOutputChannelDriver->GetConfig(ChannelConfigByTypeData);

        // DEBUG_V ("");
        // PrettyPrint (ChannelConfigByTypeData, String ("jsonConfig"));
        // DEBUG_V ("");

    } // for each output channel

    // DEBUG_V ("");
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
    // extern void PrettyPrint(JsonObject & jsonStuff, String Name);

    // DEBUG_V (String (F ("--- WARNING: Creating a new Output Manager configuration Data set ---")));

    BuildingNewConfig = true;

    // create a place to save the config
    DynamicJsonDocument JsonConfigDoc (OM_MAX_CONFIG_SIZE);
    // DEBUG_V ("");

    JsonObject JsonConfig = JsonConfigDoc.createNestedObject (CN_output_config);
    // DEBUG_V ("");

    JsonConfig[CN_cfgver] = CurrentConfigVersion;
    JsonConfig[F ("MaxChannels")] = sizeof(OutputBuffer);

    // DEBUG_V ("for each output type");
    for (auto CurrentOutputType : OutputTypeXlateMap)
    {
        // DEBUG_V ("for each interface");
        for (DriverInfo_t & CurrentOutputChannelDriver : OutputChannelDrivers)
        {
            // DEBUG_V(String("DriverId: ") + String(CurrentOutput.DriverId));
            // DEBUG_V (String ("instantiate output type: ") + String (outputTypeId));
            InstantiateNewOutputChannel(CurrentOutputChannelDriver, CurrentOutputType.id, false);
            // DEBUG_V ("");
        } // end for each interface

        // PrettyPrint(JsonConfig, "Intermediate in OutputMgr");

        // DEBUG_V ("collect the config data for this output type");
        CreateJsonConfig (JsonConfig);
        // DEBUG_V(String("       Heap: ") + String(ESP.getFreeHeap()));
        // DEBUG_V(String(" overflowed: ") + String(JsonConfigDoc.overflowed()));
        // DEBUG_V(String("memoryUsage: ") + String(JsonConfigDoc.memoryUsage()));
        // PrettyPrint(JsonConfig, "Final Port in OutputMgr");

        // DEBUG_V ("");
    } // end for each output type

    // DEBUG_V ("leave the outputs disabled");
    for (auto & CurrentOutputChannelDriver : OutputChannelDrivers)
    {
        InstantiateNewOutputChannel(CurrentOutputChannelDriver, e_OutputType::OutputType_Disabled);
    }// end for each interface

    // PrettyPrint(JsonConfig, "Complete OutputMgr");

    // DEBUG_V ("Outputs Are disabled");

    CreateJsonConfig (JsonConfig);

    // DEBUG_V (String ("       Heap: ") + String (ESP.getFreeHeap ()));
    // DEBUG_V (String (" overflowed: ") + String (JsonConfigDoc.overflowed()));
    // DEBUG_V (String ("memoryUsage: ") + String (JsonConfigDoc.memoryUsage()));

    SetConfig(JsonConfigDoc);

    // DEBUG_V (String (F ("--- WARNING: Creating a new Output Manager configuration Data set - Done ---")));

    BuildingNewConfig = true;

    // DEBUG_END;

} // CreateNewConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::GetConfig (String & Response)
{
    // DEBUG_START;

    FileMgr.ReadConfigFile (ConfigFileName, Response);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::GetConfig (byte * Response, size_t maxlen )
{
    // DEBUG_START;

    FileMgr.ReadConfigFile (ConfigFileName, Response, maxlen);

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonArray OutputStatus = jsonStatus.createNestedArray (CN_output);
    for (auto & CurrentOutput : OutputChannelDrivers)
    {
        // DEBUG_V("");
        JsonObject channelStatus = OutputStatus.createNestedObject ();
        CurrentOutput.pOutputChannelDriver->GetStatus(channelStatus);
        // DEBUG_V("");
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
void c_OutputMgr::InstantiateNewOutputChannel(DriverInfo_t & CurrentOutputChannelDriver, e_OutputType NewOutputChannelType, bool StartDriver)
{
    // DEBUG_START;

    do // once
    {
        // is there an existing driver?
        if (nullptr != CurrentOutputChannelDriver.pOutputChannelDriver)
        {
            // DEBUG_V (String ("OutputChannelDrivers[uint(CurrentOutputChannel.DriverId)]->GetOutputType () '") + String (OutputChannelDrivers[uint(CurrentOutputChannel.DriverId)]->GetOutputType()) + String ("'"));
            // DEBUG_V (String ("NewOutputChannelType '") + int(NewOutputChannelType) + "'");

            // DEBUG_V ("does the driver need to change?");
            if (CurrentOutputChannelDriver.pOutputChannelDriver->GetOutputType() == NewOutputChannelType)
            {
                // DEBUG_V ("nothing to change");
                break;
            }

            String DriverName;
            CurrentOutputChannelDriver.pOutputChannelDriver->GetDriverName(DriverName);
            if (!IsBooting)
            {
                logcon(String(F(" Shutting Down '")) + DriverName + String(F("' on Output: ")) + String(CurrentOutputChannelDriver.DriverId));
            }
            delete CurrentOutputChannelDriver.pOutputChannelDriver;
            CurrentOutputChannelDriver.pOutputChannelDriver = nullptr;
            // DEBUG_V ("");
        } // end there is an existing driver

        // DEBUG_V ("");

        // get the new data and UART info
        gpio_num_t dataPin = OutputChannelIdToGpioAndPort[CurrentOutputChannelDriver.DriverId].dataPin;
        uart_port_t UartId = OutputChannelIdToGpioAndPort[CurrentOutputChannelDriver.DriverId].UartId;

        switch (NewOutputChannelType)
        {
            case e_OutputType::OutputType_Disabled:
            {
                // logcon (CN_stars + String (F (" Disabled output type for channel '")) + CurrentOutputChannel.DriverId + "'. **************");
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }

#if defined(SUPPORT_OutputType_DMX)
            case e_OutputType::OutputType_DMX:
            {
#ifdef SUPPORT_UART_OUTPUT
                if ((CurrentOutputChannelDriver.DriverId >= OutputChannelId_UART_FIRST) && (CurrentOutputChannelDriver.DriverId <= OutputChannelId_UART_LAST))
                {
                    // logcon (CN_stars + String (F (" Starting DMX for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputSerialUart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_DMX);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_UART_OUTPUT

                // DEBUG_V ("");
                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start DMX for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
#endif // defined(SUPPORT_OutputType_DMX)

#if defined(SUPPORT_OutputType_GECE)
            case e_OutputType::OutputType_GECE:
            {
                if ((CurrentOutputChannelDriver.DriverId >= OutputChannelId_UART_FIRST) && (CurrentOutputChannelDriver.DriverId <= OutputChannelId_UART_LAST))
                {
                    // logcon (CN_stars + String (F (" Starting GECE for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputGECE(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_GECE);
                    // DEBUG_V ("");
                    break;
                }
                // DEBUG_V ("");

                if (!BuildingNewConfig)
                {
                     logcon (CN_stars + String (F (" Cannot Start GECE for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");

                break;
            }
#endif // defined(SUPPORT_OutputType_GECE)

#ifdef SUPPORT_OutputType_Serial
            case e_OutputType::OutputType_Serial:
            {
#ifdef SUPPORT_UART_OUTPUT
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting Generic Serial for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputSerialUart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Serial);
                    // DEBUG_V ("");
                    break;
                }
                // DEBUG_V ("");
#endif // def SUPPORT_UART_OUTPUT

#ifdef SUPPORT_RMT_OUTPUT
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting Generic Serial for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputSerialRmt(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Serial);
                    // DEBUG_V ("");
                    break;
                }
                // DEBUG_V ("");
#endif // def SUPPORT_RMT_OUTPUT

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Generic Serial for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");

                break;
            }
#endif // def SUPPORT_OutputType_Serial

#ifdef SUPPORT_OutputType_Renard
            case e_OutputType::OutputType_Renard:
            {
#ifdef SUPPORT_UART_OUTPUT
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting Renard for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputSerialUart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Renard);
                    // DEBUG_V ("");
                    break;
                }
                // DEBUG_V ("");
#endif // def SUPPORT_UART_OUTPUT

#ifdef xSUPPORT_RMT_OUTPUT
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting Renard for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputSerialRmt(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Renard);
                    // DEBUG_V ("");
                    break;
                }
                // DEBUG_V ("");
#endif // def SUPPORT_RMT_OUTPUT

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Renard for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");

                break;
            }
#endif // def SUPPORT_OutputType_Renard

#ifdef SUPPORT_OutputType_Relay
            case e_OutputType::OutputType_Relay:
            {
                if (CurrentOutputChannelDriver.DriverId == OutputChannelId_Relay)
                {
                    // logcon (CN_stars + String (F (" Starting RELAY for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputRelay(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Relay);
                    // DEBUG_V ("");
                    break;
                }
                // DEBUG_V ("");

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start RELAY for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");

                break;
            }
#endif // def SUPPORT_OutputType_Relay

#ifdef SUPPORT_OutputType_Servo_PCA9685
            case e_OutputType::OutputType_Servo_PCA9685:
            {
                if (CurrentOutputChannelDriver.DriverId == OutputChannelId_Relay)
                {
                    // logcon (CN_stars + String (F (" Starting Servo PCA9685 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputServoPCA9685(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Servo_PCA9685);
                    // DEBUG_V ("");
                    break;
                }
                // DEBUG_V ("");

                if (!BuildingNewConfig) 
                {
                    logcon(CN_stars + String(F(" Cannot Start Servo PCA9685 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
#endif // SUPPORT_OutputType_Servo_PCA9685

#ifdef SUPPORT_OutputType_WS2811
            case e_OutputType::OutputType_WS2811:
            {
#ifdef SUPPORT_RMT_OUTPUT
                // DEBUG_V("RMT");
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting WS2811 RMT for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputWS2811Rmt(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_WS2811);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_RMT_OUTPUT

#ifdef SUPPORT_UART_OUTPUT
                // DEBUG_V ("UART");
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting WS2811 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputWS2811Uart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_WS2811);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_UART_OUTPUT

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start WS2811 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");

                break;
            }
#endif // def SUPPORT_OutputType_WS2811

#ifdef SUPPORT_OutputType_UCS1903
            case e_OutputType::OutputType_UCS1903:
            {
#ifdef SUPPORT_RMT_OUTPUT
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting TM1814 RMT for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputUCS1903Rmt(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_UCS1903);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_RMT_OUTPUT
#ifdef SUPPORT_UART_OUTPUT
                // DEBUG_V ("");
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting TM1814 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputUCS1903Uart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_UCS1903);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_UART_OUTPUT

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start UCS1903 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
#endif // def SUPPORT_OutputType_UCS1903

#ifdef SUPPORT_OutputType_TM1814
            case e_OutputType::OutputType_TM1814:
            {
#ifdef SUPPORT_RMT_OUTPUT
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting TM1814 RMT for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputTM1814Rmt(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_TM1814);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_RMT_OUTPUT
#ifdef SUPPORT_UART_OUTPUT
                // DEBUG_V ("");
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting TM1814 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputTM1814Uart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_TM1814);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_UART_OUTPUT

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start TM1814 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
#endif // def SUPPORT_OutputType_TM1814

#ifdef SUPPORT_OutputType_WS2801
            case e_OutputType::OutputType_WS2801:
            {
                if (CurrentOutputChannelDriver.DriverId == OutputChannelId_SPI_1)
                {
                    // logcon (CN_stars + String (F (" Starting WS2811 RMT for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputWS2801Spi(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_WS2801);
                    // DEBUG_V ("");
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start WS2801 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");

                break;
            }
#endif // def SUPPORT_OutputType_WS2801

#ifdef SUPPORT_OutputType_APA102
            case e_OutputType::OutputType_APA102:
            {
                if (CurrentOutputChannelDriver.DriverId == OutputChannelId_SPI_1)
                {
                    // logcon (CN_stars + String (F (" Starting WS2811 RMT for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputAPA102Spi(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_APA102);
                    // DEBUG_V ("");
                    break;
                }

                if (!BuildingNewConfig) 
                {
                    logcon(CN_stars + String(F(" Cannot Start WS2801 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");

                break;
            }
#endif // def SUPPORT_OutputType_APA102

#ifdef SUPPORT_OutputType_GS8208
            case e_OutputType::OutputType_GS8208:
            {
#ifdef SUPPORT_RMT_OUTPUT
                if (OM_IS_RMT)
                {
                    // logcon (CN_stars + String (F (" Starting GS8208 RMT for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputGS8208Rmt(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_GS8208);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_RMT_OUTPUT
#ifdef SUPPORT_UART_OUTPUT
                // DEBUG_V ("");
                if (OM_IS_UART)
                {
                    // logcon (CN_stars + String (F (" Starting GS8208 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputGS8208Uart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_GS8208);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_UART_OUTPUT

                if (!BuildingNewConfig) 
                {
                    logcon(CN_stars + String(F(" Cannot Start GS8208 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
#endif // def SUPPORT_OutputType_GS8208

#ifdef SUPPORT_OutputType_UCS8903
            case e_OutputType::OutputType_UCS8903:
            {
#ifdef SUPPORT_RMT_OUTPUT
                if (OM_IS_RMT)
                {
                    // DEBUG_V(CN_stars + String(F(" Starting UCS8903 RMT for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputUCS8903Rmt(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_UCS8903);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_RMT_OUTPUT
#ifdef SUPPORT_UART_OUTPUT
                // DEBUG_V ("");
                if (OM_IS_UART)
                {
                    // DEBUG_V(CN_stars + String(F(" Starting UCS8903 UART for channel '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputUCS8903Uart(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_UCS8903);
                    // DEBUG_V ("");
                    break;
                }
#endif // def SUPPORT_UART_OUTPUT

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start UCS8903 for channel '")) + CurrentOutputChannelDriver.DriverId + "'. " + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
#endif // def SUPPORT_OutputType_UCS8903

            default:
            {
                if (!IsBooting)
                {
                    logcon(CN_stars + String(F(" Unknown output type: '")) + String(NewOutputChannelType) + String(F(" for channel '")) + CurrentOutputChannelDriver.DriverId + String(F("'. Using disabled. ")) + CN_stars);
                }
                CurrentOutputChannelDriver.pOutputChannelDriver = new c_OutputDisabled(CurrentOutputChannelDriver.DriverId, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
        } // end switch (NewChannelType)

        // DEBUG_V ("");
        String sDriverName;
        CurrentOutputChannelDriver.pOutputChannelDriver->GetDriverName(sDriverName);
        if (!IsBooting)
        {
            logcon("'" + sDriverName + F("' Initialization for Output: ") + String(CurrentOutputChannelDriver.DriverId));
        }
        if (StartDriver)
        {
            // DEBUG_V("Starting Driver");
            CurrentOutputChannelDriver.pOutputChannelDriver->Begin();
        }

    } while (false);

    // String temp;
    // CurrentOutputChannel.pOutputChannelDriver->GetDriverName (temp);
    // DEBUG_V (String ("Driver Name: ") + temp);

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
    
    // try to load and process the config file
    if (!FileMgr.LoadConfigFile(ConfigFileName, [this](DynamicJsonDocument &JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject JsonConfig = JsonConfigDoc.as<JsonObject> ();
            // DEBUG_V ("");
            this->ProcessJsonConfig (JsonConfig);
            // DEBUG_V (""); }))
        }))
    {
        if (!IsBooting)
        {
            logcon(CN_stars + String(F(" Error loading Output Manager Config File ")) + CN_stars);
        }

        // create a config file with default values
        // DEBUG_V ("");
        CreateNewConfig ();
    }

    // DEBUG_END;

} // LoadConfig

//-----------------------------------------------------------------------------
/*
    check the contents of the config and send
    the proper portion of the config to the currently instantiated channels

    needs
        ref to data from config file
    returns
        true - config was properly processes
        false - config had an error.
*/
bool c_OutputMgr::ProcessJsonConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;
    bool Response = false;

    // DEBUG_V ("");

    do // once
    {
        if (false == jsonConfig.containsKey (CN_output_config))
        {
            logcon (String (F ("No Output Interface Settings Found. Using Defaults")));
            break;
        }
        JsonObject OutputChannelMgrData = jsonConfig[CN_output_config];
        // DEBUG_V ("");

        uint8_t TempVersion = !CurrentConfigVersion;
        setFromJSON (TempVersion, OutputChannelMgrData, CN_cfgver);

        // DEBUG_V (String ("TempVersion: ") + String (TempVersion));
        // DEBUG_V (String ("CurrentConfigVersion: ") + String (CurrentConfigVersion));
        // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
        // PrettyPrint (OutputChannelMgrData, "Output Config");
        if (TempVersion != CurrentConfigVersion)
        {
            logcon (String (F ("OutputMgr: Incorrect Version found. Using existing/default config.")));
            // break;
        }

        // do we have a channel configuration array?
        if (false == OutputChannelMgrData.containsKey (CN_channels))
        {
            // if not, flag an error and stop processing
            logcon (String (F ("No Output Channel Settings Found. Using Defaults")));
            break;
        }
        JsonObject OutputChannelArray = OutputChannelMgrData[CN_channels];
        // DEBUG_V ("");

        // for each output channel
        for (auto & CurrentOutputChannelDriver : OutputChannelDrivers)
        {
            // get access to the channel config
            if (false == OutputChannelArray.containsKey(String(CurrentOutputChannelDriver.DriverId).c_str()))
            {
                // if not, flag an error and stop processing
                logcon(String(F("No Output Settings Found for Channel '")) + CurrentOutputChannelDriver.DriverId + String(F("'. Using Defaults")));
                break;
            }
            JsonObject OutputChannelConfig = OutputChannelArray[String(CurrentOutputChannelDriver.DriverId).c_str()];
            // DEBUG_V ("");

            // set a default value for channel type
            uint32_t ChannelType = uint32_t (OutputType_End);
            setFromJSON (ChannelType, OutputChannelConfig, CN_type);
            // DEBUG_V ("");

            // is it a valid / supported channel type
            if (ChannelType >= uint32_t (OutputType_End))
            {
                // if not, flag an error and move on to the next channel
                logcon(String(F("Invalid Channel Type in config '")) + ChannelType + String(F("'. Specified for channel '")) + CurrentOutputChannelDriver.DriverId + "'. Disabling channel");
                InstantiateNewOutputChannel(CurrentOutputChannelDriver, e_OutputType::OutputType_Disabled);
                continue;
            }
            // DEBUG_V ("");

            // do we have a configuration for the channel type?
            if (false == OutputChannelConfig.containsKey (String (ChannelType)))
            {
                // if not, flag an error and stop processing
                logcon(String(F("No Output Settings Found for Channel '")) + CurrentOutputChannelDriver.DriverId + String(F("'. Using Defaults")));
                InstantiateNewOutputChannel(CurrentOutputChannelDriver, e_OutputType::OutputType_Disabled);
                continue;
            }

            JsonObject OutputChannelDriverConfig = OutputChannelConfig[String (ChannelType)];
            // DEBUG_V ("");

            // make sure the proper output type is running
            InstantiateNewOutputChannel(CurrentOutputChannelDriver, e_OutputType(ChannelType));
            // DEBUG_V ("");

            // send the config to the driver. At this level we have no idea what is in it
            CurrentOutputChannelDriver.pOutputChannelDriver->SetConfig(OutputChannelDriverConfig);

        } // end for each channel

        // all went well
        Response = true;

    } while (false);

    // did we get a valid config?
    if (false == Response)
    {
        // save the current config since it is the best we have.
        // DEBUG_V ("");
        CreateNewConfig ();
    }

    UpdateDisplayBufferReferences ();

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

    if (true == FileMgr.SaveConfigFile (ConfigFileName, ConfigData))
    {
        ConfigLoadNeeded = true;
    } // end we got a config and it was good
    else
    {
        logcon (CN_stars + String (F (" Error Saving Output Manager Config File ")) + CN_stars);
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
    // DEBUG_V("");

    if (true == FileMgr.SaveConfigFile(ConfigFileName, ConfigData))
    {
        ConfigLoadNeeded = true;
    } // end we got a config and it was good
    else
    {
        logcon(CN_stars + String(F(" Error Saving Output Manager Config File ")) + CN_stars);
    }

    // DEBUG_END;

} // SaveConfig

//-----------------------------------------------------------------------------
///< Called from loop(), renders output data
void c_OutputMgr::Render()
{
    // DEBUG_START;
    // do we need to save the current config?
    if (true == ConfigLoadNeeded)
    {
        ConfigLoadNeeded = false;
        LoadConfig ();
    } // done need to save the current config

    if (false == IsOutputPaused)
    {
        // DEBUG_START;
        for (DriverInfo_t & OutputChannel : OutputChannelDrivers)
        {
            OutputChannel.pOutputChannelDriver->Render ();
        }
    }
    // DEBUG_END;
} // render

//-----------------------------------------------------------------------------
void c_OutputMgr::UpdateDisplayBufferReferences (void)
{
    // DEBUG_START;

    size_t OutputBufferOffset = 0;

    // DEBUG_V (String ("        BufferSize: ") + String (sizeof(OutputBuffer)));
    // DEBUG_V (String ("OutputBufferOffset: ") + String (OutputBufferOffset));

    for (auto & OutputChannel : OutputChannelDrivers)
    {
        OutputChannel.StartingChannelId = OutputBufferOffset;
        OutputChannel.pOutputChannelDriver->SetOutputBufferAddress(&OutputBuffer[OutputBufferOffset]);

        size_t ChannelsNeeded     = OutputChannel.pOutputChannelDriver->GetNumChannelsNeeded ();
        size_t AvailableChannels  = sizeof(OutputBuffer) - OutputBufferOffset;
        size_t ChannelsToAllocate = min (ChannelsNeeded, AvailableChannels);

        // DEBUG_V (String ("    ChannelsNeeded: ") + String (ChannelsNeeded));
        // DEBUG_V (String (" AvailableChannels: ") + String (AvailableChannels));
        // DEBUG_V (String ("ChannelsToAllocate: ") + String (ChannelsToAllocate));

        OutputChannel.pOutputChannelDriver->SetOutputBufferSize (ChannelsToAllocate);
        OutputChannel.ChannelCount = ChannelsToAllocate;
        OutputChannel.EndChannelId = OutputBufferOffset + ChannelsToAllocate;

        if (AvailableChannels < ChannelsNeeded)
        {
            logcon (String (F ("--- OutputMgr: ERROR: Too many output channels have been Requested: ")) + String (ChannelsNeeded));
            // DEBUG_V (String ("    ChannelsNeeded: ") + String (ChannelsNeeded));
            // DEBUG_V (String (" AvailableChannels: ") + String (AvailableChannels));
            // DEBUG_V (String ("ChannelsToAllocate: ") + String (ChannelsToAllocate));
        }

        OutputBufferOffset += ChannelsToAllocate;
        // DEBUG_V (String ("pOutputChannel->GetBufferUsedSize: ") + String (pOutputChannel->GetBufferUsedSize ()));
        // DEBUG_V (String ("OutputBufferOffset: ") + String(OutputBufferOffset));
    }

    // DEBUG_V (String ("   TotalBufferSize: ") + String (OutputBufferOffset));
    UsedBufferSize = OutputBufferOffset;
    // DEBUG_V (String ("       OutputBuffer: 0x") + String (uint32_t (OutputBuffer), HEX));
    // DEBUG_V (String ("     UsedBufferSize: ") + String (uint32_t (UsedBufferSize)));
    InputMgr.SetBufferInfo (OutputBufferOffset);

    // DEBUG_END;

} // UpdateDisplayBufferReferences

//-----------------------------------------------------------------------------
void c_OutputMgr::PauseOutputs (void)
{
    // DEBUG_START;

    for (auto & CurrentOutput : OutputChannelDrivers)
    {
        CurrentOutput.pOutputChannelDriver->PauseOutput ();
    }

    // DEBUG_END;
} // PauseOutputs

//-----------------------------------------------------------------------------
void c_OutputMgr::WriteChannelData(size_t StartChannelId, size_t ChannelCount, byte *pSourceData)
{
    // DEBUG_START;

    do // once
    {
        if (((StartChannelId + ChannelCount) > UsedBufferSize) || (0 == ChannelCount))
        {
            // DEBUG_V(String("ERROR: Invalid parameters"));
            // DEBUG_V(String("StartChannelId: ") + String(StartChannelId, HEX));
            // DEBUG_V(String("  ChannelCount: ") + String(ChannelCount));
            // DEBUG_V(String("UsedBufferSize: ") + String(UsedBufferSize));
            break;
        }

        // DEBUG_V(String("&OutputBuffer[StartChannelId]: 0x") + String(uint(&OutputBuffer[StartChannelId]), HEX));
        size_t EndChannelId = StartChannelId + ChannelCount;
        // Serial.print('1');
        for (auto & currentOutputChannelDriver : OutputChannelDrivers)
        {
            // Serial.print('2');
            // does this output handle this block of data?
            if (StartChannelId < currentOutputChannelDriver.StartingChannelId)
            {
                // we have gone beyond where we can put this data.
                // Serial.print('3');
                break;
            }

            if (StartChannelId > currentOutputChannelDriver.EndChannelId)
            {
                // move to the next driver
                // Serial.print('4');
                continue;
            }
            // Serial.print('5');

            size_t lastChannelToSet = min(EndChannelId, currentOutputChannelDriver.EndChannelId);
            size_t ChannelsToSet = lastChannelToSet - StartChannelId;
            size_t RelativeStartChannelId = StartChannelId - currentOutputChannelDriver.StartingChannelId;
            // DEBUG_V(String("               StartChannelId: 0x") + String(StartChannelId, HEX));
            // DEBUG_V(String("                 EndChannelId: 0x") + String(EndChannelId, HEX));
            // DEBUG_V(String("             lastChannelToSet: 0x") + String(lastChannelToSet, HEX));
            // DEBUG_V(String("                ChannelsToSet: 0x") + String(ChannelsToSet, HEX));
            if (ChannelsToSet)
            {
                currentOutputChannelDriver.pOutputChannelDriver->WriteChannelData(RelativeStartChannelId, ChannelsToSet, pSourceData);
            }
            StartChannelId += ChannelsToSet;
            pSourceData += ChannelsToSet;
            // memcpy(&OutputBuffer[StartChannelId], pSourceData, ChannelCount);
        }

    } while (false);
    // DEBUG_END;

} // WriteChannelData

//-----------------------------------------------------------------------------
void c_OutputMgr::ReadChannelData(size_t StartChannelId, size_t ChannelCount, byte *pTargetData)
{
    // DEBUG_START;

    do // once
    {
        if ((StartChannelId + ChannelCount) > UsedBufferSize)
        {
            // DEBUG_V(String("ERROR: Invalid parameters"));
            // DEBUG_V(String("StartChannelId: ") + String(StartChannelId, HEX));
            // DEBUG_V(String("  ChannelCount: ") + String(ChannelCount));
            // DEBUG_V(String("UsedBufferSize: ") + String(UsedBufferSize));
            break;
        }
        // DEBUG_V(String("&OutputBuffer[StartChannelId]: 0x") + String(uint(&OutputBuffer[StartChannelId]), HEX));
        size_t EndChannelId = StartChannelId + ChannelCount;
        // Serial.print('1');
        for (auto & currentOutputChannelDriver : OutputChannelDrivers)
        {
            // Serial.print('2');
            // does this output handle this block of data?
            if (StartChannelId < currentOutputChannelDriver.StartingChannelId)
            {
                // we have gone beyond where we can put this data.
                // Serial.print('3');
                break;
            }

            if (StartChannelId > currentOutputChannelDriver.EndChannelId)
            {
                // move to the next driver
                // Serial.print('4');
                continue;
            }
            // Serial.print('5');

            size_t lastChannelToSet = min(EndChannelId, currentOutputChannelDriver.EndChannelId);
            size_t ChannelsToSet = lastChannelToSet - StartChannelId;
            size_t RelativeStartChannelId = StartChannelId - currentOutputChannelDriver.StartingChannelId;
            // DEBUG_V(String("               StartChannelId: 0x") + String(StartChannelId, HEX));
            // DEBUG_V(String("                 EndChannelId: 0x") + String(EndChannelId, HEX));
            // DEBUG_V(String("             lastChannelToSet: 0x") + String(lastChannelToSet, HEX));
            // DEBUG_V(String("                ChannelsToSet: 0x") + String(ChannelsToSet, HEX));
            currentOutputChannelDriver.pOutputChannelDriver->ReadChannelData(RelativeStartChannelId, ChannelsToSet, pTargetData);
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

    memset(OutputBuffer, 0x00, sizeof(OutputBuffer));

    // DEBUG_END;

} // ClearBuffer

// create a global instance of the output channel factory
c_OutputMgr OutputMgr;
