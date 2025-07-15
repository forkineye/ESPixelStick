/*
* WebMgr.cpp - Output Management class
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
*/

#include "WebMgr.hpp"
#include "FileMgr.hpp"

#include "input/InputMgr.hpp"
#include "service/FPPDiscovery.h"
#include "network/NetworkMgr.hpp"

#include <Int64String.h>

#include <FS.h>
#include <LittleFS.h>

#ifdef SUPPORT_SENSOR_DS18B20
#include "service/SensorDS18B20.h"
#endif // def SUPPORT_SENSOR_DS18B20

// #define ESPALEXA_DEBUG
#define ESPALEXA_MAXDEVICES 2
#define ESPALEXA_ASYNC //it is important to define this before #include <Espalexa.h>!
#include <Espalexa.h>

const uint8_t HTTP_PORT = 80;      ///< Default web server port

static Espalexa         espalexa;
static EspalexaDevice   AlexaDevice;
static EFUpdate         efupdate; /// EFU Update Handler
static AsyncWebServer   webServer (HTTP_PORT);  // Web Server

//-----------------------------------------------------------------------------
void PrettyPrint(JsonDocument &jsonStuff, String Name)
{
    // DEBUG_V ("---------------------------------------------");
    LOG_PORT.println (String (F ("---- Pretty Print: '")) + Name + "'");
    serializeJson (jsonStuff, LOG_PORT);
    LOG_PORT.println ("");
        // DEBUG_V ("---------------------------------------------");

} // PrettyPrint

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
    // this gets called pre-setup so there is little we can do here.

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
    do // once
    {
        if (NetworkMgr.IsConnected())
        {
            init();
        }

    } while (false);

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
    if(!HasBeenInitialized)
    {
        // DEBUG_START;
        // Add header for SVG plot support?
    	DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Origin"),  "*");
    	DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Headers"), "append, delete, entries, foreach, get, has, keys, set, values, Authorization, Content-Type, Content-Range, Content-Disposition, Content-Description, cache-control, x-requested-with");
    	DefaultHeaders::Instance ().addHeader (F ("Access-Control-Allow-Methods"), "GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH");

        // Setup WebSockets
        using namespace std::placeholders;

        // Static Handlers
   	 	webServer.serveStatic ("/UpdRecipe/", LittleFS, "/UpdRecipe.json");

        // Heap status handler
    	webServer.on ("/heap", HTTP_GET | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            request->send (200, CN_textSLASHplain, String (ESP.getFreeHeap ()).c_str());
        });

    	webServer.on ("/XJ", HTTP_POST | HTTP_GET | HTTP_OPTIONS, [this](AsyncWebServerRequest* request)
        {
            ProcessXJRequest (request);
        });

    	webServer.on ("/settime", HTTP_POST | HTTP_GET | HTTP_OPTIONS, [this](AsyncWebServerRequest* request)
        {
            // DEBUG_V("/settime");
            // DEBUG_V(String("URL: ") + request->url());
            if(HTTP_OPTIONS == request->method())
            {
                // DEBUG_V("Options request");
                request->send (200);
            }
            else
            {
                // String url = request->url ();
                // DEBUG_V (String ("url: ") + String (url));
                const AsyncWebParameter * pParam = request->getParam("newtime");
                // DEBUG_V (String (" param Name: ") + String (pParam->name()));
                // DEBUG_V (String ("param Value: ") + String (pParam->value()));

                String newDate = pParam->value();
                // DEBUG_V (String ("newDate: ") + String (newDate));
                ProcessSetTimeRequest (time_t(newDate.toInt()));
                request->send (200, CN_applicationSLASHjson, F("{\"status\":\"OK\"}"));
            }
        });

        // Reboot handler
    	webServer.on ("/X6", HTTP_POST | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("X6")
            if(HTTP_OPTIONS == request->method())
            {
                request->send (200);
            }
            else
            {
                request->send (200, CN_applicationSLASHjson, F("{\"status\":\"Rebooting\"}"));

                String Reason = F("Browser X6 Reboot requested.");
                RequestReboot(Reason, 100000);
            }
        });

        // Reboot handler
    	webServer.on ("/X7", HTTP_POST | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("X7")
            if(HTTP_OPTIONS == request->method())
            {
                request->send (200);
            }
            else
            {
                extern void DeleteConfig ();
                // DEBUG_V ("");
                InputMgr.DeleteConfig ();
                OutputMgr.DeleteConfig ();
                DeleteConfig ();

                request->send (200, CN_applicationSLASHjson, F("{\"status\":\"Rebooting\"}"));
                // DEBUG_V ("");
                String Reason = F("Browser requested Reboot");
                RequestReboot(Reason, 100000);;
            }
        });

    	webServer.on ("/relay", HTTP_GET | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("relay")
            if(HTTP_OPTIONS == request->method())
            {
                request->send (200);
            }
            else
            {
                // DEBUG_V ("relay");
                // DEBUG_V(String("url: ") + request->url ());
                String RawRelayId = request->url ().substring (String (F("/relay/")).length ());
                // DEBUG_V(String("RawRelayId: '") + RawRelayId + "'");
                int EndIdPos = RawRelayId.indexOf('/');
                // DEBUG_V(String("EndIdPos: '") + String(EndIdPos) + "'");
                if(-1 != EndIdPos)
                {
                    String RelayId = RawRelayId.substring(0, EndIdPos);
                    // DEBUG_V(String("RelayId: '") + RelayId + "'");

                    String RelayState = RawRelayId.substring(EndIdPos+1);
                    // DEBUG_V(String("RelayState: '") + RelayState + "'");
                    String Response = emptyString;
                    OutputMgr.RelayUpdate(RelayId.toInt(), RelayState, Response);

                    request->send (Response.equalsIgnoreCase("OK") ? 200 : 401, CN_textSLASHplain, Response);
                }
                else
                {
                    request->send (200, CN_applicationSLASHjson, F("{\"status\":\"No Relay ID Found\"}"));
                }

                // DEBUG_V ("relay");
            }
        });

        // ping handler
    	webServer.on ("/XP", HTTP_GET | HTTP_POST | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("XP");
            request->send (200, CN_applicationSLASHjson, F("{\"pong\":true}"));
        });

    	webServer.on ("/V1", HTTP_GET | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("V1");
            if(HTTP_OPTIONS == request->method())
            {
                request->send (200);
            }
            else
            {
                if (OutputMgr.GetBufferUsedSize ())
                {
                    AsyncWebServerResponse *response = request->beginChunkedResponse("application/octet-stream",
                        [](uint8_t *buffer, size_t MaxChunkLen, size_t index) -> size_t
                        {
                            // Write up to "MaxChunkLen" bytes into "buffer" and return the amount written.
                            // index equals the amount of bytes that have been already sent
                            // You will be asked for more data until 0 is returned
                            // Keep in mind that you can not delay or yield waiting for more data!

                            // DEBUG_V(String("            MaxChunkLen: ") + String(MaxChunkLen));
                            // DEBUG_V(String("      GetBufferUsedSize: ") + String(OutputMgr.GetBufferUsedSize ()));
                            // DEBUG_V(String("                  index: ") + String(index));
                            size_t NumBytesAvailableToSend = min(MaxChunkLen, (OutputMgr.GetBufferUsedSize () - index));
                            // DEBUG_V(String("NumBytesAvailableToSend: ") + String(NumBytesAvailableToSend));
                            if(0 != NumBytesAvailableToSend)
                            {
                                memcpy(buffer, OutputMgr.GetBufferAddress () + index, NumBytesAvailableToSend);
                            }
                            return NumBytesAvailableToSend;
                        });
                    response->setContentLength(OutputMgr.GetBufferUsedSize ());
                    response->setContentType(F("application/octet-stream"));
                    response->addHeader(F("server"), F("ESPS Diag Data"));
                    request->send(response);
                }
                else
                {
                    AsyncWebServerResponse *response = request->beginChunkedResponse("application/octet-stream",
                        [](uint8_t *buffer, size_t MaxChunkLen, size_t index) -> size_t
                        {
                            return 0;
                        });
                    response->setContentLength(0);
                    response->setContentType(F("application/octet-stream"));
                    response->addHeader(F("server"), F("ESPS Diag Data"));
                    request->send(response);
                }
            }
        });

    	webServer.on ("/file/delete", HTTP_POST | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("/file/delete");
            // DEBUG_V(String("URL: ") + request->url());
            if(HTTP_OPTIONS == request->method())
            {
                request->send (200);
            }
            else
            {
                // DEBUG_V (String ("url: ") + String (request->url ()));
                String filename = request->url ().substring (String (F("/file/delete")).length ());
                // DEBUG_V (String ("filename: ") + String (filename));
                FileMgr.DeleteSdFile(filename);
                    request->send (200, CN_applicationSLASHjson, F("{\"status\":\"Deleted\"}"));
            }
        });

    	webServer.on ("/fseqfilelist", HTTP_GET,
        	[this](AsyncWebServerRequest* request)
        	{
                // DEBUG_V("Process: HTTP_GET /fseqfilelist");
                request->send (LittleFS, F("/fseqfilelist.json"), CN_applicationSLASHjson);
            }
        );

        // JSON Config Handler
    	webServer.on ("/conf", HTTP_GET,
        	[this](AsyncWebServerRequest* request)
        	{
                String RequestFileName = String("/") + request->url().substring(6);
                // DEBUG_V(String("RequestFileName: ") + RequestFileName);
                RequestFileName.replace("//", "/");
                RequestFileName.replace(".gz", "");
                // DEBUG_V(String("RequestFileName: ") + RequestFileName);
                request->send (LittleFS, RequestFileName, CN_applicationSLASHjson);
            }
        );

        webServer.on ("/conf", HTTP_PUT | HTTP_POST | HTTP_OPTIONS,
        	[this](AsyncWebServerRequest* request)
        	{
                String RequestFileName = request->url().substring(6);
                WebRequestMethodComposite RequestMethod = request->method();
                // DEBUG_V(String("  RequestMethod: ") + String(RequestMethod));
                // DEBUG_V(String("RequestFileName: ") + RequestFileName);
                if(HTTP_OPTIONS == RequestMethod)
                {
                    request->send (200);
                }
                else if(HTTP_GET == RequestMethod)
                {
                    // DEBUG_V("Process: HTTP_GET");
                    request->send (401, CN_applicationSLASHjson, F("{\"status\":\"File Not supported\"}"));
                }
                else if((HTTP_POST == RequestMethod) ||
                        (HTTP_PUT  == RequestMethod))
                {
                    // DEBUG_V(String("           url: ") + request->url());
                    FileMgr.RenameFlashFile(RequestFileName + ".tmp", RequestFileName);
                    if(RequestReadConfigFile(RequestFileName))
                    {
                        request->send (200, CN_applicationSLASHjson, F("{\"status\":\"XFER Complete\"}"));
                        DelayReboot(10000);
                    }
                    else
                    {
                        // delete unexpected config file.
                        FileMgr.DeleteFlashFile(RequestFileName + ".tmp");
                        request->send (404, CN_applicationSLASHjson, F("{\"status\":\"File Not supported\"}"));
                    }
                }
                else
                {
                    // DEBUG_V("Unsupported HTTP type");
                    request->send (401, CN_applicationSLASHjson, F("{\"status\":\"HTTP_TYPE Not supported\"}"));
                }
            },

        	[this](AsyncWebServerRequest *request, String filename, uint32_t index, uint8_t *data, uint32_t len, bool final)
        	{
                // DEBUG_V("Save File Chunk - Start");
                // DEBUG_V(String("  url: ") + request->url());
                // DEBUG_V(String("  len: ") + String(len));
                // DEBUG_V(String("index: ") + String(index));
                // DEBUG_V(String("  sum: ") + String(index + len));
                // DEBUG_V(String(" file: ") + filename);
                // DEBUG_V(String("final: ") + String(final));
                DelayReboot(10000);
                if(0 == index)
                {
                    // DEBUG_V("Deleting config file.");
                    FileMgr.DeleteFlashFile(filename);
                }

                if(FileMgr.SaveFlashFile(filename + ".tmp", index, data, len, final))
                {
                    // DEBUG_V("Save Chunk - Success");
                }
                else
                {
                    logcon(String(F("Upload failed for: ")) + filename);
                    // remove the incomplete file
                    FileMgr.DeleteFlashFile(filename + ".tmp");
                    // This will cause the defaults to be recreated.
                    RequestReadConfigFile(filename);

                    // DEBUG_V("Save Chunk - Failed");
                    request->send (404, CN_applicationSLASHjson, F("{\"status\":\"Could not save data\"}"));
                }
        	},

            [this](AsyncWebServerRequest *request, uint8_t *data, uint32_t len, uint32_t index, uint32_t total)
            {
                // DEBUG_V("Save Data Chunk - Start");
                String UploadFileName = request->url().substring(5);
                DelayReboot(10000);

                // DEBUG_V(String("  url: ") + request->url());
                // DEBUG_V(String("  len: ") + String(len));
                // DEBUG_V(String("index: ") + String(index));
                // DEBUG_V(String("total: ") + String(total));
                // DEBUG_V(String("  sum: ") + String(index + len));
                // DEBUG_V(String(" file: ") + UploadFileName);
                // DEBUG_V(String("final: ") + String(total <= (index+len)));

                if(0 == index)
                {
                    // DEBUG_V("Deleting config file.");
                    FileMgr.DeleteFlashFile(UploadFileName);
                }

            	if(FileMgr.SaveFlashFile(UploadFileName + ".tmp", index, data, len, total <= (index+len)))
                {
                    // DEBUG_V("Save Chunk - Success");
                }
                else
                {
                    // remove the incomplete file
                    FileMgr.DeleteFlashFile(String("/") + UploadFileName + ".tmp");
                    logcon(String(F("Upload failed for: ")) + UploadFileName);
                    // This will cause the defaults to be recreated.
                    RequestReadConfigFile(UploadFileName);

                    // DEBUG_V("Save Chunk - Failed");
                    request->send (404, CN_applicationSLASHjson, F("{\"status\":\"Could not save data\"}"));
                }
            }
        );

        // Firmware upload handler
    	webServer.on ("/updatefw", HTTP_POST,
            [](AsyncWebServerRequest* request)
            {
                // DEBUG_V("Client requested reboot");
                if (WebMgr.efupdate.hasError ())
                {
                    // DEBUG_V ("efupdate.hasError, ignoring reboot request");
                    String ErrorMsg;
                    WebMgr.efupdate.getError (ErrorMsg);
                    request->send (500, CN_applicationSLASHjson, String(F("{\"status\":\"Update Error: ")) + ErrorMsg + F(" \"}"));
                }
                else
                {
                    // DEBUG_V ("efupdate.hasError == false");
                    String Reason = F("EFU Update Success");
                    request->send (200, CN_applicationSLASHjson, F("{\"status\":\"EFU Update Success\"}"));
                    RequestReboot(Reason, 100000);
                }
            },
            [](AsyncWebServerRequest* request, String filename, uint32_t index, uint8_t* data, uint32_t len, bool final)
             {WebMgr.FirmwareUpload (request, filename, index, data, len, final); }); //.setFilter (ON_STA_FILTER);

    	// URL's needed for FPP Connect fseq uploading and querying
   	 	webServer.on ("/fpp", HTTP_GET,
        	[](AsyncWebServerRequest* request)
        	{
        		FPPDiscovery.ProcessGET(request);
        	});

    	// URL's needed for FPP Connect fseq uploading and querying
        webServer.on ("/api/system", HTTP_GET,
        	[](AsyncWebServerRequest* request)
        	{
        		FPPDiscovery.ProcessGET(request);
        	});

    	// URL's needed for FPP Connect fseq uploading and querying
    	webServer.on ("/fpp", HTTP_POST | HTTP_PUT,
        	[](AsyncWebServerRequest* request)
        	{
            	FPPDiscovery.ProcessPOST(request);
        	},

        	[](AsyncWebServerRequest *request, String filename, uint32_t index, uint8_t *data, uint32_t len, bool final)
        	{
            	FPPDiscovery.ProcessFile(request, filename, index, data, len, final);
        	},

            [](AsyncWebServerRequest *request, uint8_t *data, uint32_t len, uint32_t index, uint32_t total)
            {
                FPPDiscovery.ProcessBody(request, data, len, index, total);
            });

        // URL that FPP's status pages use to grab JSON about the current status, what's playing, etc...
        // This can be used to mimic the behavior of actual FPP remotes
    	webServer.on ("/fppjson.php", HTTP_GET, [](AsyncWebServerRequest* request)
            {
                FPPDiscovery.ProcessFPPJson(request);
            });

    	// URL's needed for FPP Connect fseq uploading and querying
    	webServer.on ("/api/fppd", HTTP_GET, [](AsyncWebServerRequest* request)
            {
                FPPDiscovery.ProcessFPPDJson(request);
            });

    	// URL's needed for FPP Connect fseq uploading and querying
    	webServer.on ("/api/channel", HTTP_GET, [](AsyncWebServerRequest* request)
            {
                FPPDiscovery.ProcessFPPDJson(request);
            });

    	// URL's needed for FPP Connect fseq uploading and querying
    	webServer.on ("/api/playlists", HTTP_GET, [](AsyncWebServerRequest* request)
            {
                FPPDiscovery.ProcessFPPDJson(request);
            });

    	// URL's needed for FPP Connect fseq uploading and querying
    	webServer.on ("/api/cape", HTTP_GET, [](AsyncWebServerRequest* request)
            {
                FPPDiscovery.ProcessFPPDJson(request);
            });

    	// URL's needed for FPP Connect fseq uploading and querying
    	webServer.on ("/api/proxies", HTTP_GET, [](AsyncWebServerRequest* request)
            {
                FPPDiscovery.ProcessFPPDJson(request);
            });

        // must be last servestatic entry
    	webServer.serveStatic ("/", LittleFS, "/www/").setDefaultFile ("index.html");

        // FS Debugging Handler
        // webServer.serveStatic ("/fs", LittleFS, "/" );

        // if the client posts to the file upload page
    	webServer.on ("/upload", HTTP_POST | HTTP_PUT | HTTP_OPTIONS,
        	[](AsyncWebServerRequest * request)
            {
                if(HTTP_OPTIONS == request->method())
                {
                    request->send (200);
                }
                else
                {
                    // DEBUG_V ("Got upload post request");
                    if (true == FileMgr.SdCardIsInstalled())
                    {
                        // Send status 200 (OK) to tell the client we are ready to receive
                        request->send (200, CN_applicationSLASHjson, F("{\"status\":\"OK\"}"));
                    }
                    else
                    {
                        request->send (404, CN_applicationSLASHjson, F("{\"status\":\"Page Not found\"}"));
                    }
                }
            },

        	[this](AsyncWebServerRequest* request, String filename, uint32_t index, uint8_t* data, uint32_t len, bool final)
            {
                // DEBUG_V (String ("Got process File request: index: ") + String (index));
                // DEBUG_V (String ("Got process File request: len:   ") + String (len));
                // DEBUG_V (String ("Got process File request: final: ") + String (final));
                FPPDiscovery.ProcessFile (request, filename, index, data, len, final);
            },

        	[this](AsyncWebServerRequest* request, uint8_t* data, uint32_t len, uint32_t index, uint32_t total)
            {
                // DEBUG_V (String ("Got process Body request: index: ") + String (index));
                // DEBUG_V (String ("Got process Body request: len:   ") + String (len));
                // DEBUG_V (String ("Got process Body request: total: ") + String (total));
                request->send (404, CN_applicationSLASHjson, F("{\"status\":\"Page Not found\"}"));
        	}
    	);

        webServer.onNotFound ([this](AsyncWebServerRequest* request)
            {
                // DEBUG_V (String("onNotFound. URL: ") + request->url());
                if (request->method() == HTTP_OPTIONS)
                {
                    request->send(200);
                }
                else if (true == this->IsAlexaCallbackValid())
                {
                    // DEBUG_V ("IsAlexaCallbackValid == true");
                    if (!espalexa.handleAlexaApiCall (request)) //if you don't know the URI, ask espalexa whether it is an Alexa control request
                    {
                        // DEBUG_V ("Alexa Callback could not resolve the request");
                        request->send (404, CN_applicationSLASHjson, F("{\"status\":\"Page Not found\"}"));
                    }
                }
                else
                {
                    // DEBUG_V ("IsAlexaCallbackValid == false");
                    request->send (404, CN_applicationSLASHjson, F("{\"status\":\"Page Not found\"}"));
        		}
            });

    	//give espalexa a pointer to the server object so it can use your server instead of creating its own
    	espalexa.begin (&webServer);

        // webServer.begin ();

        pAlexaDevice = new((char*)&AlexaDevice) EspalexaDevice (String (F ("ESP")), [this](EspalexaDevice* pDevice)
        {
            this->onAlexaMessage (pDevice);

        }, EspalexaDeviceType::extendedcolor);

    	pAlexaDevice->setName (config.id);
    	espalexa.addDevice (pAlexaDevice);
    	espalexa.setDiscoverable ((nullptr != pAlexaCallback) ? true : false);

    	logcon (String (F ("Web server listening on port ")) + HTTP_PORT);

        HasBeenInitialized = true;
    }
    // DEBUG_END;
}

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

