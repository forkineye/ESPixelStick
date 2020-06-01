/*
* InputMgr.cpp - Input Management class
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
*   This is a factory class used to manage the Input port. It creates and deletes
*   the Input channel functionality as needed to support any new configurations 
*   that get sent from from the WebPage.
*
*/

#include <Arduino.h>
#include "../ESPixelStick.h"
#include "../FileIO.h"

//-----------------------------------------------------------------------------
// bring in driver definitions
#include "E131Input.h"
// needs to be last
#include "InputMgr.hpp"
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_InputMgr::c_InputMgr ()
{
    // this gets called pre-setup so there is nothing we can do here.
} // c_InputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the Input channels into a safe state
c_InputMgr::~c_InputMgr()
{
    // DEBUG_START;

    // delete pInputInstances;
    for (auto CurrentInput : pInputChannelDrivers)
    {
        // the drivers will put the hardware in a safe state
        delete CurrentInput;
    }
    // DEBUG_END;

} // ~c_InputMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_InputMgr::Begin (uint8_t* BufferStart, uint16_t BufferSize)
{
    // DEBUG_START;

    InputDataBuffer     = BufferStart;
    InputDataBufferSize = BufferSize;
    // DEBUG_V ("");

    // make sure the pointers are set up properly
    int ChannelIndex = e_InputChannelIds::InputChannelId_1;
    for (auto CurrentInput : pInputChannelDrivers)
    {
        // DEBUG_V ("");
        // the drivers will put the hardware in a safe state
        pInputChannelDrivers[ChannelIndex] = new c_InputE131 (e_InputChannelIds(ChannelIndex),
                                                              e_InputType::InputType_E1_31,
                                                              InputDataBuffer, 
                                                              InputDataBufferSize);
        ChannelIndex++;
        // DEBUG_V ("");
    }
    // DEBUG_V ("");

    // load up the configuration from the saved file. This also starts the drivers
    LoadConfig ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
/* Load and process the current configuration
*
*   needs
*       Nothing
*   returns
*       Nothing
*/
void c_InputMgr::LoadConfig ()
{
    // DEBUG_START;

    // try to load and process the config file
    if (FileIO::loadConfig (String("/" + String(IM_SECTION_NAME) + ".json").c_str(), 
        [this](DynamicJsonDocument & JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject JsonConfig = JsonConfigDoc.as<JsonObject> ();
            // DEBUG_V ("");
            this->DeserializeConfig (JsonConfig);
            // DEBUG_V ("");
        }))
    {
        // DEBUG_V ("");
        // go through each Input port and tell it to go
        for (auto CurrentInputChannelDriver : pInputChannelDrivers)
        {
            // if a driver is running, then re/start it
            if (nullptr != CurrentInputChannelDriver)
            {
                // DEBUG_V ("");
                CurrentInputChannelDriver->Begin ();
                // DEBUG_V ("");
            }
        } // end for each Input channel
    } // end we got a config and it was good
    else
    {
        LOG_PORT.println (F ("EEEE Error loading Input Manager Config File. EEEE"));

        // create an empty config file
        SaveConfig (); 
        // DEBUG_V ("");
    }

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
void c_InputMgr::SaveConfig ()
{
    // DEBUG_START;
    String sJsonConfig = "";

    // try to save the config file
    DynamicJsonDocument JsonConfigDoc(1024);
    JsonObject JsonConfig = JsonConfigDoc.createNestedObject(IM_SECTION_NAME);

    // DEBUG_V ("");
    SerializeConfig (JsonConfig);
    // DEBUG_V ("");

#ifdef foo
    serializeJsonPretty (JsonConfigDoc, sJsonConfig);

    LOG_PORT.println ("iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii PrettyJsonConfig iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii ccc");
    LOG_PORT.println (sJsonConfig);
    LOG_PORT.println ("iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii PrettyJsonConfig iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii ccc");
#endif // def foo

    serializeJson (JsonConfigDoc, sJsonConfig);
    // DEBUG_V ("");
    if (FileIO::saveConfig (String ("/" + String (IM_SECTION_NAME) + ".json").c_str (), sJsonConfig))
    {
        LOG_PORT.println (F ("**** Saved Input Manager Config File. ****"));
    } // end we got a config and it was good
    else
    {
        LOG_PORT.println (F ("EEEE Error Saving Input Manager Config File. EEEE"));
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
bool c_InputMgr::DeserializeConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    boolean retval = false;
    do // once
    {
#ifdef foo
        String sJsonConfig = "";
        serializeJsonPretty (jsonConfig, sJsonConfig);

        LOG_PORT.println ("iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii PrettyJsonConfig iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii ccc");
        LOG_PORT.println (sJsonConfig);
        LOG_PORT.println ("iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii PrettyJsonConfig iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii ccc");
#endif // def foo

        if (false == jsonConfig.containsKey (IM_SECTION_NAME))
        {
            LOG_PORT.println (F ("No Input Interface Settings Found. Using Defaults"));
            break;
        }
        JsonObject InputChannelMgrData = jsonConfig[IM_SECTION_NAME];

        // extract my own config data here

        // do we have a channel configuration array?
        if (false == InputChannelMgrData.containsKey (IM_CHANNEL_SECTION_NAME))
        {
            // if not, flag an error and stop processing
            LOG_PORT.println (F ("No Input Channel Settings Found. Using Defaults"));
            break;
        }
        JsonObject InputChannelArray = InputChannelMgrData[IM_CHANNEL_SECTION_NAME];

        // for each Input channel
        for (uint32_t ChannelIndex = InputChannelId_Start;
            ChannelIndex < e_InputChannelIds::InputChannelId_End;
            ChannelIndex++)
        {
            // get access to the channel config
            if (false == InputChannelArray.containsKey (String(ChannelIndex).c_str()))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String(F ("No Input Settings Found for Channel '")) + ChannelIndex + String(F("'. Using Defaults")));
                break;
            }
            JsonObject InputChannelConfig = InputChannelArray[String (ChannelIndex).c_str ()];

            // set a default value for channel type
            uint32_t TempChannelType = uint32_t (InputType_End)+1;
            FileIO::setFromJSON (TempChannelType, InputChannelConfig[IM_CHANNEL_TYPE_NAME]);

            // todo - this is wrong. Each type will have a config

            // is it a valid / supported channel type
            if ((TempChannelType < e_InputType::InputType_Start) || (TempChannelType > e_InputType::InputType_End))
            {
                // if not, flag an error and move on to the next channel
                LOG_PORT.println (String (F ("Invalid Channel Type in config '")) + TempChannelType + String (F ("'. Specified for channel '")) + ChannelIndex + "'. Disabling channel");
                InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType::InputType_Disabled);
                continue;
            }

            // make sure the proper Input type is running
            InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType (TempChannelType));

            // do we have a configuration for the running channel type?
            String DriverName = "";
            pInputChannelDrivers[ChannelIndex]->GetDriverName (DriverName);
            if (false == InputChannelConfig.containsKey (DriverName))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String (F ("No Input Settings Found for Channel '")) + ChannelIndex + "." + DriverName + String (F ("'. Using Defaults")));
                continue;
            }
            JsonObject InputChannelDriverConfig = InputChannelConfig[DriverName];

            // send the config to the driver. At this level we have no idea what is in it
            retval |= pInputChannelDrivers[ChannelIndex]->SetConfig (InputChannelDriverConfig);

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

