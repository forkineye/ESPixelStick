/*
* InputMgr.cpp - Input Management class
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
*   This is a factory class used to manage the Input channel. It creates and deletes
*   the Input channel functionality as needed to support any new configurations
*   that get sent from from the WebPage.
*
*/

#include "ESPixelStick.h"

//-----------------------------------------------------------------------------
// bring in driver definitions
#include "input/InputDisabled.hpp"
#include "input/InputE131.hpp"
#include "input/InputEffectEngine.hpp"
#include "input/InputMQTT.h"
#include "input/InputAlexa.h"
#include "input/InputDDP.h"
#include "input/InputFPPRemote.h"
#include "input/InputArtnet.hpp"
// needs to be last
#include "input/InputMgr.hpp"

//-----------------------------------------------------------------------------
// Local Data definitions
//-----------------------------------------------------------------------------
struct InputTypeXlateMap_t
{
    c_InputMgr::e_InputType id;
    String name;
    c_InputMgr::e_InputChannelIds ChannelId;
};

static const InputTypeXlateMap_t InputTypeXlateMap[c_InputMgr::e_InputType::InputType_End] =
{
    {c_InputMgr::e_InputType::InputType_E1_31,    "E1.31",      c_InputMgr::e_InputChannelIds::InputPrimaryChannelId},
    {c_InputMgr::e_InputType::InputType_DDP,      "DDP",        c_InputMgr::e_InputChannelIds::InputPrimaryChannelId},
#ifdef SUPPORT_FPP
    {c_InputMgr::e_InputType::InputType_FPP,      "FPP Remote", c_InputMgr::e_InputChannelIds::InputSecondaryChannelId},
#endif // def SUPPORT_FPP
    {c_InputMgr::e_InputType::InputType_Artnet,   "Artnet",     c_InputMgr::e_InputChannelIds::InputPrimaryChannelId},
    {c_InputMgr::e_InputType::InputType_Effects,  "Effects",    c_InputMgr::e_InputChannelIds::InputSecondaryChannelId},
    {c_InputMgr::e_InputType::InputType_MQTT,     "MQTT",       c_InputMgr::e_InputChannelIds::InputSecondaryChannelId},
    {c_InputMgr::e_InputType::InputType_Alexa,    "Alexa",      c_InputMgr::e_InputChannelIds::InputSecondaryChannelId},
    {c_InputMgr::e_InputType::InputType_Disabled, "Disabled",   c_InputMgr::e_InputChannelIds::InputChannelId_ALL}
};

uint32_t DeltaTime = 0;

#if defined ARDUINO_ARCH_ESP32
#   include <functional>

static TaskHandle_t PollTaskHandle = NULL;
//----------------------------------------------------------------------------
void InputMgrTask (void *arg)
{
    // DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));
    // DEBUG_V(String("Current Task Priority: ") + String(uxTaskPriorityGet(NULL)));
    const uint32_t MinPollTimeMs = FPP_TICKER_PERIOD_MS;
    uint32_t PollStartTime = millis();
    uint32_t PollEndTime = PollStartTime;
    uint32_t PollTime = pdMS_TO_TICKS(MinPollTimeMs);

    while(1)
    {
        DeltaTime = PollEndTime - PollStartTime;

        if (DeltaTime < MinPollTimeMs)
        {
            PollTime = pdMS_TO_TICKS(MinPollTimeMs - DeltaTime);
            vTaskDelay(PollTime);
        }
        else
        {
            // DEBUG_V(String("handle time wrap and long frames. DeltaTime:") + String(DeltaTime));
        }
        FeedWDT();

        PollStartTime = millis();

        InputMgr.Process();
        FeedWDT();

        // record the loop end time
        PollEndTime = millis();
    }
} // InputMgrTask
#else
void TimerPollHandler()
{
    InputMgr.Process();
}
#endif // def ARDUINO_ARCH_ESP32

