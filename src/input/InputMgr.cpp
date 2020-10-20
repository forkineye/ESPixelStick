/*
* InputMgr.cpp - Input Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2020 Shelby Merrick
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
*   This is a factory class used to manage the Input channel. It creates and deletes
*   the Input channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "../ESPixelStick.h"
#include "../FileIO.h"

//-----------------------------------------------------------------------------
// bring in driver definitions
#include "InputDisabled.hpp"
#include "InputE131.hpp"
#include "InputEffectEngine.hpp"
#include "InputMQTT.h"
#include "InputAlexa.h"
#include "InputDDP.h"
#include "InputFPPRemote.h"
// needs to be last
#include "InputMgr.hpp"

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Local Data definitions
//-----------------------------------------------------------------------------
typedef struct InputTypeXlateMap_t
{
    c_InputMgr::e_InputType id;
    String name;
};

InputTypeXlateMap_t InputTypeXlateMap[c_InputMgr::e_InputType::InputType_End] =
{
    {c_InputMgr::e_InputType::InputType_E1_31,    "E1.31"      },
    {c_InputMgr::e_InputType::InputType_Effects,  "Effects"    },
    {c_InputMgr::e_InputType::InputType_MQTT,     "MQTT"       },
    {c_InputMgr::e_InputType::InputType_Alexa,    "Alexa"      },
    {c_InputMgr::e_InputType::InputType_DDP,      "DDP"        },
    {c_InputMgr::e_InputType::InputType_FPP,      "FPP Remote" },
    {c_InputMgr::e_InputType::InputType_Disabled, "Disabled"   }
};

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_InputMgr::c_InputMgr ()
{
    ConfigFileName = String (F ("/")) + String (IM_SECTION_NAME) + F (".json");

    // this gets called pre-setup so there is nothing we can do here.
    int pInputChannelDriversIndex = 0;
    for (c_InputCommon* CurrentInput : pInputChannelDrivers)
    {
        pInputChannelDrivers[pInputChannelDriversIndex++] = nullptr;
    }

} // c_InputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the Input channels into a safe state
c_InputMgr::~c_InputMgr ()
{
    // DEBUG_START;

    // delete pInputInstances;
    int pInputChannelDriversIndex = 0;
    for (c_InputCommon* CurrentInput : pInputChannelDrivers)
    {
        if (nullptr != pInputChannelDrivers[pInputChannelDriversIndex])
        {
            // the drivers will put the hardware in a safe state
            delete pInputChannelDrivers[pInputChannelDriversIndex];
        }
        pInputChannelDriversIndex++;
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
    // DEBUG_V ("InputDataBufferSize: " + String (InputDataBufferSize));

    // prevent recalls
    if (true == HasBeenInitialized) { return; }
    HasBeenInitialized = true;

    // make sure the pointers are set up properly
    int ChannelIndex = 0;
    for (c_InputCommon* CurrentInput : pInputChannelDrivers)
    {
        InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex++), e_InputType::InputType_Disabled);
        // DEBUG_V ("");
    }

    // load up the configuration from the saved file. This also starts the drivers
    LoadConfig ();

    // CreateNewConfig ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputMgr::CreateJsonConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    // add IM config parameters
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
    for (c_InputCommon* CurrentChannel : pInputChannelDrivers)
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
        ChannelConfigData[IM_CHANNEL_TYPE_NAME] = int (CurrentChannel->GetInputType ());

        String DriverTypeId = String (int (CurrentChannel->GetInputType ()));
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
        // DEBUG_V ("Add the Input channel configuration for type: " + DriverTypeId);

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
    objects. To create a complete config we need to start each Input type on
    each Input channel and collect the configuration at each stage.
*/
void c_InputMgr::CreateNewConfig ()
{
    // DEBUG_START;
    LOG_PORT.println (F ("--- WARNING: Creating a new Input Manager configuration Data set - Start ---"));

    // create a place to save the config
    DynamicJsonDocument JsonConfigDoc (IM_JSON_SIZE);
    JsonObject JsonConfig = JsonConfigDoc.createNestedObject (IM_SECTION_NAME);

    // DEBUG_V ("for each Input type");
    for (int InputTypeId = int (InputType_Start); InputTypeId < int (InputType_End); ++InputTypeId)
    {
        // DEBUG_V ("for each input channel");
        int ChannelIndex = 0;
        for (c_InputCommon* CurrentInput : pInputChannelDrivers)
        {
            // DEBUG_V (String("instantiate the Input type: ") + InputTypeId);
            InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex++), e_InputType (InputTypeId));
        }// end for each interface

        // DEBUG_V ("collect the config data");
        CreateJsonConfig (JsonConfig);

        // DEBUG_V ("");

    } // end for each Input type

    // DEBUG_V ("leave the Inputs disabled");
    int ChannelIndex = 0;
    for (c_InputCommon* CurrentInput : pInputChannelDrivers)
    {
        InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex++), e_InputType::InputType_Disabled);
    }// end for each interface

    // DEBUG_V ("");

    // Record the default configuration
    CreateJsonConfig (JsonConfig);

    ConfigData.clear ();
    serializeJson (JsonConfigDoc, ConfigData);
    ConfigSaveNeeded = false;
    SaveConfig ();

    JsonConfigDoc.garbageCollect ();

    LOG_PORT.println (F ("--- WARNING: Creating a new Input Manager configuration Data set - Done ---"));
    // DEBUG_END;

} // CreateNewConfig