//-----------------------------------------------------------------------------
void c_InputMgr::SerializeConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    // add OM config parameters
    // DEBUG_V ("");

    // add the channels header
    JsonObject InputMgrChannelsData;
    if (true == jsonConfig.containsKey (IM_CHANNEL_SECTION_NAME))
    {
        // DEBUG_V ("");
        InputMgrChannelsData = jsonConfig[IM_CHANNEL_SECTION_NAME];
    }
    else
    {
        // add our section header
        // DEBUG_V ("");
        InputMgrChannelsData = jsonConfig.createNestedObject (IM_CHANNEL_SECTION_NAME);
    }

    // add the channel configurations
    // DEBUG_V ("For Each Input Channel");
    for (auto CurrentChannel : pInputChannelDrivers)
    {
        // DEBUG_V (String("Create Section in Config file for the Input channel: '") + CurrentChannel->GetInputChannelId() + "'");
        // create a record for this channel
        JsonObject ChannelConfigData;
        String sChannelId = String (CurrentChannel->GetInputChannelId ());
        if (true == InputMgrChannelsData.containsKey (sChannelId))
        {
            // DEBUG_V ("");
            ChannelConfigData = InputMgrChannelsData[sChannelId];
        }
        else
        {
            // add our section header
            // DEBUG_V ("");
            ChannelConfigData = InputMgrChannelsData.createNestedObject (sChannelId);
        }
        
        // save the name as the selected channel type
        ChannelConfigData[IM_CHANNEL_TYPE_NAME] = int(CurrentChannel->GetInputType ());

        String DriverTypeName = ""; CurrentChannel->GetDriverName (DriverTypeName);
        JsonObject ChannelConfigByTypeData;
        if (true == ChannelConfigData.containsKey (DriverTypeName))
        {
            ChannelConfigByTypeData = ChannelConfigData[DriverTypeName];
            // DEBUG_V ("");
        }
        else
        {
            // add our section header
            // DEBUG_V ("");
            ChannelConfigByTypeData = ChannelConfigData.createNestedObject (DriverTypeName);
        }

        // ask the channel to add its data to the record
        // DEBUG_V ("Add the Input channel configuration");
        CurrentChannel->GetConfig (ChannelConfigByTypeData);
        // DEBUG_V ("");
    }

    // smile. Your done
    // DEBUG_END;
} // SerializeConfig

