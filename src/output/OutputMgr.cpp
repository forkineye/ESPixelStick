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

// bring in driver definitions
#include "OutputWS2811.hpp"
#include "OutputGECE.hpp"
#include "OutputDisabled.hpp"
// needs to be last
#include "OutputMgr.hpp"


typedef struct OutputChannelIdToGpioAndPortEntry_t
{
    gpio_num_t dataPin;
    uart_port_t UartId;
};

OutputChannelIdToGpioAndPortEntry_t OutputChannelIdToGpioAndPort[] =
{
    {gpio_num_t::GPIO_NUM_2,  uart_port_t::UART_NUM_1},
    {gpio_num_t::GPIO_NUM_13, uart_port_t::UART_NUM_2},
    {gpio_num_t::GPIO_NUM_10, uart_port_t (-1)},
};


///< Start up the driver and put it into a safe mode
c_OutputMgr::c_OutputMgr ()
{
    // this gets called pre-setup so there is nothing we can do here.
} // c_OutputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_OutputMgr::~c_OutputMgr()
{
    DEBUG_START;

    // delete pOutputInstances;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        // the drivers will put the hardware in a safe state
        delete CurrentOutput;
    }
    DEBUG_END;

} // ~c_OutputMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_OutputMgr::Begin ()
{
    // DEBUG_START;

    // make sure the pointers are set up properly
    int ChannelIndex = 0;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        // the drivers will put the hardware in a safe state
        pOutputChannelDrivers[ChannelIndex++] = new c_OutputDisabled (c_OutputMgr::e_OutputChannelIds (ChannelIndex),
                                                                      gpio_num_t (-1),
                                                                      uart_port_t (-1));
    }

    // load up the configuration from the saved file. This also starts the drivers
    LoadConfig ();

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
///< Load and process the current configuration
/*
*   needs
*       Nothing
*   returns
*       Nothing
*/
void c_OutputMgr::LoadConfig ()
{
    // DEBUG_START;

    // try to load and process the config file
    if (FileIO::loadConfig (String("/" + String(OM_SECTION_NAME) + ".json").c_str(), std::bind (&c_OutputMgr::DeserializeConfig, this, std::placeholders::_1)))
    {
        // go through each output port and tell it to go
        for (auto CurrentOutputChannelDriver : pOutputChannelDrivers)
        {
            // if a driver is running, then re/start it
            if (nullptr != CurrentOutputChannelDriver)
            {
                CurrentOutputChannelDriver->Begin ();
            }
        } // end for each output channel
    } // end we got a config and it was good
    else
    {
        LOG_PORT.println (F ("EEEE Error loading Output Manager Config File. EEEE"));

        // create an empty config file
        SaveConfig (); 
    }

    // todo - remove this
    InstantiateNewOutputChannel (e_OutputChannelIds::OutputChannelId_1,
                                 e_OutputType::OutputType_WS2811);

    // DEBUG_END;
} // LoadConfig

