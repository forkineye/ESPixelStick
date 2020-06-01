/*
* WebMgr.cpp - Output Management class
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
*/

#include <Arduino.h>
#include "ESPixelStick.h"

#include "output/OutputMgr.hpp"
#include "input/InputMgr.hpp"

#include "WebMgr.hpp"

const uint8_t HTTP_PORT = 80;      ///< Default web server port

AsyncWebServer  webServer (HTTP_PORT);  // Web Server
AsyncWebSocket  webSocket ("/ws");      // Web Socket Plugin

//-----------------------------------------------------------------------------
///< Start up the driver and put it into a safe mode
c_WebMgr::c_WebMgr ()
{
    // this gets called pre-setup so there is nothing we can do here.
} // c_WebMgr

//-----------------------------------------------------------------------------
///< deallocate any resources and put the output channels into a safe state
c_WebMgr::~c_WebMgr()
{
    // DEBUG_START;

    // DEBUG_END;

} // ~c_WebMgr

//-----------------------------------------------------------------------------
///< Start the module
void c_WebMgr::Begin (config_t * NewConfig)
{
    // DEBUG_START;

    // save the pointer to the config
    config = NewConfig;

    init ();

   //  DEBUG_END;

} // begin

//-----------------------------------------------------------------------------
// Configure and start the web server
void c_WebMgr::init ()
{
    // DEBUG_START;
    // Add header for SVG plot support?
    DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Origin"), "*");

    // Setup WebSockets
    webSocket.onEvent ([this](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void* arg, uint8_t * data, size_t len) 
        { 
            this->onWsEvent(server, client, type, arg, data, len);
        });
    webServer.addHandler (& webSocket);

    // Heap status handler
    webServer.on ("/heap", HTTP_GET, [](AsyncWebServerRequest* request)
        {
            request->send (200, "text/plain", String (ESP.getFreeHeap ()));
        });

    // JSON Config Handler
    webServer.on ("/conf", HTTP_GET, [this](AsyncWebServerRequest* request)
        {
            request->send (200, "text/json", this->GetConfiguration());
        });

#ifdef ToDoFixThis
    // Firmware upload handler - only in station mode
    webServer.on ("/updatefw", HTTP_POST, [](AsyncWebServerRequest* request)
        {
            webSocket.textAll ("X6");
        }, [this]() {this->onFirmwareUpload (); }); // todo .setFilter (ON_STA_FILTER);
#endif // def ToDoFixThis

    // Root access for testing
    webServer.serveStatic ("/root", SPIFFS, "/");

    // Static Handler
    webServer.serveStatic ("/", SPIFFS, "/www/").setDefaultFile ("index.html");

    // Raw config file Handler - but only on station
    //  webServer.serveStatic("/config.json", SPIFFS, "/config.json").setFilter(ON_STA_FILTER);

    webServer.onNotFound ([](AsyncWebServerRequest* request)
        {
            request->send (404, "text/plain", "Page not found");
        });
    
    webServer.begin ();

    LOG_PORT.println (String(F ("- Web Server started on port ")) + HTTP_PORT);
    
    // DEBUG_END;
}

//-----------------------------------------------------------------------------
/*
    Gather config data from the various config sources and send it to the web page.
*/
String c_WebMgr::GetConfiguration ()
{
    extern void GetConfig (JsonObject & json);
    // DEBUG_START;

    String Response = "";

    // set up a framework to get the config data
    DynamicJsonDocument JsonConfigDoc (1024);

    // DEBUG_V("");

    JsonObject JsonSystemConfig = JsonConfigDoc.createNestedObject ("system");
    GetConfig (JsonSystemConfig);
    // DEBUG_V ("");

    JsonObject JsonOutputConfig = JsonConfigDoc.createNestedObject ("outputs");
    OutputMgr.GetConfig (JsonOutputConfig);
    // DEBUG_V ("");

    JsonObject JsonInputConfig = JsonConfigDoc.createNestedObject ("inputs");
    InputMgr.GetConfig (JsonInputConfig);

#ifdef foo
    // todo Get input config
    String sJsonConfig = "";
    serializeJsonPretty (JsonConfigDoc, sJsonConfig);

    LOG_PORT.println ("sssssssssssssssssssss Pretty JsonConfig sssssssssssssssssssss");
    LOG_PORT.println (sJsonConfig);
    LOG_PORT.println ("sssssssssssssssssssss Pretty JsonConfig sssssssssssssssssssss");
#endif // def foo

    // now make it something we can transmit
    serializeJson (JsonConfigDoc, Response);

    // DEBUG_END;

    return Response;

} // GetConfiguration