//-----------------------------------------------------------------------------
/* Create an instance of the desired Input type in the desired channel
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
void c_InputMgr::InstantiateNewInputChannel (e_InputChannelIds ChannelIndex, e_InputType NewInputChannelType)
{
    // DEBUG_START;

    // Are we changing to a new Input type.
    if (pInputChannelDrivers[int(ChannelIndex)]->GetInputType() != NewInputChannelType)
    {
        // DEBUG_V ("shut down the existing driver");
        delete pInputChannelDrivers[int(ChannelIndex)];
        pInputChannelDrivers[int(ChannelIndex)] = nullptr;

        switch (NewInputChannelType)
        {
            case e_InputType::InputType_E1_31:
            {
                LOG_PORT.println (String (F ("************** Starting up DMX for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[int(ChannelIndex)] = new c_InputE131 (ChannelIndex, e_InputType::InputType_E1_31, InputDataBuffer, InputDataBufferSize);
                break;
            }

            case e_InputType::InputType_Disabled:
            default:
            {
                LOG_PORT.println (String (F ("************** Unknown Input type for channel '")) + ChannelIndex + "'. Using disabled. **************");
 //               pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, e_InputType::InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                break;
            }

        } // end switch (NewChannelType)

        pInputChannelDrivers[int(ChannelIndex)]->Begin ();

    } // end replace running driver.

    // DEBUG_END;

} // InstantiateNewInputChannel

//-----------------------------------------------------------------------------
/* Returns the current configuration for the Input channels
*
*   needs
*       Reference to string into which to place the configuration
*       presentation style
*   returns
*       string populated with currently available configuration
*/
void c_InputMgr::GetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    JsonObject JsonImConfig = jsonConfig.createNestedObject (IM_SECTION_NAME);

    SerializeConfig (JsonImConfig);

    // DEBUG_END;
} // GetConfig

//-----------------------------------------------------------------------------
/* Sets the configuration for the current active ports
*
*   Needs
*       Reference to the incoming JSON configuration doc
*   Returns
*       true - No Errors found
*       false - Had an issue and it was reported to the log interface
*/
bool c_InputMgr::SetConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    boolean retval = false;
    if (jsonConfig.containsKey (IM_SECTION_NAME))
    {
        DeserializeConfig (jsonConfig);
    }
    else
    {
        LOG_PORT.println (F("EEEE No Input Manager settings found. EEEE"));
    }
    // DEBUG_END;
} // SetConfig

//-----------------------------------------------------------------------------
///< Called from loop(), renders Input data
void c_InputMgr::Process()
{
    // DEBUG_START;
    for (c_InputCommon * pInputChannel : pInputChannelDrivers)
    {
        pInputChannel->Process ();
    }
    // DEBUG_END;
} // render

// create a global instance of the Input channel factory
c_InputMgr InputMgr;