//-----------------------------------------------------------------------------
void c_InputMgr::GetConfig (char * Response)
{
    // DEBUG_START;

    strcat (Response, ConfigData.c_str ());

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputMgr::GetOptions (JsonObject & jsonOptions)
{
    // DEBUG_START;

    JsonArray SelectedOptionList = jsonOptions.createNestedArray ("selectedoptionlist");

    // build a list of the current available channels and their output type
    for (c_InputCommon* currentInput : pInputChannelDrivers)
    {
        JsonObject selectedoption = SelectedOptionList.createNestedObject ();
        selectedoption[F ("id")            ] = currentInput->GetInputChannelId ();
        selectedoption[F ("selectedoption")] = currentInput->GetInputType ();
    }

    // DEBUG_V ("");

    JsonArray jsonOptionsArray = jsonOptions.createNestedArray (F ("list"));
    // DEBUG_V ("");

    // Build a list of Valid options for this device
    for (InputTypeXlateMap_t currentInputType : InputTypeXlateMap)
    {
        // DEBUG_V ("");
        JsonObject jsonOptionsArrayEntry  = jsonOptionsArray.createNestedObject ();
        jsonOptionsArrayEntry[F ("id")]   = int(currentInputType.id);
        jsonOptionsArrayEntry[F ("name")] = currentInputType.name;

        // DEBUG_V ("");
    } // end for each Input type

    // DEBUG_END;
} // GetOptions

//-----------------------------------------------------------------------------
void c_InputMgr::GetStatus (JsonObject & jsonStatus)
{
    // DEBUG_START;

    JsonArray InputStatus = jsonStatus.createNestedArray (F ("input"));
    for (c_InputCommon* CurrentInput : pInputChannelDrivers)
    {
        JsonObject channelStatus = InputStatus.createNestedObject ();
        CurrentInput->GetStatus (channelStatus);
        // DEBUG_V("");
    }

    // DEBUG_END;
} // GetStatus

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

    do // once
    {
        // is there an existing driver?
        if (nullptr != pInputChannelDrivers[ChannelIndex])
        {
            // DEBUG_V (String("pInputChannelDrivers[ChannelIndex]->GetInputType () '") + pInputChannelDrivers[ChannelIndex]->GetInputType () + String("'"));
            // DEBUG_V (String("NewInputChannelType '") + int(NewInputChannelType) + "'");

            // DEBUG_V ("does the driver need to change?");
            if (pInputChannelDrivers[ChannelIndex]->GetInputType () == NewInputChannelType)
            {
                // DEBUG_V ("nothing to change");
                break;
            }

            // DEBUG_V ("shut down the existing driver");
            delete pInputChannelDrivers[ChannelIndex];
            pInputChannelDrivers[ChannelIndex] = nullptr;
            // DEBUG_V ("");
        } // end there is an existing driver

     // DEBUG_V ("InputDataBufferSize: " + String(InputDataBufferSize));

        switch (NewInputChannelType)
        {
            case e_InputType::InputType_Disabled:
            {
                // LOG_PORT.println (String (F ("************** Disabled Input type for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            case e_InputType::InputType_E1_31:
            {
                // LOG_PORT.println (String (F ("************** Starting E1.31 for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputE131 (ChannelIndex, InputType_E1_31, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            case e_InputType::InputType_Effects:
            {
                // LOG_PORT.println (String (F ("************** Starting Effects Engine for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputEffectEngine (ChannelIndex, InputType_Effects, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            case e_InputType::InputType_MQTT:
            {
                // LOG_PORT.println (String (F ("************** Starting MQTT for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputMQTT (ChannelIndex, InputType_MQTT, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            case e_InputType::InputType_Alexa:
            {
                // LOG_PORT.println (String (F ("************** Starting Alexa for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputAlexa (ChannelIndex, InputType_Alexa, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            case e_InputType::InputType_DDP:
            {
                // LOG_PORT.println (String (F ("************** Starting DDP for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputDDP (ChannelIndex, InputType_DDP, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            case e_InputType::InputType_FPP:
            {
                // LOG_PORT.println (String (F ("************** Starting DDP for channel '")) + ChannelIndex + "'. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputFPPRemote (ChannelIndex, InputType_FPP, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            default:
            {
                LOG_PORT.println (String (F ("************** Unknown Input type for channel '")) + ChannelIndex + "'. Using disabled. **************");
                pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }
        } // end switch (NewChannelType)
          
        // DEBUG_V ("");
        pInputChannelDrivers[ChannelIndex]->Begin ();

    } while (false);

    // DEBUG_END;

} // InstantiateNewInputChannel

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
    if (!FileIO::loadConfig (ConfigFileName, [this](DynamicJsonDocument & JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject JsonConfig = JsonConfigDoc.as<JsonObject> ();
            // DEBUG_V ("");
            this->ProcessJsonConfig (JsonConfig);
            // DEBUG_V ("");
        }, IM_JSON_SIZE))
    {
        LOG_PORT.println (F ("EEEE Error loading Input Manager Config File. EEEE"));

        // create a config file with default values
        // DEBUG_V ("");
        CreateNewConfig ();
    }

    // DEBUG_END;

} // LoadConfig

//-----------------------------------------------------------------------------
///< Called from loop(), renders Input data
void c_InputMgr::Process ()
{
    // DEBUG_START;
    // do we need to save the current config?
    if (true == ConfigSaveNeeded)
    {
        // DEBUG_V ("ConfigData: " + ConfigData);

        ConfigSaveNeeded = false;
        SaveConfig ();
        // DEBUG_V("");

    } // done need to save the current config

    // DEBUG_V("");
    for (c_InputCommon* pInputChannel : pInputChannelDrivers)
    {
        pInputChannel->Process ();
        // DEBUG_V("");
    }
    // DEBUG_END;
} // Process

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
bool c_InputMgr::ProcessJsonConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    boolean Response = false;

    // DEBUG_V ("InputDataBufferSize: " + String (InputDataBufferSize));

    // keep a local copy of the config
    ConfigData.clear ();
    serializeJson (jsonConfig, ConfigData);
    // DEBUG_V ("ConfigData: " + ConfigData);

    do // once
    {
        if (false == jsonConfig.containsKey (IM_SECTION_NAME))
        {
            LOG_PORT.println (F ("No Input Interface Settings Found. Using Defaults"));
            ConfigData.clear ();
            break;
        }
        JsonObject InputChannelMgrData = jsonConfig[IM_SECTION_NAME];
        // DEBUG_V ("");

        // extract my own config data here

        // do we have a channel configuration array?
        if (false == InputChannelMgrData.containsKey (IM_CHANNEL_SECTION_NAME))
        {
            // if not, flag an error and stop processing
            LOG_PORT.println (F ("No Input Channel Settings Found. Using Defaults"));
            ConfigData.clear ();
            break;
        }
        JsonObject InputChannelArray = InputChannelMgrData[IM_CHANNEL_SECTION_NAME];
        // DEBUG_V ("");

        // for each Input channel
        for (uint32_t ChannelIndex = uint32_t (InputChannelId_Start);
            ChannelIndex < uint32_t (InputChannelId_End);
            ChannelIndex++)
        {
            // get access to the channel config
            if (false == InputChannelArray.containsKey (String (ChannelIndex)))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String (F ("No Input Settings Found for Channel '")) + ChannelIndex + String (F ("'. Using Defaults")));
                continue;
            }
            JsonObject InputChannelConfig = InputChannelArray[String (ChannelIndex)];
            // DEBUG_V ("");

            // set a default value for channel type
            uint32_t ChannelType = uint32_t (InputType_Default);
            FileIO::setFromJSON (ChannelType, InputChannelConfig[IM_CHANNEL_TYPE_NAME]);
            // DEBUG_V ("");

            // is it a valid / supported channel type
            if ((ChannelType < uint32_t (InputType_Start)) || (ChannelType >= uint32_t (InputType_End)))
            {
                // if not, flag an error and move on to the next channel
                LOG_PORT.println (String (F ("Invalid Channel Type in config '")) + ChannelType + String (F ("'. Specified for channel '")) + ChannelIndex + "'. Disabling channel");
                InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType::InputType_Disabled);
                continue;
            }
            // DEBUG_V ("");

            // do we have a configuration for the channel type?
            if (false == InputChannelConfig.containsKey (String (ChannelType)))
            {
                // if not, flag an error and stop processing
                LOG_PORT.println (String (F ("No Input Settings Found for Channel '")) + ChannelIndex + String (F ("'. Using Defaults")));
                InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType::InputType_Disabled);
                continue;
            }

            JsonObject InputChannelDriverConfig = InputChannelConfig[String (ChannelType)];
            // DEBUG_V ("");

            // make sure the proper Input type is running
            InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType (ChannelType));
            // DEBUG_V (String ("Response: ") + Response);

            // send the config to the driver. At this level we have no idea what is in it
            pInputChannelDrivers[ChannelIndex]->SetConfig (InputChannelDriverConfig);
            // DEBUG_V (String("Response: ") + Response);

        } // end for each channel

		Response = true;

    } while (false);

    // did we get a valid config?
    if (false == Response)
    {
        // save the current config since it is the best we have.
        // DEBUG_V ("");
        CreateNewConfig ();
    }

    // DEBUG_V ("InputDataBufferSize: " + String (InputDataBufferSize));

    // DEBUG_END;
    return Response;

} // ProcessJsonConfig

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

    // DEBUG_V ("ConfigData: " + ConfigData);

    if (FileIO::SaveConfig (ConfigFileName, ConfigData))
    {
        LOG_PORT.println (F ("**** Saved Input Manager Config File. ****"));
        // DEBUG_V ("ConfigData: " + ConfigData);
    } // end we got a config and it was good
    else
    {
        LOG_PORT.println (F ("EEEE Error Saving Input Manager Config File. EEEE"));
    }

    // DEBUG_END;

} // SaveConfig