//-----------------------------------------------------------------------------
/// Handle Web Service events
/** Handles all Web Service event types and performs initial parsing of Web Service event data.
 * Text messages that start with 'X' are treated as "Simple" format messages, else they're parsed as JSON.
 */
void c_WebMgr::onWsEvent (AsyncWebSocket* server, AsyncWebSocketClient* client,
    AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    switch (type)
    {
    case WS_EVT_DATA:
    {
        AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
        if (info->opcode == WS_TEXT)
        {
            if (data[0] == 'X')
            {
                procSimple (data, client);
            }
            else
            {
                procJSON (data, client);
            }
        }
        else
        {
            LOG_PORT.println (F ("-- binary message --"));
        }
        break;
    }

    case WS_EVT_CONNECT:
    {
        LOG_PORT.println (String (F ("* WS Connect - ")) + client->id ());
        break;
    }

    case WS_EVT_DISCONNECT:
    {
        LOG_PORT.println (String (F ("* WS Disconnect - ")) + client->id ());
        break;
    }

    case WS_EVT_PONG:
    {
        LOG_PORT.println (F ("* WS PONG *"));
        break;
    }

    case WS_EVT_ERROR:
    default:
    {
        LOG_PORT.println (F ("** WS ERROR **"));
        break;
    }
    } // end switch (type) 
} // onEvent

//-----------------------------------------------------------------------------
/// Process simple format 'X' messages
void c_WebMgr::procSimple (uint8_t* data, AsyncWebSocketClient* client)
{
    switch (data[1])
    {
        case SimpleMessage::GET_STATUS:
        {
            DynamicJsonDocument json (1024);

            // system statistics
            JsonObject system = json.createNestedObject ("system");
            system["freeheap"] = (String)ESP.getFreeHeap ();
            system["uptime"] = (String)millis ();

            // Ask WiFi to add stats

            // Ask Input to add stats

            // Ask Services to add stats

            String response;
            serializeJson (json, response);
            client->text ("XJ" + response);

            break;
        }
    }
} // procSimple

