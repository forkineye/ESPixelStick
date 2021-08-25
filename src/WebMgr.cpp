/*
* WebMgr.cpp - Output Management class
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
*/

#include "ESPixelStick.h"

#include "output/OutputMgr.hpp"
#include "input/InputMgr.hpp"
#include "service/FPPDiscovery.h"
#include "WiFiMgr.hpp"

#include "WebMgr.hpp"
#include "FileMgr.hpp"
#include <Int64String.h>

#include <FS.h>

#include <time.h>
#include <sys/time.h>

#ifdef ARDUINO_ARCH_ESP8266
#	include <LittleFS.h>
#   define LITTLEFS LittleFS
#elif defined ARDUINO_ARCH_ESP32
#	include <LITTLEFS.h>
#else
#	error "Unsupported CPU type."
#endif

// #define ESPALEXA_DEBUG
#define ESPALEXA_MAXDEVICES 2
#define ESPALEXA_ASYNC //it is important to define this before #include <Espalexa.h>!
#include <Espalexa.h>

const uint8_t HTTP_PORT = 80;      ///< Default web server port

static Espalexa         espalexa;
static EFUpdate         efupdate; /// EFU Update Handler
static AsyncWebServer   webServer (HTTP_PORT);  // Web Server
static AsyncWebSocket   webSocket ("/ws");      // Web Socket Plugin

//-----------------------------------------------------------------------------
void PrettyPrint (JsonArray& jsonStuff, String Name)
{
    // DEBUG_V ("---------------------------------------------");
    LOG_PORT.println (String (F ("---- Pretty Print: '")) + Name + "'");
    serializeJson (jsonStuff, LOG_PORT);
    LOG_PORT.println ("");
    // DEBUG_V ("---------------------------------------------");

} // PrettyPrint

