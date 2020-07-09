/*
* OutputMgr.cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2019 Shelby Merrick
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

#include <Arduino.h>
#include "../ESPixelStick.h"
#include "../FileIO.h"

//-----------------------------------------------------------------------------
// bring in driver definitions
#include "OutputDisabled.hpp"
#include "OutputGECE.hpp"
#include "OutputSerial.hpp"
#include "OutputWS2811.hpp"
// needs to be last
#include "OutputMgr.hpp"

//-----------------------------------------------------------------------------
// Local Data definitions
//-----------------------------------------------------------------------------
typedef struct OutputTypeXlateMap_t
{
    c_OutputMgr::e_OutputType id;
    String name;
};

OutputTypeXlateMap_t OutputTypeXlateMap[c_OutputMgr::e_OutputType::OutputType_End] =
{
    {c_OutputMgr::e_OutputType::OutputType_WS2811,   "WS2811"   },
    {c_OutputMgr::e_OutputType::OutputType_GECE,     "GECE"     },
    {c_OutputMgr::e_OutputType::OutputType_Serial,   "Serial"   },
    {c_OutputMgr::e_OutputType::OutputType_Renard,   "Renard"   },
    {c_OutputMgr::e_OutputType::OutputType_DMX,      "DMX"      },
#ifdef ARDUINO_ARCH_ESP32
    {c_OutputMgr::e_OutputType::OutputType_SPI,      "SPI"      },
#endif // def ARDUINO_ARCH_ESP32
    {c_OutputMgr::e_OutputType::OutputType_Disabled, "Disabled" }
};

//-----------------------------------------------------------------------------
typedef struct OutputChannelIdToGpioAndPortEntry_t
{
    gpio_num_t dataPin;
    uart_port_t UartId;
};

//-----------------------------------------------------------------------------
OutputChannelIdToGpioAndPortEntry_t OutputChannelIdToGpioAndPort[] =
{
    {gpio_num_t::GPIO_NUM_2,  uart_port_t::UART_NUM_1},
    {gpio_num_t::GPIO_NUM_13, uart_port_t::UART_NUM_2},
    {gpio_num_t::GPIO_NUM_10, uart_port_t (-1)},
};

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_OutputMgr::c_OutputMgr ()
{
    ConfigFileName = String (F ("/")) + String (OM_SECTION_NAME) + F(".json");

    // this gets called pre-setup so there is nothing we can do here.
    int pOutputChannelDriversIndex = 0;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        pOutputChannelDrivers[pOutputChannelDriversIndex++] = nullptr;
    }

} // c_OutputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_OutputMgr::~c_OutputMgr()
{
    // DEBUG_START;

    // delete pOutputInstances;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        // the drivers will put the hardware in a safe state
        delete CurrentOutput;
    }
    // DEBUG_END;

} // ~c_OutputMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_OutputMgr::Begin ()
{
    // DEBUG_START;

    // prevent recalls
    if (true == HasBeenInitialized) { return; }
    HasBeenInitialized = true;

    // make sure the pointers are set up properly
    int ChannelIndex = 0;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        InstantiateNewOutputChannel (e_OutputChannelIds(ChannelIndex++),
                                     e_OutputType::OutputType_Disabled);
        // DEBUG_V ("");
    }

    // load up the configuration from the saved file. This also starts the drivers
    LoadConfig ();

    // DEBUG_V ("");
    // todo - remove this. For debugging without UI
/*    InstantiateNewOutputChannel (e_OutputChannelIds::OutputChannelId_1,
                                 e_OutputType::OutputType_WS2811);
*/
    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_OutputMgr::CreateJsonConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    // add OM config parameters
    // DEBUG_V ("");

    // add the channels header
    JsonObject OutputMgrChannelsData;
    if (true == jsonConfig.containsKey (OM_CHANNEL_SECTION_NAME))
    {
        // DEBUG_V ("");
        OutputMgrChannelsData = jsonConfig[OM_CHANNEL_SECTION_NAME];
    }
    else
    {
        // add our section header
        // DEBUG_V ("");
        OutputMgrChannelsData = jsonConfig.createNestedObject (OM_CHANNEL_SECTION_NAME);
    }

    // add the channel configurations
    // DEBUG_V ("For Each Output Channel");
    for (auto CurrentChannel : pOutputChannelDrivers)
    {
        // DEBUG_V (String("Create Section in Config file for the output channel: '") + CurrentChannel->GetOutputChannelId() + "'");
        // create a record for this channel
        JsonObject ChannelConfigData;
        String sChannelId = String (CurrentChannel->GetOutputChannelId ());
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
        ChannelConfigData[OM_CHANNEL_TYPE_NAME] = int (CurrentChannel->GetOutputType ());

        String DriverTypeId = String (int (CurrentChannel->GetOutputType ()));
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

        // ask the channel to add its data to the record
        // DEBUG_V ("Add the output channel configuration for type: " + DriverTypeId);

        // Populate the driver name
        String DriverName = ""; CurrentChannel->GetDriverName (DriverName);
        ChannelConfigByTypeData[F ("type")] = DriverName;

        CurrentChannel->GetConfig (ChannelConfigByTypeData);
        // DEBUG_V ("");
    }

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
    LOG_PORT.println (F ("--- WARNING: Creating a new Output Manager configuration Data set - Start ---"));

    // create a place to save the config
    DynamicJsonDocument JsonConfigDoc (2048);
    JsonObject JsonConfig = JsonConfigDoc.createNestedObject (OM_SECTION_NAME);

    // DEBUG_V ("for each output type");
    for (int outputTypeId = int (OutputType_Start); outputTypeId < int (OutputType_End); ++outputTypeId)
    {
        // DEBUG_V ("for each interface");
        int ChannelIndex = 0;
        for (auto CurrentOutput : pOutputChannelDrivers)
        {
            // DEBUG_V (String("instantiate the output type: ") + outputTypeId);
            InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex++), e_OutputType (outputTypeId));
        }// end for each interface

        // DEBUG_V ("collect the config data");
        CreateJsonConfig (JsonConfig);

        // DEBUG_V ("");

    } // end for each output type

    // DEBUG_V ("leave the outputs disabled");
    int ChannelIndex = 0;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType::OutputType_Disabled);
    }// end for each interface
    // DEBUG_V ("");

    ConfigData.clear ();

    // DEBUG_V ("save the config data to NVRAM");
    serializeJson (JsonConfigDoc, ConfigData);
    ConfigSaveNeeded = false;
    SaveConfig ();

    JsonConfigDoc.garbageCollect ();

    LOG_PORT.println (F ("--- WARNING: Creating a new Output Manager configuration Data set - Done ---"));
    // DEBUG_END;

} // CreateNewConfig