//-----------------------------------------------------------------------------
// Methods
//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_InputMgr::c_InputMgr ()
{
    ConfigFileName = String (F ("/")) + String (CN_input_config) + F (".json");

    // this gets called pre-setup so there is nothing we can do here.
    int pInputChannelDriversIndex = 0;
    for (auto & CurrentInput : InputChannelDrivers)
    {
        CurrentInput.pInputChannelDriver = nullptr;
        CurrentInput.DriverId = pInputChannelDriversIndex;

        EffectEngineIsConfiguredToRun[pInputChannelDriversIndex] = false;
        ++pInputChannelDriversIndex;
    }

} // c_InputMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the Input channels into a safe state
c_InputMgr::~c_InputMgr ()
{
    // DEBUG_START;

#ifdef ARDUINO_ARCH_ESP32
    if(PollTaskHandle)
    {
        logcon("Stop Input Task");
        vTaskDelete(PollTaskHandle);
        PollTaskHandle = NULL;
    }
#endif // def ARDUINO_ARCH_ESP32

    // delete pInputInstances;
    int pInputChannelDriversIndex = 0;
    for (auto & CurrentInput : InputChannelDrivers)
    {
        if (nullptr != CurrentInput.pInputChannelDriver)
        {
            // the drivers will put the hardware in a safe state
            delete CurrentInput.pInputChannelDriver;
            CurrentInput.pInputChannelDriver = nullptr;
        }
        pInputChannelDriversIndex++;
    }
    // DEBUG_END;

} // ~c_InputMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_InputMgr::Begin (uint32_t BufferSize)
{
    // DEBUG_START;

    // DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));

    InputDataBufferSize = BufferSize;
    // DEBUG_V (String("InputDataBufferSize: ") + String (InputDataBufferSize));

    // prevent recalls
    if (true == HasBeenInitialized) { return; }

    String temp = String (F("Effects Control"));
    ExternalInput.Init (0,0, c_ExternalInput::Polarity_t::ActiveLow, temp);

    // make sure the pointers are set up properly
    for (auto & CurrentInput : InputChannelDrivers)
    {
        InstantiateNewInputChannel(e_InputChannelIds(CurrentInput.DriverId), e_InputType::InputType_Disabled);
        // DEBUG_V ("");
    }

    // load up the configuration from the saved file. This also starts the drivers
    LoadConfig ();

    // CreateNewConfig();

#if defined ARDUINO_ARCH_ESP32
    xTaskCreatePinnedToCore(InputMgrTask, "InputMgrTask", 4096, NULL, INPUTMGR_TASK_PRIORITY, &PollTaskHandle, 1);
#else
    MsTicker.attach_ms (uint32_t (FPP_TICKER_PERIOD_MS), &TimerPollHandler); // Add Timer Function
#endif // ! defined ARDUINO_ARCH_ESP32

    HasBeenInitialized = true;

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
void c_InputMgr::CreateJsonConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;

    // add IM config parameters
    // DEBUG_V ("");

    JsonObject InputMgrButtonData = jsonConfig[IM_EffectsControlButtonName];

    if (!InputMgrButtonData)
    {
        // DEBUG_V ("Create button data");
        InputMgrButtonData = jsonConfig[IM_EffectsControlButtonName].to<JsonObject> ();
    }
    // DEBUG_V ("");

    // PrettyPrint (InputMgrButtonData, String("Before ECB"));
    ExternalInput.GetConfig (InputMgrButtonData);
    // PrettyPrint (InputMgrButtonData, String("After ECB"));

    // DEBUG_V ("");

    // add the channels header
    JsonObject InputMgrChannelsData = jsonConfig[(char*)CN_channels];
    if (!InputMgrChannelsData)
    {
        // add our section header
        // DEBUG_V ("");
        InputMgrChannelsData = jsonConfig[(char*)CN_channels].to<JsonObject> ();
    }

    // add the channel configurations
    // DEBUG_V ("For Each Input Channel");
    for (auto & CurrentChannel : InputChannelDrivers)
    {
        if (nullptr == CurrentChannel.pInputChannelDriver)
        {
            // DEBUG_V ("");
            continue;
        }

        // DEBUG_V (String("Create Section in Config file for the Input channel: '") + CurrentChannel.pInputChannelDriver->GetInputChannelId() + "'");
        // create a record for this channel
        String sChannelId = String (CurrentChannel.pInputChannelDriver->GetInputChannelId ());
        JsonObject ChannelConfigData = InputMgrChannelsData[sChannelId];
        if (!ChannelConfigData)
        {
            // add our section header
            // DEBUG_V ("");
            ChannelConfigData = InputMgrChannelsData[sChannelId].to<JsonObject> ();
        }

        // save the name as the selected channel type
        JsonWrite(ChannelConfigData, CN_type, int (CurrentChannel.pInputChannelDriver->GetInputType ()));

        String DriverTypeId = String (int (CurrentChannel.pInputChannelDriver->GetInputType ()));
        JsonObject ChannelConfigByTypeData = ChannelConfigData[(String (DriverTypeId))];
        if (!ChannelConfigByTypeData)
        {
            // add our section header
            // DEBUG_V ("");
            ChannelConfigByTypeData = ChannelConfigData[DriverTypeId].to<JsonObject> ();
        }

        // ask the channel to add its data to the record
        // DEBUG_V ("Add the Input channel configuration for type: " + DriverTypeId);

        // Populate the driver name
        String DriverName = ""; CurrentChannel.pInputChannelDriver->GetDriverName (DriverName);
        JsonWrite(ChannelConfigByTypeData, CN_type, DriverName);

        CurrentChannel.pInputChannelDriver->GetConfig (ChannelConfigByTypeData);
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
    if (!IsBooting)
    {
        logcon (String (F ("--- WARNING: Creating a new Input Manager configuration Data set ---")));
    }

    // create a place to save the config
    // DEBUG_V(String("Heap: ") + String(ESP.getFreeHeap()));

    JsonDocument JsonConfigDoc;
    // DEBUG_V("");

    JsonObject JsonConfig = JsonConfigDoc[(char*)CN_input_config].to<JsonObject>();
    // DEBUG_V("");

    JsonWrite(JsonConfig, CN_cfgver, CurrentConfigVersion);

    // DEBUG_V ("for each Input type");
    for (int InputTypeId = int (InputType_Start);
         InputTypeId < int (InputType_End);
         ++InputTypeId)
    {
        // DEBUG_V(String("instantiate the Input type: ") + InputTypeId);
        // DEBUG_V("for each input channel");
        for (auto & CurrentInput : InputChannelDrivers)
        {
            // DEBUG_V(String("DriverId: ") + CurrentInput.DriverId);
            InstantiateNewInputChannel (e_InputChannelIds(CurrentInput.DriverId), e_InputType(InputTypeId), false);
        }// end for each interface

        // DEBUG_V ("collect the config data");
        CreateJsonConfig (JsonConfig);

        // DEBUG_V ("");

    } // end for each Input type

    // DEBUG_V ("leave the Inputs disabled");
    for (auto & CurrentInput : InputChannelDrivers)
    {
        InstantiateNewInputChannel (e_InputChannelIds (CurrentInput.DriverId), e_InputType::InputType_Disabled, false);
    }// end for each interface

    // DEBUG_V ("");

    // Record the default configuration
    CreateJsonConfig (JsonConfig);

    SetConfig(JsonConfigDoc);

    // logcon (String (F ("--- WARNING: Creating a new Input Manager configuration Data set - Done ---")));
    // DEBUG_END;

} // CreateNewConfig

//-----------------------------------------------------------------------------
void c_InputMgr::GetConfig (byte * Response, uint32_t maxlen)
{
    // DEBUGSTART;

    FileMgr.ReadFlashFile (ConfigFileName, Response, maxlen);
    // DEBUGV (String ("TempConfigData: ") + TempConfigData);

    // DEBUGEND;
} // GetConfig

//-----------------------------------------------------------------------------
void c_InputMgr::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject InputButtonStatus = jsonStatus[F ("inputbutton")].to<JsonObject> ();
    ExternalInput.GetStatistics (InputButtonStatus);

    JsonArray InputStatus = jsonStatus[F ("input")].to<JsonArray> ();
    for (auto & CurrentInput : InputChannelDrivers)
    {
        if(nullptr == CurrentInput.pInputChannelDriver)
        {
            continue;
        }

        JsonObject channelStatus = InputStatus.add<JsonObject> ();
        CurrentInput.pInputChannelDriver->GetStatus (channelStatus);
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
*/
void c_InputMgr::InstantiateNewInputChannel (e_InputChannelIds ChannelIndex, e_InputType NewInputChannelType, bool StartDriver)
{
    // DEBUG_START;
    // DEBUG_V (String ("       ChannelIndex: ") + String (ChannelIndex));
    // DEBUG_V (String ("NewInputChannelType: ") + String (NewInputChannelType));
    // DEBUG_V (String ("        StartDriver: ") + String (StartDriver));
    // DEBUG_V (String ("InputDataBufferSize: ") + String (InputDataBufferSize));

    do // once
    {
        // is there an existing driver?
        if (nullptr != InputChannelDrivers[ChannelIndex].pInputChannelDriver)
        {
            // DEBUG_V(String("InputChannelDrivers[ChannelIndex].pInputChannelDriver->GetInputType () '") + InputChannelDrivers[ChannelIndex].pInputChannelDriver->GetInputType() + String("'"));
            // DEBUG_V (String("NewInputChannelType '") + int(NewInputChannelType) + "'");

            // DEBUG_V ("does the driver need to change?");
            if (InputChannelDrivers[ChannelIndex].pInputChannelDriver->GetInputType () == NewInputChannelType)
            {
                // DEBUG_V ("nothing to change");
                break;
            }
            String DriverName;
            InputChannelDrivers[ChannelIndex].pInputChannelDriver->GetDriverName (DriverName);
            rebootNeeded |= InputChannelDrivers[ChannelIndex].pInputChannelDriver->isShutDownRebootNeeded();
            // DEBUG_V (String ("rebootNeeded: ") + String (rebootNeeded));
            if (!IsBooting) {
                logcon (String(F("Shutting Down '")) + DriverName + String(F("' on Input: ")) + String(ChannelIndex));
            }

            delete InputChannelDrivers[ChannelIndex].pInputChannelDriver;
            // DEBUG_V ();
            InputChannelDrivers[ChannelIndex].pInputChannelDriver = nullptr;

            // DEBUG_V ("");
        } // end there is an existing driver
        // DEBUG_V ();

        switch (NewInputChannelType)
        {
            case e_InputType::InputType_Disabled:
            {
                if (!IsBooting)
                {
                    logcon (String (F ("Disabled Input type for channel '")) + ChannelIndex + "'.");
                }
                InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }

            case e_InputType::InputType_E1_31:
            {
                if (InputTypeIsAllowedOnChannel (InputType_E1_31, ChannelIndex))
                {
                    if (!IsBooting)
                    {
                        logcon (String (F ("Starting E1.31 for channel '")) + ChannelIndex + "'.");
                    }
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputE131 (ChannelIndex, InputType_E1_31, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_Effects:
            {
                if (InputTypeIsAllowedOnChannel (InputType_Effects, ChannelIndex))
                {
                    if (!IsBooting)
                    {
                        logcon (String (F ("Starting Effects Engine for channel '")) + ChannelIndex + "'.");
                    }
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputEffectEngine (ChannelIndex, InputType_Effects, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_MQTT:
            {
                if (InputTypeIsAllowedOnChannel (InputType_MQTT, ChannelIndex))
                {
                    if (!IsBooting)
                    {
                        logcon (String (F ("Starting MQTT for channel '")) + ChannelIndex + "'.");
                    }
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputMQTT (ChannelIndex, InputType_MQTT, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_Alexa:
            {
                if (InputTypeIsAllowedOnChannel (InputType_Alexa, ChannelIndex))
                {
                    if (!IsBooting)
                    {
                        logcon (String (F ("Starting Alexa for channel '")) + ChannelIndex + "'.");
                    }
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputAlexa (ChannelIndex, InputType_Alexa, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                }
                break;
            }

            case e_InputType::InputType_DDP:
            {
                if (InputTypeIsAllowedOnChannel (InputType_DDP, ChannelIndex))
                {
                    if (!IsBooting)
                    {
                        logcon (String (F ("Starting DDP for channel '")) + ChannelIndex + "'.");
                    }
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDDP (ChannelIndex, InputType_DDP, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                }
                break;
            }

#ifdef SUPPORT_FPP
            case e_InputType::InputType_FPP:
            {
                if (InputTypeIsAllowedOnChannel (InputType_FPP, ChannelIndex))
                {
                    if (!IsBooting)
                    {
                        logcon (String (F ("Starting FPP Remote for channel '")) + ChannelIndex + "'.");
                    }
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputFPPRemote (ChannelIndex, InputType_FPP, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                }
                break;
            }
#endif // def SUPPORT_FPP

            case e_InputType::InputType_Artnet:
            {
                if (InputTypeIsAllowedOnChannel (InputType_Artnet, ChannelIndex))
                {
                    if (!IsBooting)
                    {
                        logcon (String (F ("Starting Artnet for channel '")) + ChannelIndex + "'.");
                    }
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputArtnet (ChannelIndex, InputType_Artnet, InputDataBufferSize);
                    // DEBUG_V ("");
                }
                else
                {
                    InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                }
                break;
            }

            default:
            {
                if (!IsBooting)
                {
                    logcon (CN_stars + String (F (" Unknown Input type for channel '")) + ChannelIndex + String(F ("'. Using disabled. ")) + CN_stars);
                }
                InputChannelDrivers[ChannelIndex].pInputChannelDriver = new c_InputDisabled (ChannelIndex, InputType_Disabled, InputDataBufferSize);
                // DEBUG_V ("");
                break;
            }
        } // end switch (NewChannelType)

        // DEBUG_V ("");
        //String sDriverName;
        //pInputChannelDrivers[ChannelIndex]->GetDriverName (sDriverName);
        //Serial.println (String (CN_stars) + " '" + sDriverName + F("' Initialization for input: '") + String(ChannelIndex) + "' " + CN_stars);
        if (StartDriver)
        {
            if(nullptr != InputChannelDrivers[ChannelIndex].pInputChannelDriver) 
            {
	            // DEBUG_V (String ("StartDriver: ") + String (StartDriver));
	            InputChannelDrivers[ChannelIndex].pInputChannelDriver->Begin ();
	            // DEBUG_V ("");
	            InputChannelDrivers[ChannelIndex].pInputChannelDriver->SetBufferInfo (InputDataBufferSize);
            }
        }
        // DEBUG_V ("");

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

    ConfigLoadNeeded = NO_CONFIG_NEEDED;
    configInProgress = true;
    // try to load and process the config file
    if (!FileMgr.LoadFlashFile (ConfigFileName, [this](JsonDocument & JsonConfigDoc)
        {
            // DEBUG_V ("");
            JsonObject JsonConfig = JsonConfigDoc.as<JsonObject> ();
            // DEBUG_V ("");
            this->ProcessJsonConfig (JsonConfig);
            // DEBUG_V ("");
        }))
    {
        if (IsBooting)
        {
            // create a config file with default values
            // DEBUG_V ("");
            CreateNewConfig ();
        }
        else
        {
            logcon (CN_stars + String (F (" Error loading Input Manager Config File. Rebooting ")) + CN_stars);
            RequestReboot(100000);
        }
    }

    configInProgress = false;
    // DEBUG_END;

} // LoadConfig

//-----------------------------------------------------------------------------
///< Called from loop(), renders Input data
void c_InputMgr::Process ()
{
    // DEBUG_START;
    // do we need to save the current config?

    do // once
    {
        if (configInProgress || PauseProcessing)
        {
            // prevent calls to process when we are doing a long operation
            break;
        }

        ExternalInput.Poll ();

        if (NO_CONFIG_NEEDED != ConfigLoadNeeded)
        {
            if(abs(now() - ConfigLoadNeeded) > LOAD_CONFIG_DELAY)
            {
                // DEBUG_V ("Reload the config");
                LoadConfig ();
                // DEBUG_V ("End Save Config");
            }
        }

        if(RebootInProgress())
        {
            break;
        }

        bool aBlankTimerIsRunning = false;
        for (auto & CurrentInput : InputChannelDrivers)
        {
            if(nullptr == CurrentInput.pInputChannelDriver || aBlankTimerIsRunning)
            {
                continue;
            }
            // DEBUG_V(String("pInputChannelDriver:Start 0x") + String(uint32_t(CurrentInput.pInputChannelDriver), HEX));
            CurrentInput.pInputChannelDriver->Process ();
            // DEBUG_V(String("pInputChannelDriver:Done  0x") + String(uint32_t(CurrentInput.pInputChannelDriver), HEX));

            if (!BlankTimerHasExpired (CurrentInput.pInputChannelDriver->GetInputChannelId()))
            {
                // DEBUG_V (String ("Blank Timer is running: ") + String (CurrentInput.pInputChannelDriver->GetInputChannelId ()));
                aBlankTimerIsRunning = true;
                // break;
            }
        }

        if (false == aBlankTimerIsRunning && config.BlankDelay != 0)
        {
            // DEBUG_V("Clear Input Buffer");
            OutputMgr.ClearBuffer ();
            RestartBlankTimer (InputSecondaryChannelId);
        } // ALL blank timers have expired

        if (rebootNeeded)
        {
            // DEBUG_V("Requesting Reboot");
            RequestReboot(100000);
        }

    } while (false);

    // DEBUG_END;
} // Process

//-----------------------------------------------------------------------------
void c_InputMgr::ProcessButtonActions (c_ExternalInput::InputValue_t value)
{
    // DEBUG_START;

    for(auto & CurrentInputChannel : InputChannelDrivers)
    {
        if(nullptr == CurrentInputChannel.pInputChannelDriver) 
        {
            continue;
        }
        CurrentInputChannel.pInputChannelDriver->ProcessButtonActions(value);
    }

    // DEBUG_END;

} // ProcessButtonActions

//-----------------------------------------------------------------------------
bool c_InputMgr::FindJsonChannelConfig (JsonObject& jsonConfig,
                                         e_InputChannelIds ChanId,
                                         JsonObject& ChanConfig)
{
    // DEBUG_START;
    bool Response = false;
    // DEBUG_V ();

    // PrettyPrint(jsonConfig, "ProcessJsonConfig");

    do // once
    {
        JsonObject InputChannelMgrData = jsonConfig[(char*)CN_input_config];
        if (!InputChannelMgrData)
        {
            logcon (String (F ("No Input Interface Settings Found. Using Defaults")));
            PrettyPrint (jsonConfig, String(F ("c_InputMgr::ProcessJsonConfig")));
            break;
        }
        // DEBUG_V ("");

        uint8_t TempVersion = !InputChannelMgrData;
        setFromJSON (TempVersion, InputChannelMgrData, CN_cfgver);

        // DEBUG_V (String ("TempVersion: ") + String (TempVersion));
        // DEBUG_V (String ("CurrentConfigVersion: ") + String (CurrentConfigVersion));
        // PrettyPrint (InputChannelMgrData, "Output Config");

        if (TempVersion != CurrentConfigVersion)
        {
            logcon (String (F ("Incorrect Version found. Using existing/default config.")));
            // break;
        }

        // extract my own config data here
        JsonObject InputButtonConfig = InputChannelMgrData[IM_EffectsControlButtonName];
        if (InputButtonConfig)
        {
            // DEBUG_V ("Found Input Button Config");
            ExternalInput.ProcessConfig (InputButtonConfig);
        }
        else
        {
            logcon (String (F ("No Input Button Settings Found. Using Defaults")));
        }

        // do we have a channel configuration array?
        JsonObject InputChannelArray = InputChannelMgrData[(char*)CN_channels];
        if (!InputChannelArray)
        {
            // if not, flag an error and stop processing
            logcon (String (F ("No Input Channel Settings Found. Using Defaults")));
            break;
        }
        // DEBUG_V ("");

        // get access to the channel config
        ChanConfig = InputChannelArray[String(ChanId)];
        if (!ChanConfig)
        {
            // if not, flag an error and stop processing
            logcon (String (F ("No Input Settings Found for Channel '")) + ChanId + String (F ("'. Using Defaults")));
            continue;
        }
        // DEBUG_V ();

        // all went well
        Response = true;

    } while (false);

    // DEBUG_END;
    return Response;

} // FindChannelJsonConfig

//-----------------------------------------------------------------------------
/*
    check the contents of the config and send
    the proper portion of the config to the currently instantiated channels
*/
bool c_InputMgr::ProcessJsonConfig (JsonObject & jsonConfig)
{
    // DEBUG_START;
    bool Response = false;

    // DEBUG_V ("InputDataBufferSize: " + String (InputDataBufferSize));

    do // once
    {
        // for each Input channel
        for (uint32_t ChannelIndex = uint32_t (InputChannelId_Start);
            ChannelIndex < uint32_t (InputChannelId_End);
            ChannelIndex++)
        {
            JsonObject InputChannelConfig;
            // DEBUG_V ("");
            if(!FindJsonChannelConfig(jsonConfig, e_InputChannelIds(ChannelIndex), InputChannelConfig))
            {
                // DEBUG_V("Did not find the desired channel configuration");
                continue;
            }
            // set a default value for channel type
            uint32_t ChannelType = uint32_t (InputType_Default);
            setFromJSON (ChannelType, InputChannelConfig, CN_type);
            // DEBUG_V ("");

            // is it a valid / supported channel type
            if (/*(ChannelType < uint32_t (InputType_Start)) ||*/ (ChannelType >= uint32_t (InputType_End)))
            {
                // if not, flag an error and move on to the next channel
                logcon (String (MN_19) + ChannelType + String (MN_20) + ChannelIndex + String(MN_03));
                InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType::InputType_Disabled);
                continue;
            }
            // DEBUG_V ("");

            // do we have a configuration for the channel type?
            JsonObject InputChannelDriverConfig = InputChannelConfig[String (ChannelType)];
            if (!InputChannelDriverConfig)
            {
                // if not, flag an error and stop processing
                logcon (String (F ("No Input Settings Found for Channel '")) + ChannelIndex + String (F ("'. Using Defaults")));
                InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType::InputType_Disabled);
                continue;
            }
            // DEBUG_V ("");

            // make sure the proper Input type is running
            InstantiateNewInputChannel (e_InputChannelIds (ChannelIndex), e_InputType (ChannelType));
            // DEBUG_V (String ("Response: ") + Response);

            if(nullptr != InputChannelDrivers[ChannelIndex].pInputChannelDriver) 
            {
                EffectEngineIsConfiguredToRun[ChannelIndex] = (e_InputType::InputType_Effects == InputChannelDrivers[ChannelIndex].pInputChannelDriver->GetInputType()) ? true : false;

                // send the config to the driver. At this level we have no idea what is in it
                InputChannelDrivers[ChannelIndex].pInputChannelDriver->SetConfig (InputChannelDriverConfig);
                // DEBUG_V (String("Response: ") + Response);
            }
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
/* Sets the configuration for the current active ports:
*
*   WARNING: This runs in the Web server context and cannot access the File system
*/
void c_InputMgr::SetConfig (const char * NewConfigData)
{
    // DEBUG_START;

    if (true == FileMgr.SaveFlashFile (ConfigFileName, NewConfigData))
    {
        // DEBUG_V (String("NewConfigData: ") + NewConfigData);
        // FileMgr logs for us
        // logcon (CN_stars + String (F (" Saved Input Manager Config File. ")) + CN_stars);

        ScheduleLoadConfig();
    } // end we saved the config
    else
    {
        logcon (CN_stars + String (F (" Error Saving Input Manager Config File ")) + CN_stars);
    }

    // DEBUG_END;

} // SetConfig

//-----------------------------------------------------------------------------
/* Sets the configuration for the current active ports:
 *
 *   WARNING: This runs in the Web server context and cannot access the File system
 */
void c_InputMgr::SetConfig(JsonDocument & NewConfigData)
{
    // DEBUG_START;

    if (true == FileMgr.SaveFlashFile(ConfigFileName, NewConfigData))
    {
        // FileMgr logs for us
        // logcon (CN_stars + String (F (" Saved Input Manager Config File. ")) + CN_stars);

        ScheduleLoadConfig();
    } // end we saved the config
    else
    {
        logcon(CN_stars + String(F(" Error Saving Input Manager Config File ")) + CN_stars);
    }

    // DEBUG_END;

} // SetConfig

//-----------------------------------------------------------------------------
void c_InputMgr::SetBufferInfo (uint32_t BufferSize)
{
    // DEBUG_START;

    InputDataBufferSize = BufferSize;

    // DEBUG_V ("InputDataBufferSize: " + String (InputDataBufferSize));

    // pass through each active interface and set the buffer info
            // for each Input channel
    for (int ChannelIndex = int (InputChannelId_Start);
        ChannelIndex < int (InputChannelId_End);
        ChannelIndex++)
    {
        if (nullptr != InputChannelDrivers[ChannelIndex].pInputChannelDriver)
        {
            InputChannelDrivers[ChannelIndex].pInputChannelDriver->SetBufferInfo (InputDataBufferSize);
        }
    } // end for each channel

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
void c_InputMgr::SetOperationalState (bool ActiveFlag)
{
    // DEBUG_START;
    // DEBUG_V(String("ActiveFlag: ") + String(ActiveFlag));

    PauseProcessing = !ActiveFlag;

    // pass through each active interface and set the active state
    for (auto & InputChannel : InputChannelDrivers)
    {
        // DEBUG_V ("");
        if (nullptr != InputChannel.pInputChannelDriver)
        {
            InputChannel.pInputChannelDriver->SetOperationalState(ActiveFlag);
            // DEBUG_V ("");
        }
    }

    // DEBUG_END;

} // SetOutputState

//-----------------------------------------------------------------------------
void c_InputMgr::NetworkStateChanged (bool _IsConnected)
{
    // DEBUG_START;

    IsConnected = _IsConnected;

    if (HasBeenInitialized)
    {
        // pass through each active interface and notify WiFi changed state
        for (auto & InputChannel : InputChannelDrivers)
        {
            // DEBUG_V (String ("pInputChannel: 0x") + String (uint32_t (), HEX));
            InputChannel.pInputChannelDriver->NetworkStateChanged (IsConnected);
        }
    }

    // DEBUG_END;
} // NetworkStateChanged

// create a global instance of the Input channel factory
c_InputMgr InputMgr;
