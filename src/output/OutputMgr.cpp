/*
* OutputMgr.cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2026 Shelby Merrick
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
#include "output/OutputTLS3001Rmt.hpp"
// needs to be last
#include "output/OutputMgr.hpp"

#include "input/InputMgr.hpp"

#ifndef DEFAULT_RELAY_GPIO
    #define DEFAULT_RELAY_GPIO      gpio_num_t::GPIO_NUM_1
#endif // ndef DEFAULT_RELAY_GPIO

#ifdef ARDUINO_ARCH_ESP32
    #define CLASS_TYPE_NAME(n)      n ## Rmt
#else
    #define CLASS_TYPE_NAME(n)      n ## Uart
#endif // def ARDUINO_ARCH_ESP32
    #define CLASS_TYPE_NO_NAME(n)   n

#define AllocatePort(ClassType, Output, OutputType) \
{ \
    static_assert(sizeof(Output.OutputDriver) >= sizeof(ClassType)); \
    new(&Output.OutputDriver) ClassType(Output.PortDefinition, OutputType); \
    Output.OutputDriverInUse = true; \
}

//-----------------------------------------------------------------------------
// Local Data definitions
//-----------------------------------------------------------------------------
struct SupportedOutputProtocol_t
{
    c_OutputMgr::e_OutputProtocolType ProtocolId;
    String ProtocolName;
    OM_PortType_t RequiredPortType;
};

static const SupportedOutputProtocol_t SupportedOutputProtocolList[] =
{
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_Disabled, "Disabled", OM_PortType_t::OM_ANY},

    #ifdef SUPPORT_OutputProtocol_WS2811
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_WS2811, "WS2811", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_WS2811

    #ifdef SUPPORT_OutputProtocol_GECE
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_GECE, "GECE", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_GECE

    #ifdef SUPPORT_OutputProtocol_DMX
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_DMX, "DMX", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_DMX

    #ifdef SUPPORT_OutputProtocol_Renard
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_Renard, "Renard", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_Renard

    #ifdef SUPPORT_OutputProtocol_Serial
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_Serial, "Serial", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_Serial

    #ifdef SUPPORT_OutputProtocol_Relay
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_Relay, "Relay", OM_PortType_t::OM_RELAY},
    #endif // def SUPPORT_OutputProtocol_Relay

    #ifdef SUPPORT_OutputProtocol_Servo_PCA9685
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_Servo_PCA9685, "Servo_PCA9685", OM_PortType_t::OM_I2C},
    #endif // def SUPPORT_OutputProtocol_Servo_PCA9685

    #ifdef SUPPORT_OutputProtocol_UCS1903
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_UCS1903, "UCS1903", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_UCS1903

    #ifdef SUPPORT_OutputProtocol_TM1814
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_TM1814, "TM1814", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_TM1814

    #ifdef SUPPORT_OutputProtocol_WS2801
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_WS2801, "WS2801", OM_PortType_t::OM_SPI},
    #endif // def SUPPORT_OutputProtocol_WS2801

    #ifdef SUPPORT_OutputProtocol_APA102
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_APA102, "APA102", OM_PortType_t::OM_SPI},
    #endif // def SUPPORT_OutputProtocol_APA102

    #ifdef SUPPORT_OutputProtocol_GS8208
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_GS8208, "GS8208", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_GS8208

    #ifdef SUPPORT_OutputProtocol_UCS8903
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_UCS8903, "UCS8903", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_UCS8903

    #ifdef SUPPORT_OutputProtocol_TLS3001
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_TLS3001, "TLS3001", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_TLS3001

    #ifdef SUPPORT_OutputProtocol_GRINCH
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_GRINCH, "Grinch", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_GRINCH

    #ifdef SUPPORT_OutputProtocol_FireGod
    {c_OutputMgr::e_OutputProtocolType::OutputProtocol_FireGod, "FireGod", OM_PortType_t::OM_SERIAL},
    #endif // def SUPPORT_OutputProtocol_FireGod
};

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_OutputMgr::c_OutputMgr ()
{
    ConfigFileName = String ("/") + String (CN_output_config) + CN_Dotjson;

    // clear the input data buffer
    pOutputBuffer = (uint8_t*)malloc(GetBufferSize() + 1);
    memset (pOutputBuffer, 0, GetBufferSize());

    uint32_t SizeOfProtocolEntry = uint32_t(&SupportedOutputProtocolList[1]) - uint32_t(&SupportedOutputProtocolList[0]);
    NumberOfOutputProtocols = uint32_t(sizeof(SupportedOutputProtocolList)) / SizeOfProtocolEntry;

    // find the highest numbered output
    for(auto & CurrentOutputPortDefinition : OM_OutputPortDefinitions)
    {
        if(CurrentOutputPortDefinition.PortId > NumOutputPorts)
        {
            // update the highest numbered port ID
            NumOutputPorts = CurrentOutputPortDefinition.PortId;
        }
    }
    // convert the zero based port ID into a port count
    NumOutputPorts++;

    // allocate a port driver control space
    pOutputChannelDrivers = nullptr;
    // DriverInfo_t is an aligned structure
    size_t alignment = alignof(DriverInfo_t);
    SizeOfTable = sizeof(DriverInfo_t) * NumOutputPorts;
    // Ensure total_size is a multiple of alignment (which it should be if sizeof(AlignedObject) is used)
    byte * raw_mem = (((byte*)malloc(SizeOfTable + (2*alignment))) + alignment);
    pOutputChannelDrivers = static_cast<DriverInfo_t*>((void*)raw_mem);
    memset((void*)pOutputChannelDrivers, 0x00, SizeOfTable);

    // Init the driver memory
    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        CurrentOutput.DriverId = index;
        CurrentOutput.OutputDriverInUse = false;
    }

} // c_OutputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_OutputMgr::~c_OutputMgr()
{
    // DEBUG_START;

    // delete pOutputInstances;
    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        // the drivers will put the hardware in a safe state
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).~c_OutputCommon();
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
    // DEBUG_V(String("NumOutputPorts: ") + String(NumOutputPorts));
    // DEBUG_V(String("   SizeOfEntry: ") + String(sizeof(DriverInfo_t)));
    // DEBUG_V(String("   SizeOfTable: ") + String(SizeOfTable));
    // DEBUG_V(String("AddressOfTable: 0x") + String(uint32_t(pOutputChannelDrivers), HEX));

    do // once
    {
        // prevent recalls
        if (true == HasBeenInitialized)
        {
            break;
        }

        if (0 == NumOutputPorts)
        {
            String Reason = F("ERROR: No compiled output Ports defined. Rebooting");
            RequestReboot(Reason, 100000);
            break;
        }

        HasBeenInitialized = true;

        #ifdef LED_FLASH_GPIO
        ResetGpio(LED_FLASH_GPIO);
        pinMode (LED_FLASH_GPIO, OUTPUT);
        digitalWrite (LED_FLASH_GPIO, LED_FLASH_OFF);
        #endif // def LED_FLASH_GPIO

        // make sure the pointers are set up properly
        for (uint8_t index = 0; index < NumOutputPorts; ++index)
        {
            DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
            // DEBUG_V(String("DriverId: ") + String(CurrentOutput.DriverId));
            InstantiateNewOutputChannel(CurrentOutput, e_OutputProtocolType::OutputProtocol_Disabled);
            // DEBUG_V(String("init index: ") + String(index) + " Done");
        }

        // DEBUG_V("Dump SupportedOutputProtocolList");
        // DEBUG_V(String("NumberOfOutputProtocols: ") + String(NumberOfOutputProtocols));
        // for (auto x : SupportedOutputProtocolList)
        {
            // DEBUG_V(String("------------"));
            // DEBUG_V(String("      Name: ") + String(x.ProtocolName));
            // DEBUG_V(String("ProtocolId: ") + String(x.ProtocolId));
            // DEBUG_V(String("  PortType: ") + String(x.RequiredPortType));
        }

        // DEBUG_V("Dump OM_OutputPortDefinitions");
        // for(auto & x : OM_OutputPortDefinitions)
        {
            // DEBUG_V(String("------------"));
            // DEBUG_V(String("    PortId: ") + String(x.PortId));
            // DEBUG_V(String("  PortType: ") + String(x.PortType));
            // DEBUG_V(String("  DeviceId: ") + String(x.DeviceId));
        }

        // DEBUG_V("load up the configuration from the saved file. This also starts the drivers");
        LoadConfig();

        // CreateNewConfig();

        // Preset the output memory
        ClearBuffer();
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
    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        // DEBUG_V (String("Create Section in Config file for the output channel: '") + ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputPortId() + "'");
        // create a record for this channel
        String sChannelId = String(((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputPortId());
        JsonObject ChannelConfigData = OutputMgrChannelsData[sChannelId];
        if (!ChannelConfigData)
        {
            // DEBUG_V (String ("add our channel section header. Chan: ") + sChannelId);
            ChannelConfigData = OutputMgrChannelsData[sChannelId].to<JsonObject> ();
        }

        // save the name as the selected channel type
        JsonWrite(ChannelConfigData, CN_type, int(((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputType()));

        String DriverTypeId = String(int(((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputType()));
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
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetDriverName(DriverName);
        // DEBUG_V (String ("DriverName: ") + DriverName);

        JsonWrite(ChannelConfigByTypeData, CN_type, DriverName);

        // DEBUG_V ();
        // PrettyPrint (ChannelConfigByTypeData, String ("jsonConfig"));
        // DEBUG_V ();

        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetConfig(ChannelConfigByTypeData);

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
    JsonConfigDoc.to<JsonObject>();
    // DEBUG_V ();

    // DEBUG_V("Create a new output config structure.");
    JsonObject JsonConfig = JsonConfigDoc[(char*)CN_output_config].to<JsonObject> ();
    // DEBUG_V ();

    JsonWrite(JsonConfig, CN_cfgver,      ConstConfig.CurrentConfigVersion);
    JsonWrite(JsonConfig, CN_MaxChannels, GetBufferSize());

    // DEBUG_V("Collect the all ports disabled config first");
    CreateJsonConfig (JsonConfig);

    for(auto & CurrentOutputPortDefinition : OM_OutputPortDefinitions)
    {
        // DEBUG_V(String("----------"));
        // DEBUG_V(String("  PortId: ") + String(CurrentOutputPortDefinition.PortId));
        // DEBUG_V(String("PortType: ") + String(CurrentOutputPortDefinition.PortType));
        // DEBUG_V(String("DeviceId: ") + String(CurrentOutputPortDefinition.DeviceId));

        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[CurrentOutputPortDefinition.PortId];
        // set the default GPIOs etc
        CurrentOutput.PortDefinition = CurrentOutputPortDefinition;

        // look at each possible output protocol
        for(auto & CurrentOutputProtocol : SupportedOutputProtocolList)
        {
            // DEBUG_V (String ("------------------------ "));
            // DEBUG_V (String ("instantiate output type: ") + String (CurrentOutputProtocol.ProtocolName));
            // DEBUG_V (String("              ProtocolId: ") + String(CurrentOutputProtocol.ProtocolId));
            // DEBUG_V (String("        RequiredPortType: ") + String(CurrentOutputProtocol.RequiredPortType));
            // DEBUG_V (String("                DriverId: ") + String(CurrentOutput.DriverId));

            if((CurrentOutputProtocol.RequiredPortType == CurrentOutputPortDefinition.PortType) ||
               (CurrentOutputProtocol.RequiredPortType == OM_PortType_t::OM_ANY))
            {
                // DEBUG_V("Port supports this protocol");
                InstantiateNewOutputChannel(CurrentOutput, CurrentOutputProtocol.ProtocolId, false);
            }
            else
            {
                // DEBUG_V("Port does NOT support this protocol");
                continue;
            }

            // DEBUG_V ("collect the config data for this output type");
            CreateJsonConfig (JsonConfig);
        } // for each possible output protocol

        if(JsonConfigDoc.overflowed())
        {
            logcon("Error: Config size is too small. Cannot create an output config with the current settings.")
            break;
        }

        // PrettyPrint(JsonConfig, "Intermediate in OutputMgr");

        // DEBUG_V (String("       Heap: ") + String(ESP.getFreeHeap()));
        // DEBUG_V (String(" overflowed: ") + String(JsonConfigDoc.overflowed()));
        // DEBUG_V (String("memoryUsage: ") + String(JsonConfigDoc.memoryUsage()));
        // PrettyPrint(JsonConfig, "Final Port in OutputMgr");

        // DEBUG_V ();
    } // for each defined output

    // DEBUG_V ("leave the outputs disabled");
    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        InstantiateNewOutputChannel(CurrentOutput, e_OutputProtocolType::OutputProtocol_Disabled);
    }// end for each interface

    // PrettyPrint(JsonConfig, "Complete OutputMgr");

    // DEBUG_V ("Outputs Are disabled");

    CreateJsonConfig (JsonConfig);

    // DEBUG_V (String ("       Heap: ") + String (ESP.getFreeHeap ()));
    // DEBUG_V (String (" overflowed: ") + String (JsonConfigDoc.overflowed()));
    // DEBUG_V (String ("memoryUsage: ") + String (JsonConfigDoc.memoryUsage()));

    SetConfig(JsonConfigDoc);

    // DEBUG_V (String (("--- WARNING: Creating a new Output Manager configuration Data set - Done ---")));

    BuildingNewConfig = false;

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
    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        // DEBUG_V ();
        JsonObject channelStatus = OutputStatus.add<JsonObject> ();
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetStatus(channelStatus);
        // DEBUG_V ();
    }

    // DEBUG_END;
} // GetStatus

//-----------------------------------------------------------------------------
void c_OutputMgr::ClearStatistics ()
{
    // DEBUG_START;

#if defined(ARDUINO_ARCH_ESP32)
    // PollCount = 0;
#endif // defined(ARDUINO_ARCH_ESP32)

    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        // DEBUG_V ();
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).ClearStatistics();
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
void c_OutputMgr::InstantiateNewOutputChannel(DriverInfo_t & CurrentOutput, e_OutputProtocolType NewOutputChannelType, bool StartDriver)
{
    // DEBUG_START;
    // BuildingNewConfig = false;
    // IsBooting = false;
    do // once
    {
        // DEBUG_V(String("CurrentOutput: 0x") + String(uint32_t(&CurrentOutput), HEX));
        // DEBUG_V(String("OutputDriverInUse: ") + String(CurrentOutput.OutputDriverInUse));
        // is there an existing driver?
        if(CurrentOutput.OutputDriverInUse)
        {
            // DEBUG_V (String("GetOutputType () '") + String(((c_OutputCommon&)CurrentOutput.OutputDriver).GetOutputType()) + String("'"));
            // DEBUG_V (String ("NewOutputChannelType '") + int(NewOutputChannelType) + "'");

            // DEBUG_V ("does the driver need to change?");
            if (((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputType() == NewOutputChannelType)
            {
                // DEBUG_V ("nothing to change");
                break;
            }

            String DriverName;
            ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetDriverName(DriverName);
            if (!IsBooting)
            {
                logcon(String(MN_12) + DriverName + MN_13 + String(CurrentOutput.DriverId));
            }

            ((c_OutputCommon&)(CurrentOutput.OutputDriver)).~c_OutputCommon();
            memset(CurrentOutput.OutputDriver, 0x0, sizeof(CurrentOutput.OutputDriver));
            CurrentOutput.OutputDriverInUse = false;
            // DEBUG_V ();
        } // end there is an existing driver

        // DEBUG_V ();

        // get the new data and UART info
        // CurrentOutput.GpioPin   = OutputPortIdToGpioAndPort[CurrentOutput.DriverId].GpioPin;
        // CurrentOutput.PortType  = OutputPortIdToGpioAndPort[CurrentOutput.DriverId].PortType;
        // CurrentOutput.PortId    = OutputPortIdToGpioAndPort[CurrentOutput.DriverId].PortId;

        // give the other tasks a chance to run
        delay(100);

        // DEBUG_V (String("DriverId: ") + String(CurrentOutput.DriverId));
        // DEBUG_V (String(" GpioPin: ") + String(CurrentOutput.PortId));
        // DEBUG_V (String("PortType: ") + String(CurrentOutput.PortType));
        // DEBUG_V (String("  PortId: ") + String(CurrentOutput.PortId));

        if(!SetPortDefnitionDefaults(CurrentOutput, NewOutputChannelType))
        {
            logcon(String(CN_stars) + F(" No valid port definition for port ") + String(CurrentOutput.DriverId + 1) + F(" of protocol type ") + String(NewOutputChannelType))
            AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
            break;
        }

        switch (NewOutputChannelType)
        {
            case e_OutputProtocolType::OutputProtocol_Disabled:
            {
                // DEBUG_V (String (F (" Starting Disabled for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }

            #if defined(SUPPORT_OutputProtocol_DMX)
            case e_OutputProtocolType::OutputProtocol_DMX:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_SERIAL)
                {
                    // DEBUG_V (String (F (" Starting DMX for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputSerial), CurrentOutput, e_OutputProtocolType::OutputProtocol_DMX);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();
                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_DMX + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
            #endif // defined(SUPPORT_OutputProtocol_DMX)

            #if defined(SUPPORT_OutputProtocol_GECE)
            case e_OutputProtocolType::OutputProtocol_GECE:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_SERIAL)
                {
                    // DEBUG_V (String (F (" Starting GECE for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputGECE), CurrentOutput, OutputProtocol_GECE);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_GECE + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // defined(SUPPORT_OutputProtocol_GECE)

            #ifdef SUPPORT_OutputProtocol_Serial
            case e_OutputProtocolType::OutputProtocol_Serial:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_SERIAL)
                {
                    // DEBUG_V (String (F (" Starting Generic Serial for port '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputSerial), CurrentOutput, OutputProtocol_Serial);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Generic Serial for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_Serial

            #ifdef SUPPORT_OutputProtocol_Renard
            case e_OutputProtocolType::OutputProtocol_Renard:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_SERIAL)
                {
                    // DEBUG_V (String (F (" Starting Renard for port '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputSerial), CurrentOutput, OutputProtocol_Renard);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Renard for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_Renard

            #ifdef SUPPORT_OutputProtocol_FireGod
            case e_OutputProtocolType::OutputProtocol_FireGod:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_SERIAL)
                {
                    // DEBUG_V (String (F (" Starting FireGod for port '")) + CurrentOutputChannel.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputSerial), CurrentOutput, OutputProtocol_FireGod);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start FireGod Serial for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_FireGod

            #ifdef SUPPORT_OutputProtocol_Relay
            case e_OutputProtocolType::OutputProtocol_Relay:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_RELAY)
                {
                    // DEBUG_V (String (F (" Starting RELAY for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NO_NAME(c_OutputRelay), CurrentOutput, OutputProtocol_Relay);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start RELAY for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_Relay

            #ifdef SUPPORT_OutputProtocol_Servo_PCA9685
            case e_OutputProtocolType::OutputProtocol_Servo_PCA9685:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_I2C)
                {
                    // DEBUG_V (String (F (" Starting Servo PCA9685 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NO_NAME(c_OutputServoPCA9685), CurrentOutput, OutputProtocol_Servo_PCA9685);
                    // DEBUG_V ();
                    break;
                }
                // DEBUG_V ();

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start Servo PCA9685 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
            #endif // SUPPORT_OutputProtocol_Servo_PCA9685

            #ifdef SUPPORT_OutputProtocol_WS2811
            case e_OutputProtocolType::OutputProtocol_WS2811:
            {
                // DEBUG_V ("OutputType_WS2811");
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SERIAL)
                {
                    // DEBUG_V ("UART");
                    // DEBUG_V (String (F (" Starting WS2811 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputWS2811), CurrentOutput, OutputProtocol_WS2811);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(F(" Cannot Start WS2811 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_WS2811

            #ifdef SUPPORT_OutputProtocol_UCS1903
            case e_OutputProtocolType::OutputProtocol_UCS1903:
            {
                // DEBUG_V ();
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SERIAL)
                {
                    // DEBUG_V (String (F (" Starting UCS1903 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputUCS1903), CurrentOutput, OutputProtocol_UCS1903);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_UCS1903 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
            #endif // def SUPPORT_OutputProtocol_UCS1903

            #ifdef SUPPORT_OutputProtocol_TM1814
            case e_OutputProtocolType::OutputProtocol_TM1814:
            {
                // DEBUG_V ();
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SERIAL)
                {
                    // DEBUG_V (String ((" Starting TM1814 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputTM1814), CurrentOutput, OutputProtocol_TM1814);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_TM1814 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
            #endif // def SUPPORT_OutputProtocol_TM1814

            #ifdef SUPPORT_OutputProtocol_GRINCH
            case e_OutputProtocolType::OutputProtocol_GRINCH:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SPI)
                {
                    // DEBUG_V (String ((" Starting GRINCH for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(c_OutputGrinchSpi, CurrentOutput, OutputProtocol_GRINCH);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + F("Grinch") + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_GRINCH

            #ifdef SUPPORT_OutputProtocol_WS2801
            case e_OutputProtocolType::OutputProtocol_WS2801:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SPI)
                {
                    // DEBUG_V (String ((" Starting WS2801 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(c_OutputWS2801Spi, CurrentOutput, OutputProtocol_WS2801);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_WS2801 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_WS2801

            #ifdef SUPPORT_OutputProtocol_APA102
            case e_OutputProtocolType::OutputProtocol_APA102:
            {
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SPI)
                {
                    // DEBUG_V (String ((" Starting APA102 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(c_OutputAPA102Spi, CurrentOutput, OutputProtocol_APA102);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_WS2801 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();

                break;
            }
            #endif // def SUPPORT_OutputProtocol_APA102

            #ifdef SUPPORT_OutputProtocol_GS8208
            case e_OutputProtocolType::OutputProtocol_GS8208:
            {
                // DEBUG_V ();
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SERIAL)
                {
                    // DEBUG_V (String ((" Starting GS8208 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputGS8208), CurrentOutput, OutputProtocol_GS8208);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_GS8208 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
            #endif // def SUPPORT_OutputProtocol_GS8208

            #ifdef SUPPORT_OutputProtocol_UCS8903
            case e_OutputProtocolType::OutputProtocol_UCS8903:
            {
                // DEBUG_V ();
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SERIAL)
                {
                    // DEBUG_V (String((" Starting UCS8903 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputUCS8903), CurrentOutput, OutputProtocol_UCS8903);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_UCS8903 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
            #endif // def SUPPORT_OutputProtocol_UCS8903

            #ifdef SUPPORT_OutputProtocol_TLS3001
            case e_OutputProtocolType::OutputProtocol_TLS3001:
            {
                // DEBUG_V ();
                if (CurrentOutput.PortDefinition.PortType == OM_PortType_t::OM_SERIAL)
                {
                    // DEBUG_V (String((" Starting TLS3001 for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                    AllocatePort(CLASS_TYPE_NAME(c_OutputTLS3001), CurrentOutput, OutputProtocol_TLS3001);
                    // DEBUG_V ();
                    break;
                }

                if (!BuildingNewConfig)
                {
                    logcon(CN_stars + String(MN_07) + CN_TLS3001 + MN_08 + CurrentOutput.DriverId + "'. " + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
            #endif // def SUPPORT_OutputProtocol_TLS3001

            default:
            {
                // DEBUG_V (String((" Starting Disabled as default output for port '")) + CurrentOutput.DriverId + "'. " + CN_stars);
                if (!IsBooting)
                {
                    logcon(CN_stars + String(MN_09) + String(NewOutputChannelType) + MN_10 + CurrentOutput.DriverId + MN_11 + CN_stars);
                }
                AllocatePort(c_OutputDisabled, CurrentOutput, OutputProtocol_Disabled);
                // DEBUG_V ();
                break;
            }
        } // end switch (NewChannelType)

        // DEBUG_V ();
        // DEBUG_V (String("pOutputChannelDriver: 0x") + String(uint32_t(((c_OutputCommon*)CurrentOutput.OutputDriver)),HEX));
        // DEBUG_V (String("heap: 0x") + String(uint32_t(ESP.getMaxFreeBlockSize()),HEX));

        String sDriverName;
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetDriverName(sDriverName);
        // DEBUG_V (String("Driver Name: ") + sDriverName);
        if (!IsBooting)
        {
            logcon("'" + sDriverName + MN_14 + String(CurrentOutput.DriverId));
        }
        if (StartDriver)
        {
            // DEBUG_V ("Starting Driver");
            ((c_OutputCommon&)(CurrentOutput.OutputDriver)).Begin();
        }

    } while (false);

    // DEBUG_END;

} // InstantiateNewOutputChannel

//-----------------------------------------------------------------------------
bool c_OutputMgr::SetPortDefnitionDefaults(DriverInfo_t & CurrentOutput, e_OutputProtocolType TargetProtocolType)
{
    // DEBUG_START;
    bool Response = false;

    do // once
    {
        // DEBUG_V(String("          DriverId: ") + String(CurrentOutput.DriverId));
        // DEBUG_V(String("TargetProtocolType: ") + String(TargetProtocolType));

        // default return
        OM_PortType_t TargetPortType = OM_PortType_t(-1);

        // DEBUG_V("search the Protocol definitions to find the needed 'port capabilities'");
        for(auto & CurrentOutputTypeXlateMapEntry : SupportedOutputProtocolList)
        {
            // DEBUG_V(String(" XlateProtocolType: ") + CurrentOutputTypeXlateMapEntry.ProtocolName);
            // DEBUG_V(String(" XlateProtocolType: ") + String(CurrentOutputTypeXlateMapEntry.ProtocolId));
            // DEBUG_V(String("     XlatePortType: ") + String(CurrentOutputTypeXlateMapEntry.RequiredPortType));
            if(TargetProtocolType == CurrentOutputTypeXlateMapEntry.ProtocolId)
            {
                // DEBUG_V("Found the target protocol");
                // DEBUG_V(String(" XlateProtocolType: ") + CurrentOutputTypeXlateMapEntry.ProtocolName);
                // DEBUG_V(String(" XlateProtocolType: ") + String(CurrentOutputTypeXlateMapEntry.ProtocolId));
                // DEBUG_V(String("     XlatePortType: ") + String(CurrentOutputTypeXlateMapEntry.RequiredPortType));
                TargetPortType = CurrentOutputTypeXlateMapEntry.RequiredPortType;
                break;
            }
        }

        // DEBUG_V(String("    TargetPortType: ") + String(TargetPortType));
        if(TargetPortType == OM_PortType_t(-1))
        {
            // DEBUG_V("Could not find the target protocol");
            break;
        }

        // DEBUG_V("search the port definitions to see if the target port supports the target protocol requirements");
        for(auto & CurrentPortDefinition : OM_OutputPortDefinitions)
        {
            // DEBUG_V(String("     CurrentPortId: ") + String(CurrentPortDefinition.PortId));
            // DEBUG_V(String("   CurrentPortType: ") + String(CurrentPortDefinition.PortType));
            // DEBUG_V(String("   CurrentPortGPIO: ") + String(CurrentPortDefinition.gpios.data));

            if (CurrentPortDefinition.PortId != CurrentOutput.DriverId)
            {
                // DEBUG_V("PortId is not a match");
                continue;
            }

            if ((CurrentPortDefinition.PortType != TargetPortType) && (TargetPortType != OM_PortType_t::OM_ANY))
            {
                // DEBUG_V("Port Type is not a match");
                continue;
            }

            // DEBUG_V(String("     CurrentPortId: ") + String(CurrentPortDefinition.PortId));
            // DEBUG_V(String("   CurrentPortType: ") + String(CurrentPortDefinition.PortType));
            // DEBUG_V(String("   CurrentPortGPIO: ") + String(CurrentPortDefinition.gpios.data));
            // DEBUG_V("Copy the base data");
            CurrentOutput.PortDefinition = CurrentPortDefinition;

            #ifdef ARDUINO_ARCH_ESP8266
            // fixed uart
            CurrentOutput.PortDefinition.DeviceId = 1;
            #else
            CurrentOutput.PortDefinition.DeviceId = CurrentOutput.DriverId;
            #endif // def ARDUINO_ARCH_ESP8266

            // DEBUG_V(String("              GPIO: ") + String(CurrentOutput.PortDefinition.gpios.data));
            // DEBUG_V(String("          DeviceId: ") + String(CurrentOutput.PortDefinition.DeviceId));

            Response = true;
            break;
        }

    } while(false);

    // DEBUG_V(String("          Response: ") + String(Response));
    // DEBUG_END;
    return Response;

} // SetPortDefnitionDefaults

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
            String Reason = MN_15;
            RequestReboot(Reason, 100000);
        }
    }

    ConfigInProgress = false;

    // DEBUG_END;

} // LoadConfig

//-----------------------------------------------------------------------------
bool c_OutputMgr::FindJsonChannelConfig (JsonDocument& jsonConfig,
                                         OM_PortId_t PortId,
                                         JsonObject& ChanConfig)
{
    // DEBUG_START;
    bool Response = false;
    // DEBUG_V ();
    // DEBUG_V(String("ChanId: ") + String(ChanId));
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

        uint8_t TempVersion = !ConstConfig.CurrentConfigVersion;
        setFromJSON (TempVersion, OutputChannelMgrData, CN_cfgver);
        // DEBUG_V (String ("TempVersion: ") + String (TempVersion));
        // DEBUG_V (String ("CurrentConfigVersion: ") + String (ConstConfig.CurrentConfigVersion));
        // PrettyPrint (OutputChannelMgrData, "Output Config");
        if (TempVersion != ConstConfig.CurrentConfigVersion)
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
        ChanConfig = OutputChannelArray[String(PortId)];
        if (!ChanConfig)
        {
            // if not, flag an error and stop processing
            logcon(String(MN_16) + PortId + MN_18);
            break;
        }
        // PrettyPrint(ChanConfig, "ProcessJson Channel Config");

        int32_t type = -1;
        setFromJSON(type, ChanConfig, CN_type);
        // do we need to go deeper into the config?
        if(-1 != type)
        {
            // DEBUG_V("only looking for the overall channel config");
            Response = true;
            break;
        }

        // go deeper and get the config for the specific channel type
        // PrettyPrint(ChanConfig, "ProcessJson Channel Config");

        // DEBUG_V ();
        JsonObject OutputChannelConfig = OutputChannelArray[String(PortId)];
        // PrettyPrint (OutputChannelConfig, "Output Channel Config");

        // do we have a configuration for the channel type?
        ChanConfig = OutputChannelConfig[String(type)];
        if (!ChanConfig)
        {
            // Not found
            logcon(String(MN_16) + PortId + MN_18);
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
        PauseOutputs(true);

        // for each output channel
        for (uint8_t index = 0; index < NumOutputPorts; ++index)
        {
            DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
            JsonObject OutputChannelConfig;
            if(!FindJsonChannelConfig (jsonConfig, CurrentOutput.DriverId, OutputChannelConfig))
            {
                // DEBUG_V(String("cant find config for port: ") + String(CurrentOutput.DriverId));
                continue;
            }
            // DEBUG_V ();

            // PrettyPrint(OutputChannelConfig, "ProcessJson port Config");

            // set a default value for channel type
            int OutputPortType = e_OutputProtocolType::OutputProtocol_Disabled;
            setFromJSON (OutputPortType, OutputChannelConfig, CN_type);
            // DEBUG_V ();

            // is it a valid / supported channel type
            if (e_OutputProtocolType(OutputPortType) >= e_OutputProtocolType::OutputProtocol_Last)
            {
                // DEBUG_V (String("OutputProtocol_Last: ") + String(e_OutputProtocolType::OutputProtocol_Last));
                // DEBUG_V (String("     OutputPortType: ") + String(OutputPortType));
                // if not, flag an error and move on to the next channel
                logcon(String(MN_19) + OutputPortType + MN_20 + CurrentOutput.DriverId + MN_03);
                InstantiateNewOutputChannel(CurrentOutput, e_OutputProtocolType::OutputProtocol_Disabled);
                continue;
            }
            // DEBUG_V ();

            // do we have a configuration for the channel type?
            JsonObject OutputChannelDriverConfig = OutputChannelConfig[String (OutputPortType)];
            if (!OutputChannelDriverConfig)
            {
                // if not, flag an error and stop processing
                logcon(String(MN_16) + CurrentOutput.DriverId + MN_18);
                InstantiateNewOutputChannel(CurrentOutput, e_OutputProtocolType::OutputProtocol_Disabled);
                continue;
            }
            // DEBUG_V ();
            // PrettyPrint(OutputChannelDriverConfig, "ProcessJson Channel Driver Config before driver create");
            // DEBUG_V ();

            // make sure the proper output type is running
            InstantiateNewOutputChannel(CurrentOutput, e_OutputProtocolType(OutputPortType));

            // DEBUG_V ();
            // PrettyPrint(OutputChannelDriverConfig, "ProcessJson Channel Driver Config");
            // DEBUG_V ();

            // send the config to the driver. At this level we have no idea what is in it
            ((c_OutputCommon&)(CurrentOutput.OutputDriver)).SetConfig(OutputChannelDriverConfig);
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

    PauseOutputs(false);

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

    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        // DEBUG_V();
        if(e_OutputProtocolType::OutputProtocol_Disabled != ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputType())
        {
            // DEBUG_V (String("Output GPIO: ") + String(((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputGpio()));

            NeedToTurnOffConsole |= ((c_OutputCommon&)(CurrentOutput.OutputDriver)).ValidateGpio(ConsoleTxGpio, ConsoleRxGpio);
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
        for (uint8_t index = 0; index < NumOutputPorts; ++index)
        {
            DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
            // //DEBUG_V("Poll a channel");
            ((c_OutputCommon&)(CurrentOutput.OutputDriver)).Poll ();
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

    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        // String DriverName;
        // OutputChannel.pOutputChannelDriver->GetDriverName(DriverName);
        // DEBUG_V(String("Name: ") + DriverName);
        // DEBUG_V(String("PortId: ") + String(OutputChannel.pOutputChannelDriver->GetOutputPortId()) );

        CurrentOutput.OutputBufferStartingOffset = OutputBufferOffset;
        CurrentOutput.OutputChannelStartingOffset = OutputChannelOffset;
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).SetOutputBufferAddress(pOutputBuffer + OutputBufferOffset);

        uint32_t OutputBufferDataBytesNeeded        = ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetNumOutputBufferBytesNeeded ();
        uint32_t VirtualOutputBufferDataBytesNeeded = ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetNumOutputBufferChannelsServiced ();

        uint32_t AvailableChannels = GetBufferSize() - OutputBufferOffset;

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
        CurrentOutput.OutputBufferDataSize  = OutputBufferDataBytesNeeded;
        CurrentOutput.OutputBufferEndOffset = OutputBufferOffset - 1;
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).SetOutputBufferSize (OutputBufferDataBytesNeeded);

        OutputChannelOffset += VirtualOutputBufferDataBytesNeeded;
        CurrentOutput.OutputChannelSize      = VirtualOutputBufferDataBytesNeeded;
        CurrentOutput.OutputChannelEndOffset = OutputChannelOffset;

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
#ifdef SUPPORT_OutputProtocol_Relay
        for (uint8_t index = 0; index < NumOutputPorts; ++index)
        {
            DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
            // DEBUG_V(String("DriverId: ") + String(CurrentOutput.DriverId));
            // DEBUG_V(String("PortType: ") + String(((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputType()));
            if(e_OutputProtocolType::OutputProtocol_Relay == ((c_OutputCommon&)(CurrentOutput.OutputDriver)).GetOutputType())
            {
                ((c_OutputRelay*)(CurrentOutput.OutputDriver))->RelayUpdate(RelayId, NewValue, Response);
                break;
            }
        }
#endif // def SUPPORT_OutputProtocol_Relay

    } while(false);

    // DEBUG_END;
} // RelayUpdate

//-----------------------------------------------------------------------------
void c_OutputMgr::PauseOutputs(bool PauseTheOutput)
{
    // DEBUG_START;
    // DEBUG_V(String("PauseTheOutput: ") + String(PauseTheOutput));

    OutputIsPaused = PauseTheOutput;

    for (uint8_t index = 0; index < NumOutputPorts; ++index)
    {
        DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
        ((c_OutputCommon&)(CurrentOutput.OutputDriver)).PauseOutput(PauseTheOutput);
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
        for (uint8_t index = 0; index < NumOutputPorts; ++index)
        {
            DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
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
                ((c_OutputCommon&)(CurrentOutput.OutputDriver)).WriteChannelData(RelativeStartChannelId, ChannelsToSet, pSourceData);
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
        for (uint8_t index = 0; index < NumOutputPorts; ++index)
        {
            DriverInfo_t & CurrentOutput = pOutputChannelDrivers[index];
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
            ((c_OutputCommon&)(CurrentOutput.OutputDriver)).ReadChannelData(RelativeStartChannelId, ChannelsToSet, pTargetData);
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