//-----------------------------------------------------------------------------
/*
*   Save the current configuration to NVRAM
*
*   needs
*       Nothing
*   returns
*       Nothing
*/
void c_OutputMgr::SaveConfig ()
{
    // try to save the config file
    DynamicJsonDocument JsonConfigDoc(1024);

    SerializeConfig (JsonConfigDoc);

    String sJsonConfig = "";
    serializeJsonPretty (JsonConfigDoc, sJsonConfig);

    LOG_PORT.println ("ccccccccccccccccccccccccPrettyJsonConfigccccccccccccccccccccccccccc");
    LOG_PORT.println (sJsonConfig);
    LOG_PORT.println ("ccccccccccccccccccccccccPrettyJsonConfigccccccccccccccccccccccccccc");

    if (FileIO::saveConfig (String ("/" + String (OM_SECTION_NAME) + ".json").c_str (), sJsonConfig))
    {
        LOG_PORT.println (F ("**** Saved Output Manager Config File. ****"));
    } // end we got a config and it was good
    else
    {
        LOG_PORT.println (F ("EEEE Error Saving Output Manager Config File. EEEE"));
    }

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
bool c_OutputMgr::DeserializeConfig (DynamicJsonDocument & jsonConfig)
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

        // extract my own config data here

        // do we have a channel configuration array?
        if (false == OutputChannelMgrData.containsKey (OM_CHANNEL_SECTION_NAME))
        {
            // if not, flag an error and stop processing
            LOG_PORT.println (F ("No Output Channel Settings Found. Using Defaults"));
            break;
        }
        JsonObject OutputChannelArray = OutputChannelMgrData[OM_CHANNEL_SECTION_NAME];

        // for each output channel
        for (uint32_t ChannelIndex = OutputChannelId_Start;
            ChannelIndex < e_OutputChannelIds::OutputChannelId_End;
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

            // set a default value for channel type
            uint32_t TempChannelType = uint32_t (OutputType_End)+1;
            FileIO::setFromJSON (TempChannelType, OutputChannelConfig[OM_CHANNEL_TYPE_NAME]);

            // todo - this is wrong. Each type will have a config

            // is it a valid / supported channel type
            if ((TempChannelType < e_OutputType::OutputType_Start) || (TempChannelType > e_OutputType::OutputType_End))
            {
                // if not, flag an error and move on to the next channel
                LOG_PORT.println (String (F ("Invalid Channel Type in config '")) + TempChannelType + String (F ("'. Specified for channel '")) + ChannelIndex + "'. Disabling channel");
                InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType::OutputType_Disabled);
                continue;
            }

            // make sure the proper output type is running
            InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType (TempChannelType));

            // do we have a configuration for the running channel type?
            String DriverName = "";
            pOutputChannelDrivers[ChannelIndex]->GetDriverName (DriverName);
            if (false == OutputChannelConfig.containsKey (DriverName))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String (F ("No Output Settings Found for Channel '")) + ChannelIndex + "." + DriverName + String (F ("'. Using Defaults")));
                continue;
            }
            JsonObject OutputChannelDriverConfig = OutputChannelConfig[DriverName];

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
        SaveConfig ();
    }

    // DEBUG_END;
    return retval;

} // deserializeConfig

void c_OutputMgr::SerializeConfig (DynamicJsonDocument & jsonConfig)
{
    // DEBUG_START;

    JsonObject OutputMgrData;
    if (true == jsonConfig.containsKey (OM_SECTION_NAME))
    {
        OutputMgrData = jsonConfig[OM_SECTION_NAME];
    }
    else
    {
        // add our section header
        OutputMgrData = jsonConfig.createNestedObject (OM_SECTION_NAME);
    }

    // add OM config parameters

    // add the channels header
    JsonObject OutputMgrChannelsData;
    if (true == OutputMgrData.containsKey (OM_CHANNEL_SECTION_NAME))
    {
        OutputMgrChannelsData = OutputMgrData[OM_CHANNEL_SECTION_NAME];
    }
    else
    {
        // add our section header
        OutputMgrChannelsData = OutputMgrData.createNestedObject (OM_CHANNEL_SECTION_NAME);
    }

    // DEBUG_V("");

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
            ChannelConfigData = OutputMgrChannelsData[sChannelId];
        }
        else
        {
            // add our section header
            ChannelConfigData = OutputMgrChannelsData.createNestedObject (sChannelId);
        }
        
        // save the name as the selected channel type
        ChannelConfigData[OM_CHANNEL_TYPE_NAME] = int(CurrentChannel->GetOutputType ());

        String DriverTypeName = ""; CurrentChannel->GetDriverName (DriverTypeName);
        JsonObject ChannelConfigByTypeData;
        if (true == ChannelConfigData.containsKey (DriverTypeName))
        {
            ChannelConfigByTypeData = ChannelConfigData[DriverTypeName];
        }
        else
        {
            // add our section header
            ChannelConfigByTypeData = ChannelConfigData.createNestedObject (DriverTypeName);
        }

        // ask the channel to add its data to the record
        // DEBUG_V ("Add the output channel configuration");
        CurrentChannel->GetConfig (ChannelConfigByTypeData);
        // DEBUG_V ("");
    }

    // smile. Your done
    // DEBUG_END;
} // SerializeConfig

