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

// bring driver definitions
#include "OutputWS2811.hpp"
#include "OutputGECE.hpp"
// needs to be last
#include "OutputMgr.hpp"

///< Start up the driver and put it into a safe mode
c_OutputMgr::c_OutputMgr ()
{
    // make sure the initial pointer states are clean
    pOutputChannelDrivers[e_OutputChannelIds::OutputChannelId_1] = nullptr;
    InstantiateNewOutputChannel (e_OutputChannelIds::OutputChannelId_1, e_OutputType::OutputType_WS2811);

#ifdef ARDUINO_ARCH_ESP32
    pOutputChannelDrivers[e_OutputChannelIds::OutputChannelId_2] = nullptr;
    InstantiateNewOutputChannel (e_OutputChannelIds::OutputChannelId_2, e_OutputType::OutputType_WS2811);

    pOutputChannelDrivers[e_OutputChannelIds::OutputChannelId_3] = nullptr;
    InstantiateNewOutputChannel (e_OutputChannelIds::OutputChannelId_3, e_OutputType::OutputType_SPI);

#endif // def ARDUINO_ARCH_ESP32

} // c_OutputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_OutputMgr::~c_OutputMgr()
{
    // delete pOutputInstances;
    for (auto CurrentOutput : pOutputChannelDrivers)
    {
        // the drivers will put the hardware in a safe state
        delete CurrentOutput;
    }

} // ~c_OutputMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_OutputMgr::begin ()
{
    // load up the configuration from the saved file. This also starts the drivers
    LoadConfig ();

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
    // try to load and process the config file
    if (FileIO::loadConfig (String(OM_SECTION_NAME).c_str(), std::bind (&c_OutputMgr::DeserializeConfig, this, std::placeholders::_1)))
    {
        // go through each output port and tell it to go
        for (auto CurrentOutputChannelDriver : pOutputChannelDrivers)
        {
            // if a driver is running, then re/start it
            if (nullptr != CurrentOutputChannelDriver)
            {
                CurrentOutputChannelDriver->begin ();
            }
        } // end for each output channel
    } // end we got a config and it was good

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
bool c_OutputMgr::DeserializeConfig (DynamicJsonDocument & jsonConfig)
{
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
        JsonObject OutputChannelArray = jsonConfig[OM_CHANNEL_SECTION_NAME];

        // is the channel configuration array the correct size?
            // if not then flag an error and stop processing

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

            // is it a valid / supported channel type
            if ((TempChannelType < e_OutputType::OutputType_Start) || (TempChannelType >= e_OutputType::OutputType_End))
            {
                // if not, flag an error and move on to the next channel
                LOG_PORT.println (String (F ("Invalid Channel Type '")) + TempChannelType + String (F ("'. Specified for channel '")) + ChannelIndex + "'");
                continue;
            }

            // is there currently a channel defined and running?
            if (nullptr == pOutputChannelDrivers[ChannelIndex])
            {
                // channel is not currently running. Start it.
                InstantiateNewOutputChannel (e_OutputChannelIds(ChannelIndex), e_OutputType(TempChannelType));
            }
            else
            {
                // is it the same channel type as the one currently running?
                if (TempChannelType != pOutputChannelDrivers[ChannelIndex]->GetOutputType ())
                {
                    // output type has changed. Start the new type
                    InstantiateNewOutputChannel (e_OutputChannelIds (ChannelIndex), e_OutputType (TempChannelType));

                } // end channel type is different
            }

            // if we have a running channel, send it a config (if we have one)

            // do we have a running instance?
            if (nullptr == pOutputChannelDrivers[ChannelIndex])
            {
                // nope. Dont go on
                continue;
            } // end cant find a running output driver

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

    return retval;

} // deserializeConfig

void c_OutputMgr::SerializeConfig (DynamicJsonDocument & jsonConfig)
{
    // add our section header
    JsonObject OutputMgrData = jsonConfig.createNestedObject (OM_SECTION_NAME);

    // add CM config parameters

    // add the channels header
    JsonObject OutputMgrChannelsData = OutputMgrData.createNestedObject (OM_CHANNEL_SECTION_NAME);

    // add the channel configurations
    for (auto CurrentChannel : pOutputChannelDrivers)
    {
        // is a driver instantiated?
        if (nullptr == CurrentChannel)
        {
            // no active driver here
            continue;
        }

        // create a record for this channel
        JsonObject ChannelConfigData = OutputMgrData.createNestedObject (String(CurrentChannel->GetOuputChannelId()));

        // ask the channel to add its data to the record
        CurrentChannel->GetConfig (ChannelConfigData);
    }

    // smile. Your done

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
void c_OutputMgr::InstantiateNewOutputChannel (e_OutputChannelIds ChannelIndex, e_OutputType NewChannelType)
{
    // remove any running instance
    if (nullptr != pOutputChannelDrivers[ChannelIndex])
    {
        // shut down the existing driver
        delete pOutputChannelDrivers[ChannelIndex];
        pOutputChannelDrivers[ChannelIndex] = nullptr;

    } // end remove running driver.

    switch (NewChannelType)
    {
        case e_OutputType::OutputType_DMX:
        {
            LOG_PORT.println (String (F ("DMX Not supported Yet. Using WS2811.")));
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex);
            break;
        }

        case e_OutputType::OutputType_GECE:
        {
            LOG_PORT.println (String (F ("GECE Not supported Yet. Using WS2811.")));
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex);
            break;
        }

        case e_OutputType::OutputType_Serial:
        {
            LOG_PORT.println (String (F ("Generic Serial  Not supported Yet. Using WS2811.")));
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex);
            break;
        }

        case e_OutputType::OutputType_Renard:
        {
            LOG_PORT.println (String (F ("Renard Not supported Yet. Using WS2811.")));
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex);
            break;
        }

        case e_OutputType::OutputType_SPI:
        {
            LOG_PORT.println (String (F ("SPI Not supported Yet. Using WS2811.")));
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex);
            break;
        }

        case e_OutputType::OutputType_WS2811:
        {
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex);
            break;
        }

        default:
        {
            LOG_PORT.println (String (F ("Unknown output type. Using WS2811.")));
            pOutputChannelDrivers[ChannelIndex] = new c_OutputWS2811 (ChannelIndex);
            break;
        }

    } // end switch (NewChannelType)

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
void c_OutputMgr::GetConfig (String & sConfig, bool pretty)
{
    // build the config manager section


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

    boolean retval = false;
    if (jsonConfig.containsKey (OM_SECTION_NAME))
    {
//        retval = FileIO::setFromJSON (pixel_count, jsonConfig[OM_SECTION_NAME]["pixel_count"]);
    }
    else
    {
        LOG_PORT.println ("No Output Manager settings found.");
    }

} // SetConfig

//-----------------------------------------------------------------------------
///< Called from loop(), renders output data
void c_OutputMgr::render()
{
    for (c_OutputCommon * pOutputChannel : pOutputChannelDrivers)
    {
        pOutputChannel->render ();
    }
} // render