//-----------------------------------------------------------------------------
/// Process JSON messages
void c_WebMgr::procJSON (uint8_t* data, AsyncWebSocketClient* client)
{
    //LOG_PORT.printf("procJSON heap /stack stats: %u:%u:%u:%u\n", ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(), ESP.getFreeContStack());

    DynamicJsonDocument json (1024);
    DeserializationError error = deserializeJson (json, reinterpret_cast<char*>(data));
    if (error) 
    {
        LOG_PORT.println (F ("*** WebIO::procJSON(): Parse Error ***"));
        LOG_PORT.println (reinterpret_cast<char*>(data));
        return;
    }

    /** Following commands are supported:
     * - get: returns requested configuration
     * - set: receive and applies configuration
     * - opt: returns select option lists
     */
    if (json.containsKey ("cmd")) 
    {
        // Process "GET" command - return requested configuration as JSON
        if (json["cmd"]["get"]) 
        {
            String target = json["cmd"]["get"].as<String> ();

            if (target.equalsIgnoreCase ("device")) 
            {
                client->text ("{\"get\":" + serializeCore (false, true) + "}");
            }

            if (target.equalsIgnoreCase ("network")) 
            {
                client->text ("{\"get\":" + serializeCore (false, true) + "}");
            }
#ifdef JustAskInputMgr
            if (INPUT_MODES.end () != (INPUT_MODES::iterator itInput = INPUT_MODES.find (target)))
            {
                if (!itInput->first.equalsIgnoreCase (input->getKey ()))
                {
                    itInput->second->load ();
                }
                client->text ("{\"get\":" + itInput->second->serialize () + "}");
            }
#endif // def JustAskInputMgr
            // just give the array to the output manager
        }

        // Generate select option list data
        if (json["cmd"]["opt"]) 
        {
            String target = json["cmd"]["opt"].as<String> ();
            if (target.equalsIgnoreCase ("device")) 
            {
                DEBUG_V ("");
                client->text ("{\"opt\":" + GetConfiguration() + "}");
            }

            if (target.equalsIgnoreCase ("input"))
            {
                LOG_PORT.println ("*** WebIO opt input ***");
            }

            if (target.equalsIgnoreCase ("output"))
                LOG_PORT.println ("*** WebIO opt output ***");
        }
    }

    // Process "SET" command - receive configuration as JSON
    if (JsonObject root = json["cmd"]["set"].as<JsonObject> ()) 
    {
        DynamicJsonDocument doc (1024);
        doc.set (root);

        for (JsonPair kv : root) 
        {
            String key = kv.key ().c_str ();
            // Device config
            if (key.equalsIgnoreCase ("device")) 
            {
                if (dsDevice (doc))
                {
                    saveConfig ();
                }
                client->text ("{\"set\":" + serializeCore (false, true) + "}");
                // Network config
            }
            else if (key.equalsIgnoreCase ("network")) 
            {
                if (dsNetwork (doc))
                {
                    saveConfig ();
                }
                client->text ("{\"set\":" + serializeCore (false, true) + "}");
            }
            else 
            {
                // set input and output modules
                boolean changed = false;

                OutputMgr.SetConfig (root);

#ifdef foo
                // does the key exist in the input modes map?
                if (INPUT_MODES.end() != (auto itInput = INPUT_MODES.find (key)))
                {
                    if (itInput->second->deserialize (doc)) {
                        itInput->second->save ();
                        changed = true;
                    }
                    client->text ("{\"set\":" + itInput->second->serialize () + "}");
                }

                // Re-init I/O modules if their config changed
                if (changed)
                    setMode (input, output);
#endif
            }
        }
    }

    /* From wshandler:
            bool reboot = false;
            switch (data[1]) {
                case '1':   // Set Network Config
                    dsNetworkConfig(json.as<JsonObject>());
                    saveConfig();
                    client->text("S1");
                    break;
                case '2':   // Set Device Config
                    // Reboot if MQTT changed
                    if (config.mqtt != json["mqtt"]["enabled"])
                        reboot = true;

                    dsDeviceConfig(json.as<JsonObject>());
                    saveConfig();

                    if (reboot)
                        client->text("S1");
                    else
                        client->text("S2");
                    break;
                case '3':   // Set Effect Startup Config
                    dsEffectConfig(json.as<JsonObject>());
                    saveConfig();
                    client->text("S3");
                    break;
                case '4':   // Set Gamma (but no save)
                    dsGammaConfig(json.as<JsonObject>());
                    client->text("S4");
                    break;
            }
    */
} // procJSON


//-----------------------------------------------------------------------------
void c_WebMgr::onFirmwareUpload (AsyncWebServerRequest* request, String filename,
          size_t index, uint8_t* data, size_t len, bool final) 
{
    static EFUpdate efupdate; /// EFU Update Handler
    if (!index) 
    {
#ifdef ARDUINO_ARCH_ESP8266
        WiFiUDP::stopAll ();
#else
        // this is not supported for ESP32
#endif
        LOG_PORT.print (F ("* Upload Started: "));
        LOG_PORT.println (filename.c_str ());
        efupdate.begin ();
    }

    if (!efupdate.process (data, len)) 
    {
        LOG_PORT.print (F ("*** UPDATE ERROR: "));
        LOG_PORT.println (String (efupdate.getError ()));
    }

    if (efupdate.hasError ())
    {
        request->send (200, "text/plain", "Update Error: " + String (efupdate.getError ()));
    }

    if (final) 
    {
        request->send (200, "text/plain", "Update Finished: " + String (efupdate.getError ()));
        LOG_PORT.println (F ("* Upload Finished."));
        efupdate.end ();
        SPIFFS.begin ();
        //        saveConfig();
        extern bool reboot;
        reboot = true;
    }
} // onEvent

//-----------------------------------------------------------------------------
// create a global instance of the WEB UI manager
c_WebMgr WebMgr;