#ifdef SUPPORT_DEVICE_OPTION_LIST
//-----------------------------------------------------------------------------
void c_WebMgr::GetDeviceOptions ()
{
    // DEBUG_START;
    // set up a framework to get the option data
    WebJsonDoc->clear ();

    // DEBUG_V ("");
    JsonObject WebOptions = (*WebJsonDoc)[F ("options")].to<JsonObject>();
    JsonObject JsonDeviceOptions = WebOptions[CN_device].to<JsonObject> ();
    // DEBUG_V("");

    // PrettyPrint (WebOptions);

    // PrettyPrint (WebOptions);

    // now make it something we can transmit
    uint32_t msgOffset = strlen (WebSocketFrameCollectionBuffer);
    serializeJson(WebOptions, &pWebSocketFrameCollectionBuffer[msgOffset], (WebSocketFrameCollectionBufferSize - msgOffset));

    // DEBUG_END;

} // GetDeviceOptions
#endif // def SUPPORT_DEVICE_OPTION_LIST

//-----------------------------------------------------------------------------
void c_WebMgr::CreateAdminInfoFile ()
{
    // DEBUG_START;

    JsonDocument AdminJsonDoc;
    AdminJsonDoc.to<JsonObject>();
    JsonObject jsonAdmin = AdminJsonDoc[F ("admin")].to<JsonObject> ();

    JsonWrite(jsonAdmin, CN_version, ConstConfig.Version);
    JsonWrite(jsonAdmin, "built", ConstConfig.BuildDate);
    JsonWrite(jsonAdmin, "realflashsize", String (ESP.getFlashChipSize ()));
    JsonWrite(jsonAdmin, "BoardName", String(BOARD_NAME));
#ifdef ARDUINO_ARCH_ESP8266
    JsonWrite(jsonAdmin, "arch", String(CN_ESP8266));
    JsonWrite(jsonAdmin, "flashchipid", String (ESP.getChipId (), HEX));
#elif defined (ARDUINO_ARCH_ESP32)
    JsonWrite(jsonAdmin, "arch", String(CN_ESP32));
    JsonWrite(jsonAdmin, "flashchipid", int64String (ESP.getEfuseMac (), HEX));
#endif

    // write to json file
    if (true == FileMgr.SaveFlashFile (F("/admininfo.json"), AdminJsonDoc))
    {
    } // end we saved a config and it was good
    else
    {
        logcon (CN_stars + String (F("Failed to save admininfo.json file")) + CN_stars);
    }

    // DEBUG_END;
} // CreateAdminInfoFile