//-----------------------------------------------------------------------------
///< Create an instance of the desired output type in the desired channel
/*
    needs
        channel ID
        channel type
    returns 
        nothing
*/
void c_OutputMgr::InstantiateNewOutputChannel (e_OutputChannelIds ChannelIndex, e_OutputType NewOutputChannelType)
{
    // DEBUG_START;

    // Are we changing to a new output type.
    if (pOutputChannelDrivers[ChannelIndex]->GetOutputType() != NewOutputChannelType)
    {
        // DEBUG_V ("shut down the existing driver");
        // shut down the existing driver
        delete pOutputChannelDrivers[ChannelIndex];
        pOutputChannelDrivers[ChannelIndex] = nullptr;

        gpio_num_t dataPin = OutputChannelIdToGpioAndPort[ChannelIndex].dataPin;
        uart_port_t UartId = OutputChannelIdToGpioAndPort[ChannelIndex].UartId;

        switch (NewOutputChannelType)
        {
            case e_OutputType::OutputType_Disabled:
            {
                LOG_PORT.println (String (F ("************** Disabled output type for channel '")) + ChannelIndex + "'. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId);
                break;
            }

            case e_OutputType::OutputType_DMX:
            {
                LOG_PORT.println (String (F ("************** Starting up DMX for channel '")) + ChannelIndex + "'. **************");
                LOG_PORT.println (String (F ("************** DMX Not supported Yet. Using disabled. **************")));
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId);
                break;
            }

            case e_OutputType::OutputType_GECE:
            {
                LOG_PORT.println (String (F ("************** Starting up GECE for channel '")) + ChannelIndex + "'. **************");
                LOG_PORT.println (String (F ("************** GECE Not supported Yet. Using disabled. **************")));
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId);
                break;
            }

            case e_OutputType::OutputType_Serial:
            {
                LOG_PORT.println (String (F ("************** Starting up Generic Serial for channel '")) + ChannelIndex + "'. **************");
                LOG_PORT.println (String (F ("************** Generic Serial  Not supported Yet. Using disabled. **************")));
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId);
                break;
            }

            case e_OutputType::OutputType_Renard:
            {
                LOG_PORT.println (String (F ("************** Starting up Renard for channel '")) + ChannelIndex + "'. **************");
                LOG_PORT.println (String (F ("************** Renard Not supported Yet. Using disabled. **************")));
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId);
                break;
            }

            case e_OutputType::OutputType_SPI:
            {
                LOG_PORT.println (String (F ("************** Starting up SPI for channel '")) + ChannelIndex + "'. **************");
                LOG_PORT.println (String (F ("************** SPI Not supported Yet. Using disabled. **************")));
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId);
                break;
            }

            case e_OutputType::OutputType_WS2811:
            {
                LOG_PORT.println (String (F ("************** Starting up WS2811 for channel '")) + ChannelIndex + "'. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex, dataPin, UartId);
                break;
            }

            default:
            {
                LOG_PORT.println (String (F ("************** Unknown output type for channel '")) + ChannelIndex + "'. Using disabled. **************");
                pOutputChannelDrivers[ChannelIndex] = new c_OutputDisabled (ChannelIndex, dataPin, UartId);
                break;
            }

        } // end switch (NewChannelType)

        pOutputChannelDrivers[ChannelIndex]->Begin ();

    } // end replace running driver.

    // DEBUG_END;

} // InstantiateNewOutputChannel

//-----------------------------------------------------------------------------
///< Returns the current configuration for the output channels
/*
*   needs
*       Reference to string into which to place the configuration
*       presentation style
*   returns
*       string populated with currently available configuration
*/
void c_OutputMgr::GetConfig (DynamicJsonDocument & jsonConfig)
{
    // DEBUG_START;

    SerializeConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//-----------------------------------------------------------------------------
///< Sets the configuration for the current active ports
/*
*   Needs
*       Reference to the incoming JSON configuration doc
*   Returns
*       true - No Errors found
*       false - Had an issue and it was reported to the log interface
*/
bool c_OutputMgr::SetConfig (DynamicJsonDocument & jsonConfig)
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