//-----------------------------------------------------------------------------
void PrettyPrint (JsonObject& jsonStuff, String Name)
{
    // DEBUG_V ("---------------------------------------------");
    LOG_PORT.println (String (F ("---- Pretty Print: '")) + Name + "'");
    serializeJson (jsonStuff, LOG_PORT);
    LOG_PORT.println ("");
    // DEBUG_V ("---------------------------------------------");

} // PrettyPrint

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_WebMgr::c_WebMgr ()
{
    // this gets called pre-setup so there is nothing we can do here.
} // c_WebMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_WebMgr::~c_WebMgr ()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_WebMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_WebMgr::Begin (config_t* /* NewConfig */)
{
    // DEBUG_START;

    if (WiFiMgr.IsWiFiConnected ())
    {
        init ();
    }

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
// Configure and start the web server
void c_WebMgr::NetworkStateChanged (bool NewNetworkState)
{
    // DEBUG_START;

    if (true == NewNetworkState)
    {
        init ();
    }

    // DEBUG_END;
} // NetworkStateChanged

//-----------------------------------------------------------------------------
// Configure and start the web server
void c_WebMgr::init ()
{
    // DEBUG_START;
    // Add header for SVG plot support?
    DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Origin"),  "*");
    DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Headers"), "append, delete, entries, foreach, get, has, keys, set, values, Authorization, Content-Type, Content-Range, Content-Disposition, Content-Description, cache-control, x-requested-with");
    DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Methods"), "GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH");

    // Setup WebSockets
    webSocket.onEvent ([this](AsyncWebSocket* server, AsyncWebSocketClient * client, AwsEventType type, void* arg, uint8_t * data, size_t len)
        {
            this->onWsEvent (server, client, type, arg, data, len);
        });
    webServer.addHandler (&webSocket);

    // Heap status handler
    webServer.on ("/heap", HTTP_GET, [](AsyncWebServerRequest* request)
        {
            request->send (200, CN_textSLASHplain, String (ESP.getFreeHeap ()).c_str());
        });

    // JSON Config Handler
    webServer.on ("/conf", HTTP_GET, [this](AsyncWebServerRequest* request)
        {
            // DEBUG_V (CN_Heap_colon + String (ESP.getFreeHeap ()));
            this->GetConfiguration ();
            request->send (200, "text/json", WebSocketFrameCollectionBuffer);
            // DEBUG_V (CN_Heap_colon + String (ESP.getFreeHeap ()));
        });

    // Firmware upload handler
    webServer.on ("/updatefw", HTTP_POST, [](AsyncWebServerRequest* request)
        {
            webSocket.textAll ("X6");
        }, [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {WebMgr.FirmwareUpload (request, filename, index, data, len,  final); }).setFilter (ON_STA_FILTER);

    // URL's needed for FPP Connect fseq uploading and querying
    webServer.on ("/fpp", HTTP_GET,
        [](AsyncWebServerRequest* request)
        {
        FPPDiscovery.ProcessGET(request);
        });

    webServer.on ("/fpp", HTTP_POST | HTTP_PUT,
        [](AsyncWebServerRequest* request)
        {
            FPPDiscovery.ProcessPOST(request);
        },

        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            FPPDiscovery.ProcessFile(request, filename, index, data, len, final);
        },

        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            FPPDiscovery.ProcessBody(request, data, len, index, total);
        });

    // URL that FPP's status pages use to grab JSON about the current status, what's playing, etc...
    // This can be used to mimic the behavior of atual FPP remotes
    webServer.on ("/fppjson.php", HTTP_GET, [](AsyncWebServerRequest* request)
        {
            FPPDiscovery.ProcessFPPJson(request);
        });

#ifdef USE_REST
    // URL's needed for FPP Connect fseq uploading and querying
    webServer.on ("/rest", HTTP_GET,
        [this](AsyncWebServerRequest* request)
        {
            RestProcessGET (request);
        });

    webServer.on ("/rest", HTTP_POST | HTTP_PUT,
        [this](AsyncWebServerRequest* request)
        {
            RestProcessPOST (request);
        },

        [this] (AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final)
        {
            RestProcessFile (request, filename, index, data, len, final);
        },

        [this] (AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
        {
            RestProcessBody (request, data, len, index, total);
        });
#endif // def USE_REST

    // Static Handler
    webServer.serveStatic ("/", LITTLEFS, "/www/").setDefaultFile ("index.html");

    // if the client posts to the upload page
    webServer.on ("/upload", HTTP_POST | HTTP_PUT | HTTP_OPTIONS,
        [](AsyncWebServerRequest * request)
        {
            // DEBUG_V ("Got upload post request");
            if (true == FileMgr.SdCardIsInstalled())
            {
                // Send status 200 (OK) to tell the client we are ready to receive
                request->send (200);
            }
            else
            {
                request->send (404, CN_textSLASHplain, "Page Not found");
            }
        },

        [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final)
        {
            // DEBUG_V ("Got process FIle request");
            // DEBUG_V (String ("Got process File request: name: ")  + filename);
            // DEBUG_V (String ("Got process File request: index: ") + String (index));
            // DEBUG_V (String ("Got process File request: len:   ") + String (len));
            // DEBUG_V (String ("Got process File request: final: ") + String (final));
            if (true == FileMgr.SdCardIsInstalled())
            {
                this->handleFileUpload (request, filename, index, data, len, final); // Receive and save the file
            }
            else
            {
                request->send (404, CN_textSLASHplain, "Page Not found");
            }
        },

        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
        {
            // DEBUG_V (String ("Got process Body request: index: ") + String (index));
            // DEBUG_V (String ("Got process Body request: len:   ") + String (len));
            // DEBUG_V (String ("Got process Body request: total: ") + String (total));
            request->send (404, CN_textSLASHplain, "Page Not found");
        }
    );

    webServer.onNotFound ([this](AsyncWebServerRequest* request)
    {
        // DEBUG_V ("onNotFound");
        if (true == this->IsAlexaCallbackValid())
        {
            // DEBUG_V ("IsAlexaCallbackValid == true");
            if (!espalexa.handleAlexaApiCall (request)) //if you don't know the URI, ask espalexa whether it is an Alexa control request
            {
                // DEBUG_V ("Alexa Callback could not resolve the request");
                request->send (404, CN_textSLASHplain, "Page Not found");
            }
        }
        else
        {
            // DEBUG_V ("IsAlexaCallbackValid == false");
            request->send (404, CN_textSLASHplain, "Page Not found");
        }
    });

    //give espalexa a pointer to the server object so it can use your server instead of creating its own
    espalexa.begin (&webServer);

    // webServer.begin ();

    pAlexaDevice = new EspalexaDevice (String (F ("ESP")), [this](EspalexaDevice* pDevice)
        {
            this->onAlexaMessage (pDevice);

        }, EspalexaDeviceType::extendedcolor);

    pAlexaDevice->setName (config.id);
    espalexa.addDevice (pAlexaDevice);
    espalexa.setDiscoverable ((nullptr != pAlexaCallback) ? true : false);

    LOG_PORT.println (String (F ("- Web Server started on port ")) + HTTP_PORT);

    // DEBUG_END;
}

//-----------------------------------------------------------------------------
void c_WebMgr::handleFileUpload (AsyncWebServerRequest* request,
                                 String filename,
                                 size_t index,
                                 uint8_t * data,
                                 size_t len,
                                 bool final)
{
    // DEBUG_START;

    FileMgr.handleFileUpload (filename, index, data, len, final);

    // DEBUG_END;
} // handleFileUpload

//-----------------------------------------------------------------------------
void c_WebMgr::RegisterAlexaCallback (DeviceCallbackFunction cb)
{
    // DEBUG_START;

    pAlexaCallback = cb;
    espalexa.setDiscoverable (IsAlexaCallbackValid());

    // DEBUG_END;
} // RegisterAlexaCallback

//-----------------------------------------------------------------------------
void c_WebMgr::onAlexaMessage (EspalexaDevice* dev)
{
    // DEBUG_START;
    if (true == IsAlexaCallbackValid())
    {
        // DEBUG_V ("");

        pAlexaCallback (dev);
    }
    // DEBUG_END;

} // onAlexaMessage

//-----------------------------------------------------------------------------
/*
    Gather config data from the various config sources and send it to the web page.
*/
void c_WebMgr::GetConfiguration ()
{
    extern void GetConfig (JsonObject & json);
    // DEBUG_START;

    DynamicJsonDocument webJsonDoc (4096);

    JsonObject JsonSystemConfig = webJsonDoc.createNestedObject (CN_system);
    GetConfig (JsonSystemConfig);
    // DEBUG_V ("");

    // JsonObject JsonOutputConfig = webJsonDoc.createNestedObject (F ("outputs"));
    // OutputMgr.GetConfig (JsonOutputConfig);
    // DEBUG_V ("");

    // JsonObject JsonInputConfig = webJsonDoc.createNestedObject (F ("inputs"));
    // InputMgr.GetConfig (JsonInputConfig);

    // now make it something we can transmit
    serializeJson (webJsonDoc, WebSocketFrameCollectionBuffer);

    // DEBUG_END;

} // GetConfiguration

//-----------------------------------------------------------------------------
void c_WebMgr::GetDeviceOptions ()
{
    // DEBUG_START;
#ifdef SUPPORT_DEVICE_OPTION_LIST
    // set up a framework to get the option data
    DynamicJsonDocument webJsonDoc (2048);

    if (0 == webJsonDoc.capacity ())
    {
        LOG_PORT.println (F ("ERROR: Failed to allocate memory for the GetDeviceOptions web request response."));
    }

    // DEBUG_V ("");
    JsonObject WebOptions = webJsonDoc.createNestedObject (F ("options"));
    JsonObject JsonDeviceOptions = WebOptions.createNestedObject (CN_device);
    // DEBUG_V("");

    // PrettyPrint (WebOptions);

    // PrettyPrint (WebOptions);

    // now make it something we can transmit
    size_t msgOffset = strlen (WebSocketFrameCollectionBuffer);
    serializeJson (WebOptions, &WebSocketFrameCollectionBuffer[msgOffset], (sizeof (WebSocketFrameCollectionBuffer) - msgOffset));
#endif // def SUPPORT_DEVICE_OPTION_LIST

    // DEBUG_END;

} // GetDeviceOptions

//-----------------------------------------------------------------------------
/// Handle Web Service events
/** Handles all Web Service event types and performs initial parsing of Web Service event data.
 * Text messages that start with 'X' are treated as "Simple" format messages, else they're parsed as JSON.
 */
void c_WebMgr::onWsEvent (AsyncWebSocket* server, AsyncWebSocketClient * client,
    AwsEventType type, void * arg, uint8_t * data, size_t len)
{
    // DEBUG_START;
    // DEBUG_V (CN_Heap_colon + String (ESP.getFreeHeap ()));

    switch (type)
    {
        case WS_EVT_DATA:
        {
            // DEBUG_V ("");

            AwsFrameInfo* MessageInfo = static_cast<AwsFrameInfo*>(arg);
            // DEBUG_V (String (F ("               len: ")) + len);
            // DEBUG_V (String (F ("MessageInfo->index: ")) + int64String (MessageInfo->index));
            // DEBUG_V (String (F ("  MessageInfo->len: ")) + int64String (MessageInfo->len));
            // DEBUG_V (String (F ("MessageInfo->final: ")) + String (MessageInfo->final));

            // only process text messages
            if (MessageInfo->opcode != WS_TEXT)
            {
                LOG_PORT.println (F ("-- Ignore binary message --"));
                break;
            }
            // DEBUG_V ("");

            // is this a message start?
            if (0 == MessageInfo->index)
            {
                // clear the string we are building
                memset (WebSocketFrameCollectionBuffer, 0x0, sizeof (WebSocketFrameCollectionBuffer));
                // DEBUG_V ("");
            }
            // DEBUG_V ("");

            // will the message fit into our buffer?
            if (WebSocketFrameCollectionBufferSize < (MessageInfo->index + len))
            {
                // message wont fit. Dont save any of it
                LOG_PORT.println (CN_stars + String (F (" WebIO::onWsEvent(): Error: Incoming message is TOO long.")));
                break;
            }

            // add the current data to the aggregate message
            memcpy (&WebSocketFrameCollectionBuffer[MessageInfo->index], data, len);

            // is the message complete?
            if ((MessageInfo->index + len) != MessageInfo->len)
            {
                // DEBUG_V ("The message is not yet complete");
                break;
            }
            // DEBUG_V ("");

            // did we get the final part of the message?
            if (!MessageInfo->final)
            {
                // DEBUG_V ("message is not terminated yet");
                break;
            }

            // DEBUG_V (WebSocketFrameCollectionBuffer);
            // message is all here. Process it

            if (WebSocketFrameCollectionBuffer[0] == 'X')
            {
                // DEBUG_V ("");
                ProcessXseriesRequests (client);
                break;
            }

            if (WebSocketFrameCollectionBuffer[0] == 'V')
            {
                // DEBUG_V ("");
                ProcessVseriesRequests (client);
                break;
            }

            if (WebSocketFrameCollectionBuffer[0] == 'G')
            {
                // DEBUG_V ("");
                ProcessGseriesRequests (client);
                break;
            }

            OutputMgr.PauseOutputs ();

            // convert the input data into a json structure (use json read only mode)
            size_t docSize = strlen ((const char*)(&WebSocketFrameCollectionBuffer[0])) * 3;
            DynamicJsonDocument webJsonDoc (docSize);
            DeserializationError error = deserializeJson (webJsonDoc, (const char *)(&WebSocketFrameCollectionBuffer[0]));

            // DEBUG_V ("");
            if (error)
            {
                LOG_PORT.println (CN_stars + String (F (" WebIO::onWsEvent(): Parse Error: ")) + error.c_str ());
                LOG_PORT.println (WebSocketFrameCollectionBuffer);
                break;
            }
            // DEBUG_V ("");

            ProcessReceivedJsonMessage (webJsonDoc, client);
            // DEBUG_V ("");

            break;
        } // case WS_EVT_DATA:

        case WS_EVT_CONNECT:
        {
            LOG_PORT.println (String (F ("* WS Connect - ")) + client->id ());
            break;
        } // case WS_EVT_CONNECT:

        case WS_EVT_DISCONNECT:
        {
            LOG_PORT.println (String (F ("* WS Disconnect - ")) + client->id ());
            break;
        } // case WS_EVT_DISCONNECT:

        case WS_EVT_PONG:
        {
            LOG_PORT.println (F ("* WS PONG *"));
            break;
        } // case WS_EVT_PONG:

        case WS_EVT_ERROR:
        default:
        {
            LOG_PORT.println (F ("** WS ERROR **"));
            break;
        }
    } // end switch (type)

    // DEBUG_V (CN_Heap_colon + String (ESP.getFreeHeap ()));

    // DEBUG_END;

} // onEvent

//-----------------------------------------------------------------------------
/// Process simple format 'X' messages
void c_WebMgr::ProcessXseriesRequests (AsyncWebSocketClient * client)
{
    // DEBUG_START;

    switch (WebSocketFrameCollectionBuffer[1])
    {
        case SimpleMessage::GET_STATUS:
        {
            // DEBUG_V ("");
            ProcessXJRequest (client);
            break;
        } // end case SimpleMessage::GET_STATUS:

        case SimpleMessage::GET_ADMIN:
        {
            // DEBUG_V ("");
            ProcessXARequest (client);
            break;
        } // end case SimpleMessage::GET_ADMIN:

        case SimpleMessage::DO_RESET:
        {
            // DEBUG_V ("");
            extern bool reboot;
            reboot = true;
            break;
        } // end case SimpleMessage::DO_RESET:

        case SimpleMessage::DO_FACTORYRESET:
        {
            extern void DeleteConfig ();
            // DEBUG_V ("");
            InputMgr.DeleteConfig ();
            OutputMgr.DeleteConfig ();
            DeleteConfig ();

            // DEBUG_V ("");
            extern bool reboot;
            reboot = true;
            break;
        } // end case SimpleMessage::DO_RESET:

        default:
        {
            LOG_PORT.println (String (F ("ERROR: Unhandled request: ")) + WebSocketFrameCollectionBuffer);
            client->text ((String (F ("{\"Error\":Error"))).c_str());
            break;
        }

    } // end switch (data[1])

    // DEBUG_END;

} // ProcessXseriesRequests

//-----------------------------------------------------------------------------
void c_WebMgr::ProcessXARequest (AsyncWebSocketClient* client)
{
    // DEBUG_START;

    DynamicJsonDocument webJsonDoc (1024);
    JsonObject jsonAdmin = webJsonDoc.createNestedObject (F ("admin"));

    jsonAdmin["version"] = VERSION;
    jsonAdmin["built"] = BUILD_DATE;
    jsonAdmin["realflashsize"] = String (ESP.getFlashChipSize ());
#ifdef ARDUINO_ARCH_ESP8266
    jsonAdmin["flashchipid"] = String (ESP.getChipId (), HEX);
#else
    jsonAdmin["flashchipid"] = int64String (ESP.getEfuseMac (), HEX);
#endif

    strcpy (WebSocketFrameCollectionBuffer, "XA");
    size_t msgOffset = strlen (WebSocketFrameCollectionBuffer);
    serializeJson (webJsonDoc, &WebSocketFrameCollectionBuffer[msgOffset], (sizeof (WebSocketFrameCollectionBuffer) - msgOffset));
    // DEBUG_V (String(WebSocketFrameCollectionBuffer));

    client->text (WebSocketFrameCollectionBuffer);

    // DEBUG_END;

} // ProcessXARequest

//-----------------------------------------------------------------------------
void c_WebMgr::ProcessXJRequest (AsyncWebSocketClient* client)
{
    // DEBUG_START;

    DynamicJsonDocument webJsonDoc (2048);
    JsonObject status = webJsonDoc.createNestedObject (CN_status);
    JsonObject system = status.createNestedObject (CN_system);

    system[F ("freeheap")] = (String)ESP.getFreeHeap ();
    system[F ("uptime")] = millis ();
    system[F ("SDinstalled")] = FileMgr.SdCardIsInstalled ();

    // DEBUG_V ("");

    // Ask WiFi Stats
    WiFiMgr.GetStatus (system);
    // DEBUG_V ("");

    FPPDiscovery.GetStatus (system);
    // DEBUG_V ("");

    // Ask Input Stats
    InputMgr.GetStatus (status);
    // DEBUG_V ("");

    // Ask Output Stats
    OutputMgr.GetStatus (status);
    // DEBUG_V ("");

    String response;
    serializeJson (webJsonDoc, response);
    // DEBUG_V (response);

    client->text ((String (F ("XJ")) + response).c_str());

    // DEBUG_END;

} // ProcessXJRequest

//-----------------------------------------------------------------------------
/// Process simple format 'V' messages
void c_WebMgr::ProcessVseriesRequests (AsyncWebSocketClient* client)
{
    // DEBUG_START;

    // String Response;
    // serializeJson (webJsonDoc, response);
    switch (WebSocketFrameCollectionBuffer[1])
    {
        case '1':
        {
            // Diag screen is asking for real time output data
            client->binary (OutputMgr.GetBufferAddress (), OutputMgr.GetBufferUsedSize ());
            break;
        }

        default:
        {
            client->text (F ("V Error"));
            LOG_PORT.println (String(CN_stars) + F (" ERROR: Unsupported Web command V") + WebSocketFrameCollectionBuffer[1] + CN_stars);
            break;
        }
    } // end switch

    // DEBUG_END;

} // ProcessVseriesRequests

//-----------------------------------------------------------------------------
/// Process simple format 'G' messages
void c_WebMgr::ProcessGseriesRequests (AsyncWebSocketClient* client)
{
    // DEBUG_START;

    // String Response;
    // serializeJson (webJsonDoc, response);
    switch (WebSocketFrameCollectionBuffer[1])
    {
        case '2':
        {
            // xLights asking the "version"
            client->text ((String (F ("G2{\"version\": \"")) + VERSION + "\"}").c_str());
            break;
        }

        default:
        {
            client->text (F ("G Error"));
            LOG_PORT.println (String(CN_stars) + F (" ERROR: Unsupported Web command V") + WebSocketFrameCollectionBuffer[1] + CN_stars);
            break;
        }
    } // end switch

    // DEBUG_END;

} // ProcessVseriesRequests

//-----------------------------------------------------------------------------
// Process JSON messages
void c_WebMgr::ProcessReceivedJsonMessage (DynamicJsonDocument & webJsonDoc, AsyncWebSocketClient * client)
{
    // DEBUG_START;
    //LOG_PORT.printf_P( PSTR("ProcessReceivedJsonMessage heap / stack ZcppStats: %u:%u:%u:%u\n"), ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(), ESP.getFreeContStack());

    do // once
    {
        // DEBUG_V ("");

        /** Following commands are supported:
         * - get: returns requested configuration
         * - set: receive and applies configuration
         * - opt: returns select option lists
         */
        if (webJsonDoc.containsKey (CN_cmd))
        {
            // DEBUG_V ("cmd");
            {
                // JsonObject jsonCmd = webJsonDoc.as<JsonObject> ();
                // PrettyPrint (jsonCmd);
            }
            JsonObject jsonCmd = webJsonDoc[CN_cmd];
            processCmd (client, jsonCmd);
            break;
        } // webJsonDoc.containsKey ("cmd")

        // DEBUG_V ("");

    } while (false);

    // DEBUG_END;

} // ProcessReceivedJsonMessage

//-----------------------------------------------------------------------------
void c_WebMgr::processCmd (AsyncWebSocketClient * client, JsonObject & jsonCmd)
{
    // DEBUG_START;

    // PrettyPrint (jsonCmd);

    do // once
    {
        // Process get command - return requested configuration as JSON
        if (jsonCmd.containsKey (CN_get))
        {
            // DEBUG_V (CN_get);
            strcpy(WebSocketFrameCollectionBuffer, "{\"get\":");
            // DEBUG_V ("");
            processCmdGet (jsonCmd);
            strcat (WebSocketFrameCollectionBuffer, "}");
            // DEBUG_V ("");
            break;
        }

        // Process "SET" command - return requested configuration as JSON
        if (jsonCmd.containsKey ("set"))
        {
            // DEBUG_V ("set");
            // strcpy(WebSocketFrameCollectionBuffer, "{\"set\":");
            JsonObject jsonCmdSet = jsonCmd["set"];
            // DEBUG_V ("");
            processCmdSet (jsonCmdSet);
            strcpy (WebSocketFrameCollectionBuffer, "{\"cmd\":\"OK\"}");
            // DEBUG_V ("");
            break;
        }

        // Generate select option list data
        if (jsonCmd.containsKey ("opt"))
        {
            // DEBUG_V ("opt");
            strcpy(WebSocketFrameCollectionBuffer, "{\"opt\":");
            // DEBUG_V ("");
            processCmdOpt (jsonCmd);
            strcat (WebSocketFrameCollectionBuffer, "}");

            // DEBUG_V ("");
            break;
        }

        if (jsonCmd.containsKey ("delete"))
        {
            // DEBUG_V ("opt");
            JsonObject temp = jsonCmd["delete"];
            processCmdDelete (temp);
            strcpy (WebSocketFrameCollectionBuffer, "{\"cmd\":\"OK\"}");
            // DEBUG_V ("");
            break;
        }

        // log an error
        PrettyPrint (jsonCmd, String (F ("ERROR: Unhandled cmd")));
        strcpy (WebSocketFrameCollectionBuffer, "{\"cmd\":\"Error\"}");

    } while (false);

    client->text (WebSocketFrameCollectionBuffer);

    // DEBUG_END;

} // processCmd

//-----------------------------------------------------------------------------
void c_WebMgr::processCmdGet (JsonObject & jsonCmd)
{
    // DEBUG_START;
    // PrettyPrint (jsonCmd);

    // DEBUG_V ("");

    do // once
    {
        size_t bufferoffset = strlen(WebSocketFrameCollectionBuffer);
        size_t BufferFreeSize = sizeof (WebSocketFrameCollectionBuffer) - bufferoffset;

        if ((jsonCmd[CN_get] == CN_device) ||
            (jsonCmd[CN_get] == CN_network)  )
        {
            // DEBUG_V ("device/network");
            FileMgr.ReadConfigFile (ConfigFileName, 
                                    (byte*)&WebSocketFrameCollectionBuffer[bufferoffset],
                                    BufferFreeSize);
            // DEBUG_V ("");
            break;
        }

        if (jsonCmd[CN_get] == CN_output)
        {
            // DEBUG_V (CN_output);
            OutputMgr.GetConfig ((byte*)&WebSocketFrameCollectionBuffer[bufferoffset],
                                 BufferFreeSize);
            // DEBUG_V ("");
            break;
        }

        if (jsonCmd[CN_get] == CN_input)
        {
            // DEBUG_V ("input");
            InputMgr.GetConfig ((byte*)&WebSocketFrameCollectionBuffer[bufferoffset],
                                BufferFreeSize);
            // DEBUG_V ("");
            break;
        }

        if (jsonCmd[CN_get] == CN_files)
        {
            // DEBUG_V ("input");
            String Temp;
            FileMgr.GetListOfSdFiles (Temp);
            if (Temp.length () >= BufferFreeSize )
            {
                strcat (WebSocketFrameCollectionBuffer, "\"ERROR\": \"File List Too Long\"");
            }
            else
            {
                strcat (WebSocketFrameCollectionBuffer, Temp.c_str ());
            }
            // DEBUG_V ("");
            break;
        }

        // log an error
        PrettyPrint (jsonCmd, String (F ("ERROR: Unhandled Get Request")));
        strcat (WebSocketFrameCollectionBuffer, "\"ERROR\": \"Request Not Supported\"");

    } while (false);

    // DEBUG_V (WebSocketFrameCollectionBuffer);

    // DEBUG_END;

} // processCmdGet

//-----------------------------------------------------------------------------
void c_WebMgr::processCmdSet (JsonObject & jsonCmd)
{
    // DEBUG_START;
    // PrettyPrint (jsonCmd);

    do // once
    {
        if ((jsonCmd.containsKey (CN_device)) || (jsonCmd.containsKey (CN_network)))
        {
            // DEBUG_V ("device/network");
            extern void SetConfig (JsonObject &, const char* DataString);
            serializeJson (jsonCmd, WebSocketFrameCollectionBuffer, sizeof (WebSocketFrameCollectionBuffer) - 1);
            SetConfig (jsonCmd, WebSocketFrameCollectionBuffer);
            pAlexaDevice->setName (config.id);

            // DEBUG_V ("device/network: Done");
            break;
        }

        if (jsonCmd.containsKey (CN_input))
        {
            // DEBUG_V ("input");
            JsonObject imConfig = jsonCmd[CN_input];
            serializeJson (imConfig, WebSocketFrameCollectionBuffer, sizeof (WebSocketFrameCollectionBuffer) - 1);
            InputMgr.SetConfig (WebSocketFrameCollectionBuffer);
            // DEBUG_V ("input: Done");
            break;
        }

        if (jsonCmd.containsKey (CN_output))
        {
            // DEBUG_V (CN_output);
            JsonObject omConfig = jsonCmd[CN_output];
            serializeJson (omConfig, WebSocketFrameCollectionBuffer, sizeof(WebSocketFrameCollectionBuffer) - 1);
            OutputMgr.SetConfig (WebSocketFrameCollectionBuffer);
            // DEBUG_V ("output: Done");
            break;
        }

        if (jsonCmd.containsKey (F ("time")))
        {
            // PrettyPrint (jsonCmd, String ("processCmdSet"));

            // DEBUG_V ("time");
            JsonObject otConfig = jsonCmd[F ("time")];
            processCmdSetTime (otConfig);
            // DEBUG_V ("output: Done");
            break;
        }

        LOG_PORT.println ();
        PrettyPrint (jsonCmd, String(CN_stars) + F (" ERROR: Undhandled Set request type. ") + CN_stars );
        strcat (WebSocketFrameCollectionBuffer, "ERROR");

    } while (false);

    // DEBUG_END;

} // processCmdSet

//-----------------------------------------------------------------------------
void c_WebMgr::processCmdSetTime (JsonObject& jsonCmd)
{
    // DEBUG_START;

    // PrettyPrint (jsonCmd, String("processCmdSetTime"));

    time_t TimeToSet = 0;
    setFromJSON (TimeToSet, jsonCmd, F ("time_t"));

    struct timeval now = { .tv_sec = TimeToSet };

    settimeofday (&now, NULL);

    // DEBUG_V (String ("TimeToSet: ") + String (TimeToSet));
    // DEBUG_V (String ("TimeToSet: ") + String (ctime(&TimeToSet)));

    strcat (WebSocketFrameCollectionBuffer, "{\"OK\" : true}");

    // DEBUG_END;

} // ProcessXTRequest

//-----------------------------------------------------------------------------
void c_WebMgr::processCmdOpt (JsonObject & jsonCmd)
{
    // DEBUG_START;
    // PrettyPrint (jsonCmd);

    do // once
    {
        // DEBUG_V ("");
        if (jsonCmd[F ("opt")] == CN_device)
        {
            // DEBUG_V (CN_device);
            GetDeviceOptions ();
            break;
        }

        // log error
        PrettyPrint (jsonCmd, String (F ("ERROR: Unhandled 'opt' Request: ")));

    } while (false);

    // DEBUG_END;

} // processCmdOpt

//-----------------------------------------------------------------------------
void c_WebMgr::processCmdDelete (JsonObject& jsonCmd)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V ("");

        if (jsonCmd.containsKey (CN_files))
        {
            // DEBUG_V ("");

            JsonArray JsonFilesToDelete = jsonCmd[CN_files];

            // DEBUG_V ("");
            for (JsonObject JsonFile : JsonFilesToDelete)
            {
                String FileToDelete = JsonFile[CN_name];

                // DEBUG_V ("FileToDelete: " + FileToDelete);
                FileMgr.DeleteSdFile (FileToDelete);
            }

            String Temp;
            FileMgr.GetListOfSdFiles (Temp);
            Temp += "}";
            strcpy (WebSocketFrameCollectionBuffer, "{\"cmd\": { \"delete\": ");
            strcat (WebSocketFrameCollectionBuffer, Temp.c_str ());

            break;
        }

        PrettyPrint (jsonCmd, String (F ("* Unsupported Delete command: ")));
        strcat (WebSocketFrameCollectionBuffer, "Page Not found");

    } while (false);

    // DEBUG_END;

} // processCmdDelete

