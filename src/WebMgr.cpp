/*
* WebMgr.cpp - Output Management class
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
*/

#include "ESPixelStick.h"

#include "output/OutputMgr.hpp"
#include "input/InputMgr.hpp"
#include "service/FPPDiscovery.h"
#include "WiFiMgr.hpp"

#include "WebMgr.hpp"
#include <Int64String.h>

#ifdef ARDUINO_ARCH_ESP8266
#   define SD_OPEN_WRITEFLAGS   sdfat::O_READ | sdfat::O_WRITE | sdfat::O_CREAT | sdfat::O_TRUNC
#elif defined ARDUINO_ARCH_ESP32
#   include <SPIFFS.h>
#   define SD_OPEN_WRITEFLAGS   "w"

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
void PrettyPrint (JsonObject & jsonStuff)
{
    // DEBUG_V ("---------------------------------------------");
    LOG_PORT.println (F("---- Pretty ----"));
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

    init ();

    // DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
// Configure and start the web server
void c_WebMgr::init ()
{
    // DEBUG_START;
    // Add header for SVG plot support?
    DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Origin"),  "*");
    DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Headers"), "Content-Type, Content-Range, Content-Disposition, Content-Description, cache-control, x-requested-with");
    DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Methods"), "GET, PUT, POST, OPTIONS");

    // Setup WebSockets
    webSocket.onEvent ([this](AsyncWebSocket* server, AsyncWebSocketClient * client, AwsEventType type, void* arg, uint8_t * data, size_t len)
        {
            this->onWsEvent (server, client, type, arg, data, len);
        });
    webServer.addHandler (&webSocket);

    // Heap status handler
    webServer.on ("/heap", HTTP_GET, [](AsyncWebServerRequest* request)
        {
            request->send (200, "text/plain", String (ESP.getFreeHeap ()));
        });

    // JSON Config Handler
    webServer.on ("/conf", HTTP_GET, [this](AsyncWebServerRequest* request)
        {
            this->GetConfiguration ();
            request->send (200, "text/json", WebSocketFrameCollectionBuffer);
        });

    // Firmware upload handler - only in station mode
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

    // Root access for testing
    webServer.serveStatic ("/root", SPIFFS, "/");

    // Static Handler
    webServer.serveStatic ("/", SPIFFS, "/www/").setDefaultFile ("index.html");

    // if the client posts to the upload page
    webServer.on ("/upload", HTTP_POST | HTTP_PUT | HTTP_OPTIONS, 
        [](AsyncWebServerRequest * request)
        {
            // DEBUG_V ("Got upload post request");
            if (true == FPPDiscovery.SdcardIsInstalled())
            {
                // Send status 200 (OK) to tell the client we are ready to receive
                request->send (200);
            }
            else
            {
                request->send (404, "text/plain", "Page Not found");
            }
        },

        [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final)
        {
            // DEBUG_V ("Got process FIle request");
            // DEBUG_V (String ("Got process File request: name: ")  + filename);
            // DEBUG_V (String ("Got process File request: index: ") + String (index));
            // DEBUG_V (String ("Got process File request: len:   ") + String (len));
            // DEBUG_V (String ("Got process File request: final: ") + String (final));
            if (true == FPPDiscovery.SdcardIsInstalled())
            {
                this->handleFileUpload (request, filename, index, data, len, final); // Receive and save the file 
            }
            else
            {
                request->send (404, "text/plain", "Page Not found");
            }
        },

        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
        {
            // DEBUG_V (String ("Got process Body request: index: ") + String (index));
            // DEBUG_V (String ("Got process Body request: len:   ") + String (len));
            // DEBUG_V (String ("Got process Body request: total: ") + String (total));
            request->send (404, "text/plain", "Page Not found");
        }
    );

    // Raw config file Handler - but only on station
    //  webServer.serveStatic("/config.json", SPIFFS, "/config.json").setFilter(ON_STA_FILTER);

    webServer.onNotFound ([this](AsyncWebServerRequest* request)
    {
        // DEBUG_V ("onNotFound");
        if (true == this->IsAlexaCallbackValid())
        {
            // DEBUG_V ("IsAlexaCallbackValid == true");
            if (!espalexa.handleAlexaApiCall (request)) //if you don't know the URI, ask espalexa whether it is an Alexa control request
            {
                // DEBUG_V ("Alexa Callback could not resolve the request");
                request->send (404, "text/plain", "Page Not found");
            }
        }
        else
        {
            // DEBUG_V ("IsAlexaCallbackValid == false");
            request->send (404, "text/plain", "Page Not found");
        }
    });

    //give espalexa a pointer to the server object so it can use your server instead of creating its own
    espalexa.begin (&webServer);

    // webServer.begin ();

    pAlexaDevice = new EspalexaDevice (String ("ESP"), [this](EspalexaDevice* pDevice)
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

    if (0 == index) 
    {
        // save the filename
        // DEBUG_V ("UploadStart: " + filename);
        if (!filename.startsWith ("/"))
        {
            filename = "/" + filename;
        }

        // are we terminating the previous download?
        if (0 != fsUploadFileName.length())
        {
            LOG_PORT.println (String (F ("Aborting Previous File Upload For: '")) + fsUploadFileName + String (F ("'")));
            fsUploadFile.close ();
            fsUploadFileName = "";
        }

        fsUploadFileName = filename;

        LOG_PORT.println (String (F ("Upload File: '")) + fsUploadFileName + String(F("' Started")));

        SD.remove (fsUploadFileName);

        // Open the file for writing 
        fsUploadFile = SD.open (fsUploadFileName, SD_OPEN_WRITEFLAGS);
        fsUploadFile.seek (0);
    }

    if ((0 != len) && (0 != fsUploadFileName.length()))
    {
        // Write data
        // DEBUG_V ("UploadWrite: " + String (len) + String (" bytes"));
        fsUploadFile.write (data, len);
        LOG_PORT.print (String("Writting bytes: ") + String (index) + "\r");
    }

    if ((true == final) && (0 != fsUploadFileName.length ()))
    {
        LOG_PORT.println ("");
        // DEBUG_V ("UploadEnd: " + String(index + len) + String(" bytes"));
        LOG_PORT.println (String (F ("Upload File: '")) + fsUploadFileName + String (F ("' Done")));

        fsUploadFile.close ();
        fsUploadFileName = "";
    }

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

    JsonObject JsonSystemConfig = webJsonDoc.createNestedObject (F("system"));
    // GetConfig (JsonSystemConfig);
    // DEBUG_V ("");

    JsonObject JsonOutputConfig = webJsonDoc.createNestedObject (F ("outputs"));
    // OutputMgr.GetConfig (JsonOutputConfig);
    // DEBUG_V ("");

    JsonObject JsonInputConfig = webJsonDoc.createNestedObject (F ("inputs"));
    // InputMgr.GetConfig (JsonInputConfig);

    // now make it something we can transmit
    serializeJson (webJsonDoc, WebSocketFrameCollectionBuffer);

    // DEBUG_END;

} // GetConfiguration

//-----------------------------------------------------------------------------
void c_WebMgr::GetOptions ()
{
    // DEBUG_START;

    // set up a framework to get the option data
    DynamicJsonDocument webJsonDoc (4096);

    if (0 == webJsonDoc.capacity ())
    {
        LOG_PORT.println (F("ERROR: Failed to allocate memory for the GetOptions web request response."));
    }

    // DEBUG_V ("");
    JsonObject WebOptions        = webJsonDoc.createNestedObject (F ("options"));
    JsonObject JsonInputOptions  = WebOptions.createNestedObject (F ("input"));
    JsonObject JsonOutputOptions = WebOptions.createNestedObject (F ("output"));
    // DEBUG_V("");

    // PrettyPrint (WebOptions);

    InputMgr.GetOptions (JsonInputOptions);
    // DEBUG_V (""); // remove and we crash

    OutputMgr.GetOptions (JsonOutputOptions);
    // DEBUG_V ("");

    // PrettyPrint (WebOptions);

    // now make it something we can transmit
    size_t msgOffset = strlen (WebSocketFrameCollectionBuffer);
    serializeJson (WebOptions, & WebSocketFrameCollectionBuffer[msgOffset], (sizeof(WebSocketFrameCollectionBuffer) - msgOffset));

    // DEBUG_END;

} // GetOptions

//-----------------------------------------------------------------------------
/// Handle Web Service events
/** Handles all Web Service event types and performs initial parsing of Web Service event data.
 * Text messages that start with 'X' are treated as "Simple" format messages, else they're parsed as JSON.
 */
void c_WebMgr::onWsEvent (AsyncWebSocket* server, AsyncWebSocketClient * client,
    AwsEventType type, void * arg, uint8_t * data, size_t len)
{
    // DEBUG_START;
    // DEBUG_V (String ("Heap = ") + ESP.getFreeHeap ());

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
                LOG_PORT.println (String (F ("*** WebIO::onWsEvent(): Error: Incoming message is TOO long.")));
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

            // convert the input data into a json structure (use json read only mode)
            DynamicJsonDocument webJsonDoc (5*1023);
            DeserializationError error = deserializeJson (webJsonDoc, (const char *)(&WebSocketFrameCollectionBuffer[0]));

            // DEBUG_V ("");
            if (error)
            {
                LOG_PORT.println (String (F ("*** WebIO::onWsEvent(): Parse Error: ")) + error.c_str ());
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

    // DEBUG_V (String ("Heap = ") + ESP.getFreeHeap());

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

        default:
        {
            LOG_PORT.println (String (F ("ERROR: Unhandled request: ")) + WebSocketFrameCollectionBuffer);
            client->text (String (F ("{\"Error\":Error")));
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
    jsonAdmin["usedflashsize"] = "999999999";
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

    DynamicJsonDocument webJsonDoc (1024);
    JsonObject status = webJsonDoc.createNestedObject (F ("status"));
    JsonObject system = status.createNestedObject (F ("system"));

    system[F ("freeheap")] = (String)ESP.getFreeHeap ();
    system[F ("uptime")] = millis ();
    system[F ("SDinstalled")] = FPPDiscovery.SdcardIsInstalled();

    // DEBUG_V ("");

    // Ask WiFi Stats
    WiFiMgr.GetStatus (system);
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

    client->text (String (F ("XJ")) + response);

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
            client->binary (OutputMgr.GetBufferAddress (), OutputMgr.GetBufferSize ());
            break;
        }

        default:
        {
            client->text (F ("V Error"));
            LOG_PORT.println (String(F("***** ERROR: Unsupported Web command V")) + WebSocketFrameCollectionBuffer[1] + F(" *****"));
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
            client->text (String (F ("G2{\"version\": \"")) + VERSION + "\"}");
            break;
        }

        default:
        {
            client->text (F ("G Error"));
            LOG_PORT.println (String(F("***** ERROR: Unsupported Web command V")) + WebSocketFrameCollectionBuffer[1] + F(" *****"));
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
    //LOG_PORT.printf("ProcessReceivedJsonMessage heap /stack ZcppStats: %u:%u:%u:%u\n", ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(), ESP.getFreeContStack());

    do // once
    {
        // DEBUG_V ("");

        /** Following commands are supported:
         * - get: returns requested configuration
         * - set: receive and applies configuration
         * - opt: returns select option lists
         */
        if (webJsonDoc.containsKey ("cmd"))
        {
            // DEBUG_V ("cmd");
            {
                // JsonObject jsonCmd = webJsonDoc.as<JsonObject> ();
                // PrettyPrint (jsonCmd);
            }
            JsonObject jsonCmd = webJsonDoc["cmd"];
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

    WebSocketFrameCollectionBuffer[0] = NULL;
    // PrettyPrint (jsonCmd);

    do // once
    {
        // Process "GET" command - return requested configuration as JSON
        if (jsonCmd.containsKey ("get"))
        {
            // DEBUG_V ("get");
            strcpy(WebSocketFrameCollectionBuffer, "{\"get\":");
            // DEBUG_V ("");
            processCmdGet (jsonCmd);
            // DEBUG_V ("");
            break;
        }

        // Process "SET" command - return requested configuration as JSON
        if (jsonCmd.containsKey ("set"))
        {
            // DEBUG_V ("set");
            strcpy(WebSocketFrameCollectionBuffer, "{\"set\":");
            JsonObject jsonCmdSet = jsonCmd["set"];
            // DEBUG_V ("");
            processCmdSet (jsonCmdSet);
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
            // DEBUG_V ("");
            break;
        }

        // Generate select option list data
        if (jsonCmd.containsKey ("delete"))
        {
            // DEBUG_V ("opt");
            strcpy (WebSocketFrameCollectionBuffer, "{\"get\":");
            // DEBUG_V ("");
            JsonObject temp = jsonCmd["delete"];
            processCmdDelete (temp);
            // DEBUG_V ("");
            break;
        }

        // log an error
        LOG_PORT.println (String (F ("ERROR: Unhandled cmd")));
        PrettyPrint (jsonCmd);
        strcpy (WebSocketFrameCollectionBuffer, "{\"cmd\":\"Error\"");

    } while (false);

    // DEBUG_V ("");
    // terminate the response
    strcat (WebSocketFrameCollectionBuffer, "}");
    // DEBUG_V (WebSocketFrameCollectionBuffer);
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
        if ((jsonCmd["get"] == "device") || 
            (jsonCmd["get"] == "network")  )
        {
            // DEBUG_V ("device/network");
            strcat(WebSocketFrameCollectionBuffer, serializeCore (false).c_str());
            // DEBUG_V ("");
            break;
        }

        if (jsonCmd["get"] == "output")
        {
            // DEBUG_V ("output");
            OutputMgr.GetConfig (WebSocketFrameCollectionBuffer);
            // DEBUG_V ("");
            break;
        }

        if (jsonCmd["get"] == "input")
        {
            // DEBUG_V ("input");
            InputMgr.GetConfig (WebSocketFrameCollectionBuffer);
            // DEBUG_V ("");
            break;
        }

        if (jsonCmd["get"] == "files")
        {
            // DEBUG_V ("input");
            FPPDiscovery.GetListOfFiles (WebSocketFrameCollectionBuffer);
            // DEBUG_V ("");
            break;
        }

        // log an error
        LOG_PORT.println (String (F("ERROR: Unhandled Get Request")));
        strcat (WebSocketFrameCollectionBuffer, "\"ERROR: Request Not Supported\"");
        PrettyPrint (jsonCmd);

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
        if ((jsonCmd.containsKey ("device")) || (jsonCmd.containsKey ("network")))
        {
            // DEBUG_V ("device/network");
            extern void SetConfig (JsonObject &);
            SetConfig (jsonCmd);
            strcat (WebSocketFrameCollectionBuffer, serializeCore (false).c_str ());

            pAlexaDevice->setName (config.id);

            // DEBUG_V ("device/network: Done");
            break;
        }

        if (jsonCmd.containsKey ("input"))
        {
            // DEBUG_V ("input");
            JsonObject imConfig = jsonCmd["input"];
            InputMgr.SetConfig (imConfig);
            InputMgr.GetConfig (WebSocketFrameCollectionBuffer);
            // DEBUG_V ("input: Done");
            break;
        }

        if (jsonCmd.containsKey ("output"))
        {
            // DEBUG_V ("output");
            JsonObject omConfig = jsonCmd[F("output")];
            OutputMgr.SetConfig (omConfig);
            OutputMgr.GetConfig (WebSocketFrameCollectionBuffer);
            // DEBUG_V ("output: Done");
            break;
        }

        LOG_PORT.println (F("***** ERROR: Undhandled Set request type. *****"));
        PrettyPrint (jsonCmd);
        strcat (WebSocketFrameCollectionBuffer, "ERROR");

    } while (false);

    // DEBUG_V (WebSocketFrameCollectionBuffer);

 // DEBUG_END;

} // processCmdSet

//-----------------------------------------------------------------------------
void c_WebMgr::processCmdOpt (JsonObject & jsonCmd)
{
    // DEBUG_START;
    // PrettyPrint (jsonCmd);

    do // once
    {
        // DEBUG_V ("");
        if (jsonCmd["opt"] == "device")
        {
            // DEBUG_V ("device");
            GetOptions ();
            break;
        }

        // log error
        LOG_PORT.println (String (F ("ERROR: Unhandled 'opt' Request: ")));
        PrettyPrint (jsonCmd);

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

        if (jsonCmd.containsKey ("files"))
        {
            // DEBUG_V ("");

            JsonArray JsonFilesToDelete = jsonCmd["files"];

            // DEBUG_V ("");
            for (JsonObject JsonFile : JsonFilesToDelete)
            {
                String FileToDelete = JsonFile["name"];

                // DEBUG_V ("FileToDelete: " + FileToDelete);
                FPPDiscovery.DeleteFseqFile (FileToDelete);
            }

            FPPDiscovery.GetListOfFiles (WebSocketFrameCollectionBuffer);
            break;
        }

        LOG_PORT.println (String (F ("* Unsupported Delete command: ")));
        PrettyPrint (jsonCmd);
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
        // DEBUG_V ("");

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

        // DEBUG_V ("");

        if (!efupdate.process (data, len))
        {
            LOG_PORT.println (String(F ("*** UPDATE ERROR: ")) + String (efupdate.getError ()));
        }

        if (efupdate.hasError ())
        {
            // DEBUG_V ("efupdate.hasError");
            request->send (200, "text/plain", "Update Error: " + String (efupdate.getError ()));
            break;
        }
        // DEBUG_V ("");

        if (final)
        {
            request->send (200, "text/plain", "Update Finished: " + String (efupdate.getError ()));
            LOG_PORT.println (F ("* Upload Finished."));
            efupdate.end ();
            SPIFFS.begin ();
            SaveConfig();
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

//-----------------------------------------------------------------------------
// create a global instance of the WEB UI manager
c_WebMgr WebMgr;