//-----------------------------------------------------------------------------
/* Sets the configuration for the current active ports: 
*
*   WARNING: This runs in the Web server context and cannot access the File system
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
    boolean Response = false;
    if (jsonConfig.containsKey (IM_SECTION_NAME))
    {
        // DEBUG_V ("");
        // process the config data
        Response = ProcessJsonConfig (jsonConfig);

        // schedule a future save of the config file
        ConfigSaveNeeded = true;
    }
    else
    {
        LOG_PORT.println (F ("EEEE No Input Manager settings found in new config. EEEE"));
    }
    
    // DEBUG_END;
    
    return Response;
} // SetConfig

//-----------------------------------------------------------------------------
void c_InputMgr::SetBufferInfo (uint8_t* BufferStart, uint16_t BufferSize)
{
    // DEBUG_START;

    InputDataBuffer = BufferStart;
    InputDataBufferSize = BufferSize;

    // DEBUG_V ("InputDataBufferSize: " + String (InputDataBufferSize));

    // pass through each active interface and set the buffer info
            // for each Input channel
    for (int ChannelIndex = int (InputChannelId_Start);
        ChannelIndex < int (InputChannelId_End);
        ChannelIndex++)
    {
        if (nullptr != pInputChannelDrivers[ChannelIndex])
        {
            pInputChannelDrivers[ChannelIndex]->SetBufferInfo (InputDataBuffer, InputDataBufferSize);
        }
    } // end for each channel

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
void c_InputMgr::SetOperationalState (bool ActiveFlag)
{
    // DEBUG_START;

    // pass through each active interface and set the active state
    for (c_InputCommon* pInputChannel : pInputChannelDrivers)
    {
        pInputChannel->SetOperationalState (ActiveFlag);
        // DEBUG_V("");
    }

    // DEBUG_END;

} // SetOutputState

//-----------------------------------------------------------------------------
void c_InputMgr::ResetBlankTimer ()
{
    // DEBUG_START;

    // pass through each active interface and set the blank state
    for (c_InputCommon* pInputChannel : pInputChannelDrivers)
    {
        pInputChannel->ResetBlankTimer ();
        // DEBUG_V("");
    }

    // DEBUG_END;

} // SetOutputState

// create a global instance of the Input channel factory
c_InputMgr InputMgr;