//-----------------------------------------------------------------------------
void c_WebMgr::FirmwareUpload (AsyncWebServerRequest* request,
                               String filename,
                               size_t index,
                               uint8_t * data,
                               size_t len,
                               bool final)
{
    // DEBUG_START;

    do // once
    {
        // make sure we are in AP mode
        if (0 == WiFi.softAPgetStationNum ())
        {
            // DEBUG_V("Not in AP Mode");

            // we are not talking to a station so we are not in AP mode
            // break;
        }
        // DEBUG_V ("In AP Mode");

        // is the first message in the upload?
        if (0 == index)
        {
#ifdef ARDUINO_ARCH_ESP8266
            WiFiUDP::stopAll ();
#else
            // this is not supported for ESP32
#endif
            LOG_PORT.println (String(F ("* Upload Started: ")) + filename);
            efupdate.begin ();
        }

        // DEBUG_V ("Sending data to efupdate");
        // DEBUG_V (String ("data: 0x") + String (uint32(data), HEX));
        // DEBUG_V (String (" len: ") + String (len));

        if (!efupdate.process (data, len))
        {
            LOG_PORT.println (String(CN_stars) + F (" UPDATE ERROR: ") + String (efupdate.getError ()));
        }
        // DEBUG_V ("Packet has been processed");

        if (efupdate.hasError ())
        {
            // DEBUG_V ("efupdate.hasError");
            request->send (200, CN_textSLASHplain, (String (F ("Update Error: ")) + String (efupdate.getError ()).c_str()));
            break;
        }
        // DEBUG_V ("No Error");

        if (final)
        {
            request->send (200, CN_textSLASHplain, (String ( F ("Update Finished: ")) + String (efupdate.getError ())).c_str());
            LOG_PORT.println (F ("* Upload Finished."));
            efupdate.end ();
            LITTLEFS.begin ();

            extern bool reboot;
            reboot = true;
        }

    } while (false);

    // DEBUG_END;

} // onEvent