//-----------------------------------------------------------------------------
void c_WebMgr::ProcessXJRequest (AsyncWebServerRequest* client)
{
    // DEBUG_START;

    // WebJsonDoc.clear ();
    JsonDocument WebJsonDoc;
    WebJsonDoc.to<JsonObject>();
    JsonObject status = WebJsonDoc[(char*)CN_status].to<JsonObject> ();
    JsonObject system = status[(char*)CN_system].to<JsonObject> ();

    JsonWrite(system, F ("freeheap"), ESP.getFreeHeap ());
    JsonWrite(system, F ("uptime"), millis ());
    JsonWrite(system, F ("currenttime"), now ());
    JsonWrite(system, F ("SDinstalled"), FileMgr.SdCardIsInstalled ());
    JsonWrite(system, F ("DiscardedRxData"), DiscardedRxData);

    JsonObject HeapDetails = system[F("HeapDetails")].to<JsonObject> ();
#ifdef ARDUINO_ARCH_ESP32
    JsonWrite(HeapDetails, F ("n804_Free_Max"),  heap_caps_get_largest_free_block(0x804));
    JsonWrite(HeapDetails, F ("n804_Free_Tot"),  heap_caps_get_free_size(0x804));
    JsonWrite(HeapDetails, F ("n80C_Free_Max"),  heap_caps_get_largest_free_block(0x80C));
    JsonWrite(HeapDetails, F ("n80C_Free_Tot"),  heap_caps_get_free_size(0x80C));
    JsonWrite(HeapDetails, F ("n1800_Free_Max"), heap_caps_get_largest_free_block(0x1800));
    JsonWrite(HeapDetails, F ("n1800_Free_Tot"), heap_caps_get_free_size(0x1800));
#else
    JsonWrite(HeapDetails, F ("n804_Free_Max"),  ESP.getMaxFreeBlockSize());
    JsonWrite(HeapDetails, F ("n804_Free_Tot"),  ESP.getFreeHeap());
#endif // def ARDUINO_ARCH_ESP32

    // Ask WiFi Stats
    // DEBUG_V ("NetworkMgr.GetStatus");
    NetworkMgr.GetStatus (system);

#ifdef SUPPORT_SENSOR_DS18B20
    // DEBUG_V ("SensorDS18B20.GetStatus");
    SensorDS18B20.GetStatus(system);
#endif // def SUPPORT_SENSOR_DS18B20

    // DEBUG_V ("FPPDiscovery.GetStatus");
    FPPDiscovery.GetStatus (system);

    // DEBUG_V ("InputMgr.GetStatus");
    // Ask Input Stats
    InputMgr.GetStatus (status);

    // Ask Output Stats
    // DEBUG_V ("OutputMgr.GetStatus");
    OutputMgr.GetStatus (status);

    // Get File Manager Stats
    // DEBUG_V ("FileMgr.GetStatus");
    FileMgr.GetStatus (system);
    // DEBUG_V ("");

    // DEBUG_V("Send XJ response");
    String XjResult;
    serializeJson(WebJsonDoc, XjResult);
    client->send (200, CN_applicationSLASHjson, XjResult);

    // WebJsonDoc.clear();
    // DEBUG_END;

} // ProcessXJRequest

