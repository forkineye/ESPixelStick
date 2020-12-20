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
typedef struct 
{
    c_InputMgr::e_InputType id;
    String name;
    c_InputMgr::e_InputChannelIds ChannelId;
} InputTypeXlateMap_t;

static const InputTypeXlateMap_t InputTypeXlateMap[c_InputMgr::e_InputType::InputType_End] =
{
    {c_InputMgr::e_InputType::InputType_E1_31,    "E1.31",      c_InputMgr::e_InputChannelIds::InputChannelId_1},
    {c_InputMgr::e_InputType::InputType_Effects,  "Effects",    c_InputMgr::e_InputChannelIds::InputChannelId_2},
    {c_InputMgr::e_InputType::InputType_MQTT,     "MQTT",       c_InputMgr::e_InputChannelIds::InputChannelId_1},
    {c_InputMgr::e_InputType::InputType_Alexa,    "Alexa",      c_InputMgr::e_InputChannelIds::InputChannelId_1},
    {c_InputMgr::e_InputType::InputType_DDP,      "DDP",        c_InputMgr::e_InputChannelIds::InputChannelId_1},
    {c_InputMgr::e_InputType::InputType_FPP,      "FPP Remote", c_InputMgr::e_InputChannelIds::InputChannelId_1},
    {c_InputMgr::e_InputType::InputType_Disabled, "Disabled",   c_InputMgr::e_InputChannelIds::InputChannelId_ALL}
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
        pInputChannelDrivers[pInputChannelDriversIndex] = nullptr;
        EffectEngineIsConfiguredToRun[pInputChannelDriversIndex] = false;
        ++pInputChannelDriversIndex;
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
        if (nullptr != CurrentInput)
        {
            // the drivers will put the hardware in a safe state
            delete CurrentInput;
            pInputChannelDrivers[pInputChannelDriversIndex] = nullptr;
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
    // DEBUG_V (String("InputDataBufferSize: ") + String (InputDataBufferSize));

    // prevent recalls
    if (true == HasBeenInitialized) { return; }
    HasBeenInitialized = true;

    String temp = String ("Effects Control");
    ExternalInput.Init (0,0, c_ExternalInput::Polarity_t::ActiveLow, temp);

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

    JsonObject InputMgrButtonData;
    
    if (true == jsonConfig.containsKey (IM_EffectsControlButtonName))
    {
        // DEBUG_V ("");
        InputMgrButtonData = jsonConfig[IM_EffectsControlButtonName];
    }
    else
    {
        // DEBUG_V ("");
        InputMgrButtonData = jsonConfig.createNestedObject (IM_EffectsControlButtonName);
    }
    // DEBUG_V ("");
    // extern void PrettyPrint (JsonObject & jsonStuff, String Name);
    
    // PrettyPrint (InputMgrButtonData, String("Before"));
    ExternalInput.GetConfig (InputMgrButtonData);
    // PrettyPrint (InputMgrButtonData, String("After"));

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
        if (nullptr == CurrentChannel)
        {
            // DEBUG_V ("");
            continue;
        }

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
    for (int InputTypeId = int (InputType_Start); 
         InputTypeId < int (InputType_End); 
         ++InputTypeId)
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
void c_InputMgr::GetConfig (char* Response)
{
    // DEBUGSTART;

    // is a new config waiting to be saved?
    if (0 != ConfigData.length ())
    {
        // DEBUGV (String ("ConfigData: ") + ConfigData);
        // use the pending config
        strcat (Response, ConfigData.c_str ());
    }
    else
    {
        String TempConfigData;
        FileMgr.ReadConfigFile (ConfigFileName, TempConfigData);
        // DEBUGV (String ("TempConfigData: ") + TempConfigData);
        strcat (Response, TempConfigData.c_str ());
    }

    // DEBUGEND;
} // GetConfig

//-----------------------------------------------------------------------------
void c_InputMgr::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject InputButtonStatus = jsonStatus.createNestedObject (F ("inputbutton"));
    ExternalInput.GetStatistics (InputButtonStatus);

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
/* Determine whether the input type is allowed on the desired input channel
*
    needs
        channel type
        channel id
    returns
        true - input type is allowed on the input channel
*/
bool c_InputMgr::InputTypeIsAllowedOnChannel (e_InputType type, e_InputChannelIds ChannelId)
{
    // DEBUG_START;

    bool response = false;

    // DEBUG_V (String("type: ") + String(type));

    // find the input type
    for (InputTypeXlateMap_t currentInputType : InputTypeXlateMap)
    {
        // DEBUG_V ("");
        // is this the entry we are looking for?
        if (currentInputType.id == type)
        {
            // DEBUG_V ("");
            // is the input allowed on the desired channel?
            if ((currentInputType.ChannelId == ChannelId) ||
                (currentInputType.ChannelId == InputChannelId_ALL))
            {
                // DEBUG_V ("Allowed");
                response = true;
            }
            // DEBUG_V ("");

            break;
        } // found the entry we are looking for
        // DEBUG_V ("");
    } // loop entries

    // DEBUG_V (String ("response: ") + String (response));

    // DEBUG_END;
    return response;
} // InputTypeIsAllowedOnChannel

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
                if (InputTypeIsAllowedOnChannel (InputType_E1_31, ChannelIndex))
                {
                    // LOG_PORT.println (String (F ("************** Starting E1.31 for channel '")) + ChannelIndex + "'. **************");
                    pInputChannelDrivers[ChannelIndex] = new c_InputE131 (ChannelIndex, InputType_E1_31, InputDataBuffer, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_Effects:
            {
                if (InputTypeIsAllowedOnChannel (InputType_Effects, ChannelIndex))
                {
                    // LOG_PORT.println (String (F ("************** Starting Effects Engine for channel '")) + ChannelIndex + "'. **************");
                    pInputChannelDrivers[ChannelIndex] = new c_InputEffectEngine (ChannelIndex, InputType_Effects, InputDataBuffer, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_MQTT:
            {
                if (InputTypeIsAllowedOnChannel (InputType_MQTT, ChannelIndex))
                {
                    // LOG_PORT.println (String (F ("************** Starting MQTT for channel '")) + ChannelIndex + "'. **************");
                    pInputChannelDrivers[ChannelIndex] = new c_InputMQTT (ChannelIndex, InputType_MQTT, InputDataBuffer, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_Alexa:
            {
                if (InputTypeIsAllowedOnChannel (InputType_Alexa, ChannelIndex))
                {
                    // LOG_PORT.println (String (F ("************** Starting Alexa for channel '")) + ChannelIndex + "'. **************");
                    pInputChannelDrivers[ChannelIndex] = new c_InputAlexa (ChannelIndex, InputType_Alexa, InputDataBuffer, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_DDP:
            {
                if (InputTypeIsAllowedOnChannel (InputType_DDP, ChannelIndex))
                {
                    // LOG_PORT.println (String (F ("************** Starting DDP for channel '")) + ChannelIndex + "'. **************");
                    pInputChannelDrivers[ChannelIndex] = new c_InputDDP (ChannelIndex, InputType_DDP, InputDataBuffer, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_FPP:
            {
                if (InputTypeIsAllowedOnChannel (InputType_FPP, ChannelIndex))
                {
                    // LOG_PORT.println (String (F ("************** Starting DDP for channel '")) + ChannelIndex + "'. **************");
                    pInputChannelDrivers[ChannelIndex] = new c_InputFPPRemote (ChannelIndex, InputType_FPP, InputDataBuffer, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    pInputChannelDrivers[ChannelIndex] = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBuffer, InputDataBufferSize);
                }
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
    if (!FileMgr.LoadConfigFile (ConfigFileName, [this](DynamicJsonDocument & JsonConfigDoc)
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

    ExternalInput.Poll ();
    ProcessEffectsButtonActions ();

    // DEBUG_V("");
    for (c_InputCommon* pInputChannel : pInputChannelDrivers)
    {
        pInputChannel->Process ();
        // DEBUG_V("");
    }
    // DEBUG_END;
} // Process

//-----------------------------------------------------------------------------
void c_InputMgr::ProcessEffectsButtonActions ()
{
    // DEBUG_START;

    if (false == ExternalInput.IsEnabled ())
    {
        // DEBUG_V ("Effects Button is disabled");
        // is the effects engine running?
        if (e_InputType::InputType_Effects == pInputChannelDrivers[EffectsChannel]->GetInputType ())
        {
            // is the effects engine configured to be running?
            if (false == EffectEngineIsConfiguredToRun[EffectsChannel])
            {
                // DEBUG_V ("turn off effects engine");
                InstantiateNewInputChannel (e_InputChannelIds (EffectsChannel), e_InputType::InputType_Disabled);
            }
        }
    }

    else if (ExternalInput.InputHadLongPush (true))
    {
        // DEBUG_V ("Had a Long Push");
        // Is the effects engine already running?
        if (e_InputType::InputType_Effects == pInputChannelDrivers[EffectsChannel]->GetInputType ())
        {
            // DEBUG_V ("turn off effects engine");
            InstantiateNewInputChannel (e_InputChannelIds (EffectsChannel), e_InputType::InputType_Disabled);
        }
        else
        {
            // DEBUG_V ("turn on effects engine");
            InstantiateNewInputChannel (e_InputChannelIds (EffectsChannel), e_InputType::InputType_Effects);
        }
    }

    else if (ExternalInput.InputHadShortPush (true))
    {
        // DEBUG_V ("Had a Short Push");
        // is the effects engine running?
        if (e_InputType::InputType_Effects == pInputChannelDrivers[EffectsChannel]->GetInputType ())
        {
            // DEBUG_V ("tell the effects engine to go to the next effect");
            ((c_InputEffectEngine*)(pInputChannelDrivers[EffectsChannel]))->NextEffect ();
        }
    }

    // DEBUG_END;

} // ProcessEffectsButtonActions

//-----------------------------------------------------------------------------
/*
    check the contents of the config and send
    the proper portion of the config to the currently instantiated channels

    needs
        ref to data from config file
    returns
        true  - config was properly processed
        false - config had an error.
*/
bool c_InputMgr::ProcessJsonConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    boolean Response = false;

    // DEBUG_V ("InputDataBufferSize: " + String (InputDataBufferSize));
    // DEBUG_V ("ConfigData: " + ConfigData);

    do // once
    {
        if (false == jsonConfig.containsKey (IM_SECTION_NAME))
        {
            LOG_PORT.println (F ("No Input Interface Settings Found. Using Defaults"));
            break;
        }
        JsonObject InputChannelMgrData = jsonConfig[IM_SECTION_NAME];
        // DEBUG_V ("");

        // extract my own config data here
        if (true == InputChannelMgrData.containsKey (IM_EffectsControlButtonName))
        {
            // DEBUG_V ("Found Input Button Config");
            JsonObject InputButtonConfig = InputChannelMgrData[IM_EffectsControlButtonName];
            ExternalInput.ProcessConfig (InputButtonConfig);
        }
        else
        {
            LOG_PORT.println (F ("No Input Button Settings Found. Using Defaults"));
        }

        // do we have a channel configuration array?
        if (false == InputChannelMgrData.containsKey (IM_CHANNEL_SECTION_NAME))
        {
            // if not, flag an error and stop processing
            LOG_PORT.println (F ("No Input Channel Settings Found. Using Defaults"));
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
            setFromJSON (ChannelType, InputChannelConfig, IM_CHANNEL_TYPE_NAME);
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

            EffectEngineIsConfiguredToRun[ChannelIndex] = (e_InputType::InputType_Effects == pInputChannelDrivers[ChannelIndex]->GetInputType()) ? true : false ;

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

    // DEBUGV (String("ConfigData: ") + ConfigData);

    if (FileMgr.SaveConfigFile (ConfigFileName, ConfigData))
    {
        LOG_PORT.println (F ("**** Saved Input Manager Config File. ****"));
        // DEBUG_V ("ConfigData: " + ConfigData);
        ConfigData.clear ();
    } // end we saved the config
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
        serializeJson (jsonConfig, ConfigData);
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
        // DEBUG_V ("");
        if (nullptr != pInputChannel)
        {
            pInputChannel->SetOperationalState (ActiveFlag);
            // DEBUG_V ("");
        }
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

//-----------------------------------------------------------------------------
void c_InputMgr::WiFiStateChanged (bool IsConnected)
{
    // DEBUG_START;

    // pass through each active interface and notify WiFi changed state
    for (c_InputCommon* pInputChannel : pInputChannelDrivers)
    {
        // DEBUG_V("");
        pInputChannel->WiFiStateChanged (IsConnected);
    }

    // DEBUG_END;
} // WiFiStateChanged

// create a global instance of the Input channel factory
c_InputMgr InputMgr;