//-----------------------------------------------------------------------------
/*
 * This function is called as part of the Arudino "loop" and does things that need
 * periodic poking.
 *
 */
void c_WebMgr::Process ()
{
    if (true == IsAlexaCallbackValid())
    {
        espalexa.loop ();
    }
} // Process

#ifdef USE_REST
void printRequest (AsyncWebServerRequest* request)
{
    // DEBUG_V (String ("      version: '") + String (request->version ()) + "'");
    // DEBUG_V (String ("       method: '") + String (request->method ()) + "'");
    // DEBUG_V (String ("          url: '") + String (request->url ()) + "'");
    // DEBUG_V (String ("         host: '") + String (request->host ()) + "'");
    // DEBUG_V (String ("  contentType: '") + String (request->contentType ()) + "'");
    // DEBUG_V (String ("contentLength: '") + String (request->contentLength ()) + "'");
    // DEBUG_V (String ("    multipart: '") + String (request->multipart ()) + "'");

    //List all collected headers
    int headers = request->headers ();
    int i;
    for (i = 0; i < headers; i++)
    {
        AsyncWebHeader* h = request->getHeader (i);
        // DEBUG_V (String ("       HEADER: '") + h->name () + "', '" + h->value () + "'");
    }

} // printRequest
//-----------------------------------------------------------------------------
void c_WebMgr::RestProcessGET (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printRequest (request);

    // request->send (200, "text/json", WebSocketFrameCollectionBuffer);


    request->send (200, CN_textSLASHplain, "Hello");

    // DEBUG_END;

} // RestProcessPOST

//-----------------------------------------------------------------------------
void c_WebMgr::RestProcessPOST (AsyncWebServerRequest* request)
{
    // DEBUG_START;
    printRequest (request);

    request->send (404, CN_textSLASHplain, "Page Not found");

    // DEBUG_END;

} // RestProcessGET

//-----------------------------------------------------------------------------
void c_WebMgr::RestProcessFile (AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final)
{
    // DEBUG_START;
    printRequest (request);

    request->send (404, CN_textSLASHplain, "Page Not found");

    // DEBUG_END;

} // RestProcessFile

//-----------------------------------------------------------------------------
void c_WebMgr::RestProcessBody (AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
{
    // DEBUG_START;
    printRequest (request);

    request->send (404, CN_textSLASHplain, "Page Not found");

    // DEBUG_END;

} // RestProcessBody
#endif // def USE_REST

//-----------------------------------------------------------------------------
// create a global instance of the WEB UI manager
c_WebMgr WebMgr;