//-----------------------------------------------------------------------------
void c_WebMgr::ProcessSetTimeRequest (time_t EpochTime)
{
    // DEBUG_START;

    // DEBUG_V(String("EpochTime: ") + String(EpochTime));
    setTime(EpochTime);
    // FileMgr.BuildFseqList();
    // DEBUG_V(String("now: ") + String(now()));

    // DEBUG_END;
} // ProcessSetTimeRequest

//-----------------------------------------------------------------------------
bool c_WebMgr::RequestReadConfigFile(String & FileName)
{
    // DEBUG_START;
    bool response = true;

    // DEBUG_V(String("FileName: ") + FileName);
    // DEBUG_V("Scheduling config reload.");

    if(FileName.equals(F("config.json")))
    {
        extern void ScheduleLoadConfig();
        ScheduleLoadConfig();
    }
    else if(FileName.equals(F("input_config.json")))
    {
        InputMgr.ScheduleLoadConfig();
    }
    else if(FileName.equals(F("output_config.json")))
    {
        OutputMgr.ScheduleLoadConfig();
    }
    else
    {
        logcon(String(F("Unexpected Config File Name: ")) + FileName);
        response = false;
    }

    // DEBUG_END;
    return response;

} // RequestReadConfigFile

//-----------------------------------------------------------------------------
void c_WebMgr::FirmwareUpload (AsyncWebServerRequest* request,
                               String filename,
                               uint32_t index,
                               uint8_t * data,
                               uint32_t len,
                               bool final)
{
    // DEBUG_START;

    do // once
    {
        // DEBUG_V (String (" file: '") + filename + "'");
        // DEBUG_V (String ("index: ") + String (index));
        // DEBUG_V (String (" data: 0x") + String (uint32_t(data), HEX));
        // DEBUG_V (String ("  len: ") + String (len));
        // DEBUG_V (String ("final: ") + String (final));
        if (efupdate.hasError ())
        {
            // logcon (String(CN_stars) + F (" UPDATE ERROR: ") + String (efupdate.getError ()));
            // DEBUG_V ("efupdate.hasError");
            String ErrorMsg;
            WebMgr.efupdate.getError (ErrorMsg);
            request->send (500, CN_applicationSLASHjson, String(F("{\"status\":\"Update Error: ")) + ErrorMsg + F("\"}"));
            break;
        }

        // is the first message in the upload?
        if (0 == index)
        {
            if(efupdate.UpdateIsInProgress())
            {
                logcon(F("An update is already in progress"));
                // send back an error response
                request->send (429, CN_applicationSLASHjson, String(F("{\"status\":\"Update Error: Too many requests\"}")));
                break;
            }

#ifdef ARDUINO_ARCH_ESP8266
            WiFiUDP::stopAll ();
#else
            // this is not supported for ESP32
#endif
            logcon (String(F ("Upload Started: ")) + filename);
            // stop all input and output processing of intensity data.
            // InputMgr.SetOperationalState(false);
            // OutputMgr.PauseOutputs(true);

            // start the update
            efupdate.begin ();
            if (efupdate.hasError ())
            {
                // logcon (String(CN_stars) + F (" UPDATE ERROR: ") + String (efupdate.getError ()));
                // DEBUG_V ("efupdate.hasError");
                String ErrorMsg;
                WebMgr.efupdate.getError (ErrorMsg);
                request->send (500, CN_applicationSLASHjson, String(F("{\"status\":\"Update Error: ")) + ErrorMsg + F("\"}"));
                break;
            }
        }

        // DEBUG_V ("Sending data to efupdate");

        efupdate.process (data, len);
        // DEBUG_V ("Packet has been processed");

        if (efupdate.hasError ())
        {
            // logcon (String(CN_stars) + F (" UPDATE ERROR: ") + String (efupdate.getError ()));
            // DEBUG_V ("efupdate.hasError");
            String ErrorMsg;
            WebMgr.efupdate.getError (ErrorMsg);
            request->send (500, CN_applicationSLASHjson, String(F("{\"status\":\"Update Error: ")) + ErrorMsg + F("\"}"));
            break;
        }
        // DEBUG_V ("No EFUpdate Error");

        if (final)
        {
            request->send (200, CN_applicationSLASHjson, F("{\"status\":\"Update Finished\""));
            String Reason = (F ("EFU Upload Finished. Rebooting"));
            efupdate.end ();
            RequestReboot(Reason, 100000);
        }

    } while (false);

    // DEBUG_END;

} // onEvent

//-----------------------------------------------------------------------------
/*
 * This function is called as part of the Arduino "loop" and does things that need
 * periodic poking.
 *
 */
void c_WebMgr::Process ()
{
    if(HasBeenInitialized)
    {
        if (true == IsAlexaCallbackValid())
        {
        	espalexa.loop ();
        }
    }
} // Process

//-----------------------------------------------------------------------------
// create a global instance of the WEB UI manager
c_WebMgr WebMgr;