//-----------------------------------------------------------------------------
///< Get the pointer to the input buffer for the desired output channel
uint8_t * c_OutputMgr::GetBufferAddress (e_OutputChannelIds ChannelId)
{
    return pOutputChannelDrivers[ChannelId]->GetBufferAddress ();

} // GetBufferAddress

//-----------------------------------------------------------------------------
///< Get the length of the input buffer for the desired output channel
uint16_t c_OutputMgr::GetBufferSize (e_OutputChannelIds ChannelId)
{
    return pOutputChannelDrivers[ChannelId]->GetBufferSize ();

} // GetBufferSize

//-----------------------------------------------------------------------------
void c_OutputMgr::GetConfig (char * Response )
{
    // DEBUG_START;

    strcat (Response, ConfigData.c_str ());

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::GetOptions (JsonObject & jsonOptions)
{
    // DEBUG_START;
    JsonArray SelectedOptionList = jsonOptions.createNestedArray (F("selectedoptionlist"));

    // build a list of the current available channels and their output type
    for (c_OutputCommon* currentOutput : pOutputChannelDrivers)
    {
        JsonObject selectedoption = SelectedOptionList.createNestedObject ();
        selectedoption[F ("id")]             = currentOutput->GetOutputChannelId ();
        selectedoption[F ("selectedoption")] = currentOutput->GetOutputType ();
    }

    // DEBUG_V ("");

    JsonArray jsonOptionsArray = jsonOptions.createNestedArray (F("list"));
    // DEBUG_V ("");

    // Build a list of Valid options for this device
    for (OutputTypeXlateMap_t currentOutputType : OutputTypeXlateMap)
    {
        // DEBUG_V ("");
        JsonObject jsonOptionsArrayEntry  = jsonOptionsArray.createNestedObject ();
        jsonOptionsArrayEntry[F ("id")]   = int(currentOutputType.id);
        jsonOptionsArrayEntry[F ("name")] = currentOutputType.name;

        // DEBUG_V ("");
    } // end for each output type

    // DEBUG_END;
} // GetOptions

//-----------------------------------------------------------------------------
void c_OutputMgr::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonArray OutputStatus = jsonStatus.createNestedArray (F("output"));
    uint channelIndex = 0;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        JsonObject channelStatus = OutputStatus.createNestedObject ();
        CurrentOutput->GetStatus (channelStatus);
        channelIndex++;
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
void c_OutputMgr::InstantiateNewOutputChannel (e_OutputChannelIds ChannelIndex, e_OutputType NewOutputChannelType)
{
    // DEBUG_START;

    do // once
    {
        // is there an existing driver?
        if (nullptr != pOutputChannelDrivers[ChannelIndex])
        {
            // int temp = pOutputChannelDrivers[ChannelIndex]->GetOutputType ();
            // DEBUG_V (String("pOutputChannelDrivers[ChannelIndex]->GetOutputType () '") + temp + String("'"));
            // DEBUG_V (String("NewOutputChannelType '") + int(NewOutputChannelType) + "'");

            // DEBUG_V ("does the driver need to change?");
            if (pOutputChannelDrivers[ChannelIndex]->GetOutputType () == NewOutputChannelType)
            {
                // DEBUG_V ("nothing to change");
                break;
            }

            // DEBUG_V ("shut down the existing driver");
            delete pOutputChannelDrivers[ChannelIndex];
            pOutputChannelDrivers[ChannelIndex] = nullptr;
            // DEBUG_V ("");
        } // end there is an existing driver

        // DEBUG_V ("");

        // get the new data and UART info
        gpio_num_t dataPin = OutputChannelIdToGpioAndPort[ChannelIndex].dataPin;
        uart_port_t UartId = OutputChannelIdToGpioAndPort[ChannelIndex].UartId;

        switch (NewOutputChannelType)
        {
        case e_OutputType::OutputType_Disabled:
        {
            // LOG_PORT.println (String (F ("************** Disabled output type for channel '")) + ChannelIndex + "'. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
            // DEBUG_V ("");
            break;
        }

        case e_OutputType::OutputType_DMX:
        {
            if (-1 == UartId)
            {
                LOG_PORT.println (String (F ("************** Cannot Start DMX for channel '")) + ChannelIndex + "'. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }

            // LOG_PORT.println (String (F ("************** Starting DMX for channel '")) + ChannelIndex + "'. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputSerial (ChannelIndex, dataPin, UartId, OutputType_DMX);
            // DEBUG_V ("");
            break;
        }

        case e_OutputType::OutputType_GECE:
        {
            if (-1 == UartId)
            {
                LOG_PORT.println (String (F ("************** Cannot Start GECE for channel '")) + ChannelIndex + "'. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
            // LOG_PORT.println (String (F ("************** Starting GECE for channel '")) + ChannelIndex + "'. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputGECE (ChannelIndex, dataPin, UartId, OutputType_GECE);
            // DEBUG_V ("");
            break;
        }

        case e_OutputType::OutputType_Serial:
        {
            if (-1 == UartId)
            {
                LOG_PORT.println (String (F ("************** Cannot Start Generic Serial for channel '")) + ChannelIndex + "'. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
            // LOG_PORT.println (String (F ("************** Starting Generic Serial for channel '")) + ChannelIndex + "'. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputSerial (ChannelIndex, dataPin, UartId, OutputType_Serial);
            // DEBUG_V ("");
            break;
        }

        case e_OutputType::OutputType_Renard:
        {
            if (-1 == UartId)
            {
                LOG_PORT.println (String (F ("************** Cannot Start Renard for channel '")) + ChannelIndex + "'. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
            // LOG_PORT.println (String (F ("************** Starting Renard for channel '")) + ChannelIndex + "'. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputSerial (ChannelIndex, dataPin, UartId, OutputType_Renard);
            // DEBUG_V ("");
            break;
        }

#ifdef ARDUINO_ARCH_ESP32
        if (-1 != UartId)
        {
            LOG_PORT.println (String (F ("************** Cannot Start SPI for channel '")) + ChannelIndex + "'. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
            // DEBUG_V ("");
            break;
        }
        case e_OutputType::OutputType_SPI:
        {
            // LOG_PORT.println (String (F ("************** Starting SPI for channel '")) + ChannelIndex + "'. **************");
            LOG_PORT.println (String (F ("************** SPI Not supported Yet. Using disabled. **************")));
            pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
            // DEBUG_V ("");
            break;
        }
#endif // def ARDUINO_ARCH_ESP8266

        case e_OutputType::OutputType_WS2811:
        {
            if (-1 == UartId)
            {
                LOG_PORT.println (String (F ("************** Cannot Start WS2811 for channel '")) + ChannelIndex + "'. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
                // DEBUG_V ("");
                break;
            }
            // LOG_PORT.println (String (F ("************** Starting WS2811 for channel '")) + ChannelIndex + "'. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex, dataPin, UartId, OutputType_WS2811);
            // DEBUG_V ("");
            break;
        }

        default:
        {
            LOG_PORT.println (String (F ("************** Unknown output type for channel '")) + ChannelIndex + "'. Using disabled. **************");
            pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId, OutputType_Disabled);
            // DEBUG_V ("");
            break;
        }
        } // end switch (NewChannelType)

        // DEBUG_V ("");
        pOutputChannelDrivers[ChannelIndex]->Begin ();

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

    // try to load and process the config file
    if (!FileIO::loadConfig (ConfigFileName, [this](DynamicJsonDocument & JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject JsonConfig = JsonConfigDoc.as<JsonObject> ();
            // DEBUG_V ("");
            this->ProcessJsonConfig (JsonConfig);
            // DEBUG_V ("");
        }))
    {
        LOG_PORT.println (F ("EEEE Error loading Output Manager Config File. EEEE"));

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
    boolean Response = false;

    // save a copy of the config
    ConfigData.clear ();
    serializeJson (jsonConfig, ConfigData);
    // DEBUG_V ("");

    do // once
    {
        if (false == jsonConfig.containsKey (OM_SECTION_NAME))
        {
            LOG_PORT.println (F ("No Output Interface Settings Found. Using Defaults"));
            break;
        }
        JsonObject OutputChannelMgrData = jsonConfig[OM_SECTION_NAME];
        // DEBUG_V ("");

        // extract my own config data here

        // do we have a channel configuration array?
        if (false == OutputChannelMgrData.containsKey (OM_CHANNEL_SECTION_NAME))
        {
            // if not, flag an error and stop processing
            LOG_PORT.println (F ("No Output Channel Settings Found. Using Defaults"));
            break;
        }
        JsonObject OutputChannelArray = OutputChannelMgrData[OM_CHANNEL_SECTION_NAME];
        // DEBUG_V ("");

        // for each output channel
        for (uint32_t ChannelIndex = uint32_t (OutputChannelId_Start);
            ChannelIndex < uint32_t (OutputChannelId_End);
            ChannelIndex++)
        {
            // get access to the channel config
            if (false == OutputChannelArray.containsKey (String (ChannelIndex).c_str ()))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String (F ("No Output Settings Found for Channel '")) + ChannelIndex + String (F ("'. Using Defaults")));
                break;
            }
            JsonObject OutputChannelConfig = OutputChannelArray[String (ChannelIndex).c_str ()];
            // DEBUG_V ("");

            // set a default value for channel type
            uint32_t ChannelType = uint32_t (OutputType_End);
            FileIO::setFromJSON (ChannelType, OutputChannelConfig[OM_CHANNEL_TYPE_NAME]);
            // DEBUG_V ("");

            // is it a valid / supported channel type
            if ((ChannelType < uint32_t (OutputType_Start)) || (ChannelType >= uint32_t (OutputType_End)))
            {
                // if not, flag an error and move on to the next channel
                LOG_PORT.println (String (F ("Invalid Channel Type in config '")) + ChannelType + String (F ("'. Specified for channel '")) + ChannelIndex + "'. Disabling channel");
                InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType::OutputType_Disabled);
                continue;
            }
            // DEBUG_V ("");

            // do we have a configuration for the channel type?
            if (false == OutputChannelConfig.containsKey (String (ChannelType)))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String (F ("No Output Settings Found for Channel '")) + ChannelIndex + String (F ("'. Using Defaults")));
                InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType::OutputType_Disabled);
                continue;
            }

            JsonObject OutputChannelDriverConfig = OutputChannelConfig[String (ChannelType)];
            // DEBUG_V ("");

            // make sure the proper output type is running
            InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType (ChannelType));
            // DEBUG_V ("");

            // send the config to the driver. At this level we have no idea what is in it
            Response |= pOutputChannelDrivers[ChannelIndex]->SetConfig (OutputChannelDriverConfig);

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

    // DEBUG_END;
    return Response;

} // ProcessJsonConfig

//-----------------------------------------------------------------------------
/*
    This is a bit tricky. The running config is only a portion of the total 
    configuration. We need to get the existing saved configuration and add the
    current configuration to it.

*   Save the current configuration to NVRAM
*
*   needs
*       Nothing
*   returns
*       Nothing
*/
void c_OutputMgr::SaveConfig ()
{
    // DEBUG_START;
    if (FileIO::SaveConfig (ConfigFileName, ConfigData))
    {
        LOG_PORT.println (F ("**** Saved Output Manager Config File. ****"));
    } // end we got a config and it was good
    else
    {
        LOG_PORT.println (F ("EEEE Error Saving Output Manager Config File. EEEE"));
    }

    // DEBUG_END;

} // SaveConfig

//-----------------------------------------------------------------------------
/* Sets the configuration for the current active ports
*
*   Needs
*       Reference to the incoming JSON configuration doc
*   Returns
*       true - No Errors found
*       false - Had an issue and it was reported to the log interface
*/
bool c_OutputMgr::SetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    boolean Response = false;
    if (jsonConfig.containsKey (OM_SECTION_NAME))
    {
        // DEBUG_V ("");
        Response = ProcessJsonConfig (jsonConfig);

        // schedule a future save to the file system
        ConfigSaveNeeded = true;
    }
    else
    {
        LOG_PORT.println (F("EEEE No Output Manager settings found. EEEE"));
    }
    // DEBUG_END;
    return Response;
} // SetConfig

//-----------------------------------------------------------------------------
///< Called from loop(), renders output data
void c_OutputMgr::Render()
{
    // do we need to save the current config?
    if (true == ConfigSaveNeeded)
    {
        ConfigSaveNeeded = false;
        SaveConfig ();
    } // done need to save the current config

    // DEBUG_START;
    for (c_OutputCommon * pOutputChannel : pOutputChannelDrivers)
    {
        pOutputChannel->Render ();
    }
    // DEBUG_END;
} // render

// create a global instance of the output channel factory
c_OutputMgr OutputMgr;
