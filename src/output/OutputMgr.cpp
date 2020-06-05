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
///< Start up the driver and put it into a safe mode
c_OutputMgr::c_OutputMgr ()
{
    ConfigFileName = String ("/") + String (OM_SECTION_NAME) + ".json";

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

    // DEBUG_V ("");

    // open a persistent file handle
    ConfigFile = SPIFFS.open (ConfigFileName, "r+");

    // load up the configuration from the saved file. This also starts the drivers
    LoadConfig ();

    // DEBUG_V ("");
    // todo - remove this. For debugging without UI
    InstantiateNewOutputChannel (e_OutputChannelIds::OutputChannelId_1,
                                 e_OutputType::OutputType_WS2811);

    // DEBUG_END;

} // begin

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
    if (!FileIO::loadConfig (ConfigFileName.c_str(), ConfigFile, [this](DynamicJsonDocument & JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject JsonConfig = JsonConfigDoc.as<JsonObject> ();
            // DEBUG_V ("");
            this->DeserializeConfig (JsonConfig);
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
    The running config is made from a composite of running and not instantiated 
    objects. To create a complete config we need to start each output type on 
    each output channel and collect the configuration at each stage.
*/
void c_OutputMgr::CreateNewConfig ()
{
    // DEBUG_START;
    LOG_PORT.println (F ("--- WARNING: Creating a new Output Manager configuration Data set - Start ---"));

    // create a place to save the config
    DynamicJsonDocument JsonConfigDoc (4096);
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
        SerializeConfig (JsonConfig);

        // DEBUG_V ("");
#ifdef foo
        String sJsonConfig;
        serializeJsonPretty (JsonConfigDoc, sJsonConfig);

        LOG_PORT.println ("cccc PrettyJsonConfig cccc");
        LOG_PORT.println (sJsonConfig);
        LOG_PORT.println ("cccc PrettyJsonConfig cccc");
#endif // def foo

    } // end for each output type

    // DEBUG_V ("leave the outputs disabled");
    int ChannelIndex = 0;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType::OutputType_Disabled);
    }// end for each interface

    // DEBUG_V ("");

    String sJsonConfig;
#ifdef foo
    serializeJsonPretty (JsonConfigDoc, sJsonConfig);

    LOG_PORT.println ("nnnn PrettyJsonConfig nnnn");
    LOG_PORT.println (sJsonConfig);
    LOG_PORT.println ("nnnn PrettyJsonConfig nnnn");
    sJsonConfig = "";
#endif // def foo

    // DEBUG_V ("save the config data to NVRAM");
    serializeJson (JsonConfigDoc, sJsonConfig);
    if (FileIO::saveConfig (ConfigFileName, sJsonConfig))
    {
        LOG_PORT.println (F ("**** Saved Output Manager Config File. ****"));
    } // end we got a config and it was good
    else
    {
        LOG_PORT.println (F ("EEEE Error Saving Output Manager Config File. EEEE"));
    }

    LOG_PORT.println (F ("--- WARNING: Creating a new Output Manager configuration Data set - Done ---"));
    // DEBUG_END;

} // CreateNewConfig

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

    // try to load the config file
    if (!FileIO::loadConfig (ConfigFileName, ConfigFile, [this](DynamicJsonDocument& JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject JsonConfig = JsonConfigDoc.as<JsonObject> ();

            // add the OM header
            JsonObject OutputMgrData;
            if (true == JsonConfig.containsKey (OM_SECTION_NAME))
            {
                // DEBUG_V ("");
                OutputMgrData = JsonConfig[OM_SECTION_NAME];
            }
            else
            {
                // add our section header
                // DEBUG_V ("");
                OutputMgrData = JsonConfig.createNestedObject (OM_SECTION_NAME);
            }

            this->SerializeConfig (OutputMgrData);

            String sJsonConfig = "";
#ifdef foo
            serializeJsonPretty (JsonConfigDoc, sJsonConfig);

            LOG_PORT.println ("ssss PrettyJsonConfig ssss");
            LOG_PORT.println (sJsonConfig);
            LOG_PORT.println ("ssss PrettyJsonConfig ssss");
#endif // def foo
            // this forces a write to flash
            ConfigFile.close ();

            serializeJson (JsonConfigDoc, sJsonConfig);
#ifdef foo
            LOG_PORT.println ("ssss JsonConfig ssss");
            LOG_PORT.println (sJsonConfig);
            LOG_PORT.println ("ssss JsonConfig ssss");
#endif // def foo
            if (FileIO::saveConfig (ConfigFileName, sJsonConfig))
            {
                LOG_PORT.println (F ("**** Saved Output Manager Config File. ****"));
            } // end we got a config and it was good
            else
            {
                LOG_PORT.println (F ("EEEE Error Saving Output Manager Config File. EEEE"));
            }

            ConfigFile = SPIFFS.open (ConfigFileName.c_str (), "r+");
        }))
    {
        // could not read the file so we need to create a new image.
        CreateNewConfig ();
    }

    // DEBUG_END;

} // SaveConfig

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
bool c_OutputMgr::DeserializeConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    boolean retval = false;
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
        for (uint32_t ChannelIndex = uint32_t(OutputChannelId_Start);
            ChannelIndex < uint32_t(OutputChannelId_End);
            ChannelIndex++)
        {
            // get access to the channel config
            if (false == OutputChannelArray.containsKey (String(ChannelIndex).c_str()))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String(F ("No Output Settings Found for Channel '")) + ChannelIndex + String(F("'. Using Defaults")));
                break;
            }
            JsonObject OutputChannelConfig = OutputChannelArray[String (ChannelIndex).c_str ()];
            // DEBUG_V ("");

            // set a default value for channel type
            uint32_t ChannelType = uint32_t (OutputType_End);
            FileIO::setFromJSON (ChannelType, OutputChannelConfig[OM_CHANNEL_TYPE_NAME]);
            // DEBUG_V ("");

            // is it a valid / supported channel type
            if ((ChannelType < uint32_t(OutputType_Start)) || (ChannelType >= uint32_t(OutputType_End)))
            {
                // if not, flag an error and move on to the next channel
                LOG_PORT.println (String (F ("Invalid Channel Type in config '")) + ChannelType + String (F ("'. Specified for channel '")) + ChannelIndex + "'. Disabling channel");
                InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType::OutputType_Disabled);
                continue;
            }
            // DEBUG_V ("");

            // do we have a configuration for the channel type?
            if (false == OutputChannelConfig.containsKey (String(ChannelType)))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String (F ("No Output Settings Found for Channel '")) + ChannelIndex + String (F ("'. Using Defaults")));
                InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType::OutputType_Disabled);
                continue;
            }

            JsonObject OutputChannelDriverConfig = OutputChannelConfig[String(ChannelType)];
            // DEBUG_V ("");

            // make sure the proper output type is running
            InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType (ChannelType));
            // DEBUG_V ("");

            // send the config to the driver. At this level we have no idea what is in it
            retval |= pOutputChannelDrivers[ChannelIndex]->SetConfig (OutputChannelDriverConfig);

        } // end for each channel
        
        // all went well
        retval = true;

    } while (false);

    // did we get a valid config?
    if (false == retval)
    {
        // save the current config since it is the best we have.
        // DEBUG_V ("");
        CreateNewConfig ();
    }

    // DEBUG_END;
    return retval;

} // deserializeConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::SerializeConfig (JsonObject & jsonConfig)
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
        // DEBUG_V (String("Create Section in Config file for the output channel: '") + CurrentChannel->GetOuputChannelId() + "'");
        // create a record for this channel
        JsonObject ChannelConfigData;
        String sChannelId = String (CurrentChannel->GetOuputChannelId ());
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
        ChannelConfigData[OM_CHANNEL_TYPE_NAME] = int(CurrentChannel->GetOutputType ());

        String DriverTypeId = String(int(CurrentChannel->GetOutputType ()));
        JsonObject ChannelConfigByTypeData;
        if (true == ChannelConfigData.containsKey (String(DriverTypeId)))
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
        CurrentChannel->GetConfig (ChannelConfigByTypeData);
        // DEBUG_V ("");
    }

    // smile. Your done
    // DEBUG_END;
} // SerializeConfig

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
/* Returns the current configuration for the output channels
*
*   needs
*       Reference to string into which to place the configuration
*       presentation style
*   returns
*       string populated with currently available configuration
*/
void c_OutputMgr::GetConfig (JsonObject & jsonConfigResponse)
{
    // DEBUG_START;

    // try to load and process the config file
    if (!FileIO::loadConfig (ConfigFileName.c_str(), ConfigFile, [this, jsonConfigResponse](DynamicJsonDocument & JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject jsonSrc = JsonConfigDoc.as<JsonObject> ();
            // DEBUG_V ("");
            this->merge (jsonConfigResponse, jsonSrc);
            // DEBUG_V ("");
        }))
    {
        // file read error
    }

    // DEBUG_END;
} // GetConfig

//-----------------------------------------------------------------------------
void c_OutputMgr::merge (JsonVariant dst, JsonVariantConst src)
{
    // DEBUG_START;
    if (src.is<JsonObject> ())
    {
        for (auto kvp : src.as<JsonObject> ())
        {
            merge (dst.getOrAddMember (kvp.key ()), kvp.value ());
        }
    }
    else
    {
        dst.set (src);
    }
    // DEBUG_END;
} // merge

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
    boolean retval = false;
    if (jsonConfig.containsKey (OM_SECTION_NAME))
    {
        DeserializeConfig (jsonConfig);
    }
    else
    {
        LOG_PORT.println (F("EEEE No Output Manager settings found. EEEE"));
    }
    // DEBUG_END;
} // SetConfig

//-----------------------------------------------------------------------------
///< Called from loop(), renders output data
void c_OutputMgr::Render()
{
    // DEBUG_START;
    for (c_OutputCommon * pOutputChannel : pOutputChannelDrivers)
    {
        pOutputChannel->Render ();
    }
    // DEBUG_END;
} // render

// create a global instance of the output channel factory
c_OutputMgr OutputMgr;

