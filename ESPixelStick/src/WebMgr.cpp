/*
* WebMgr.cpp - Output Management class
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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
#include "network/NetworkMgr.hpp"

#include "WebMgr.hpp"
#include "FileMgr.hpp"
#include <Int64String.h>

#include <FS.h>
#include <LittleFS.h>

#include <time.h>
#include <sys/time.h>
#include <functional>

// #define ESPALEXA_DEBUG
#define ESPALEXA_MAXDEVICES 2
#define ESPALEXA_ASYNC //it is important to define this before #include <Espalexa.h>!
#include <Espalexa.h>

const uint8_t HTTP_PORT = 80;      ///< Default web server port

static Espalexa         espalexa;
static EFUpdate         efupdate; /// EFU Update Handler
static AsyncWebServer   webServer (HTTP_PORT);  // Web Server

//-----------------------------------------------------------------------------
void PrettyPrint(DynamicJsonDocument &jsonStuff, String Name)
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

        // Heap status handler
    	webServer.on ("/heap", HTTP_GET | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            request->send (200, CN_textSLASHplain, String (ESP.getFreeHeap ()).c_str());
        });

    	webServer.on ("/XJ", HTTP_POST | HTTP_GET | HTTP_OPTIONS, [this](AsyncWebServerRequest* request)
        {
            ProcessXJRequest (request);
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
                reboot = true;
                request->send (200, CN_textSLASHplain, "Rebooting");
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

                request->send (200, CN_textSLASHplain, "Rebooting");
                // DEBUG_V ("");
                extern bool reboot;
                reboot = true;
            }
        });

        // ping handler
    	webServer.on ("/XP", HTTP_GET | HTTP_POST | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("XP");
            request->send (200, CN_applicationSLASHjson, "{\"pong\":true}");
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
                    AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
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
                    request->send (404, CN_textSLASHplain, "");
                }
            }
        });

    	webServer.on ("/files", HTTP_GET | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
        {
            // DEBUG_V("files");
            if(HTTP_OPTIONS == request->method())
            {
                request->send (200);
            }
            else
            {
                String Response;
                FileMgr.GetListOfSdFiles(Response);
                request->send (200, CN_applicationSLASHjson, Response);
                // DEBUG_V(String("Files: ") + Response);
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
                String filename = request->url ().substring (String ("/file/delete").length ());
                // DEBUG_V (String ("filename: ") + String (filename));
                FileMgr.DeleteSdFile(filename);
                request->send (200);
            }
        });

        // JSON Config Handler
    	webServer.on ("/conf", HTTP_PUT | HTTP_POST | HTTP_OPTIONS,
        	[this](AsyncWebServerRequest* request)
        	{
                if(HTTP_OPTIONS == request->method())
                {
                    request->send (200);
                }
                else
                {
                    // DEBUG_V(String("           url: ") + request->url());
                    String UploadFileName = request->url().substring(6);
                    // DEBUG_V(String("UploadFileName: ") + UploadFileName);
                    // DEBUG_V ("Trigger a config file read");
                    if(UploadFileName.equals(F("config.json")))
                    {
                        extern void loadConfig();
                        loadConfig();
                        request->send (200, CN_textSLASHplain, String(F("XFER Complete")));
                    }
                    else if(UploadFileName.equals(F("input_config.json")))
                    {
                        InputMgr.LoadConfig();
                        request->send (200, CN_textSLASHplain, String(F("XFER Complete")));
                    }
                    else if(UploadFileName.equals(F("output_config.json")))
                    {
                        OutputMgr.LoadConfig();
                        request->send (200, CN_textSLASHplain, String(F("XFER Complete")));
                    }
                    else
                    {
                        logcon(String(F("Unexpected Config File Name: ")) + UploadFileName);
                        request->send (404, CN_textSLASHplain, String(F("File Not supported")));
                    }
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

            	if(FileMgr.SaveConfigFile(filename, index, data, len, final))
                {
                    // DEBUG_V("Save Chunk - Success");
                }
                else
                {
                    // DEBUG_V("Save Chunk - Failed");
                    request->send (404, CN_textSLASHplain, String(F("Could not save data")));
                }
        	},

            [this](AsyncWebServerRequest *request, uint8_t *data, uint32_t len, uint32_t index, uint32_t total)
            {
                // DEBUG_V("Save Data Chunk - Start");
                String UploadFileName = request->url().substring(5);

                // DEBUG_V(String("  url: ") + request->url());
                // DEBUG_V(String("  len: ") + String(len));
                // DEBUG_V(String("index: ") + String(index));
                // DEBUG_V(String("total: ") + String(total));
                // DEBUG_V(String("  sum: ") + String(index + len));
                // DEBUG_V(String(" file: ") + UploadFileName);
                // DEBUG_V(String("final: ") + String(total <= (index+len)));

            	if(FileMgr.SaveConfigFile(UploadFileName, index, data, len, total <= (index+len)))
                {
                    // DEBUG_V("Save Chunk - Success");
                }
                else
                {
                    // DEBUG_V("Save Chunk - Failed");
                    request->send (404, CN_textSLASHplain, String(F("Could not save data")));
                }
            }
        );

        // Firmware upload handler
    	webServer.on ("/updatefw", HTTP_POST, 
            [](AsyncWebServerRequest* request)
            {
                reboot = true;
            }, 
            [](AsyncWebServerRequest* request, String filename, uint32_t index, uint8_t* data, uint32_t len, bool final)
             {WebMgr.FirmwareUpload (request, filename, index, data, len,  final); }).setFilter (ON_STA_FILTER);

    	// URL's needed for FPP Connect fseq uploading and querying
   	 	webServer.on ("/fpp", HTTP_GET,
        	[](AsyncWebServerRequest* request)
        	{
        		FPPDiscovery.ProcessGET(request);
        	});

        webServer.on ("/api/system", HTTP_GET,
        	[](AsyncWebServerRequest* request)
        	{
        		FPPDiscovery.ProcessGET(request);
        	});

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

        // Static Handlers
   	 	webServer.serveStatic ("/UpdRecipe",               LittleFS, "/UpdRecipe.json");
   	 	webServer.serveStatic ("/conf/config.json",        LittleFS, "/config.json");
   	 	webServer.serveStatic ("/conf/input_config.json",  LittleFS, "/input_config.json");
   	 	webServer.serveStatic ("/conf/output_config.json", LittleFS, "/output_config.json");
   	 	webServer.serveStatic ("/conf/admininfo.json",     LittleFS, "/admininfo.json");

        // must be last servestatic entry
    	webServer.serveStatic ("/",                        LittleFS, "/www/").setDefaultFile ("index.html");

        // FS Debugging Handler
        // webServer.serveStatic ("/fs", LittleFS, "/" );

        // if the client posts to the upload page
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
                    	request->send (200);
                    }
                    else
                    {
                    	request->send (404, CN_textSLASHplain, "Page Not found");
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
            	request->send (404, CN_textSLASHplain, "Page Not found");
        	}
    	);

    		webServer.on ("/download", HTTP_GET | HTTP_OPTIONS, [](AsyncWebServerRequest* request)
            {
                if(HTTP_OPTIONS == request->method())
                {
                    request->send (200);
                }
                else
                {

                    // DEBUG_V (String ("url: ") + String (request->url ()));
                    String filename = request->url ().substring (String ("/download").length ());
                    // DEBUG_V (String ("filename: ") + String (filename));

                    AsyncWebServerResponse* response = new AsyncFileResponse (ESP_SDFS, filename, F("application/octet-stream"), true);
                    request->send (response);

            		// DEBUG_V ("Send File Done");
                }
    		});

    		webServer.onNotFound ([this](AsyncWebServerRequest* request)
            {
                // DEBUG_V (String("onNotFound. URL: ") + request->url());
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

    if (0 == WebJsonDoc->capacity ())
    {
        logcon (F ("ERROR: Failed to allocate memory for the GetDeviceOptions web request response."));
    }

    // DEBUG_V ("");
    JsonObject WebOptions = WebJsonDoc->createNestedObject (F ("options"));
    JsonObject JsonDeviceOptions = WebOptions.createNestedObject (CN_device);
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

    DynamicJsonDocument AdminJsonDoc(1024);
    JsonObject jsonAdmin = AdminJsonDoc.createNestedObject (F ("admin"));

    jsonAdmin[CN_version] = VERSION;
    jsonAdmin["built"] = BUILD_DATE;
    jsonAdmin["realflashsize"] = String (ESP.getFlashChipSize ());
    jsonAdmin["BoardName"] = String(BOARD_NAME);
#ifdef ARDUINO_ARCH_ESP8266
    jsonAdmin["arch"] = CN_ESP8266;
    jsonAdmin["flashchipid"] = String (ESP.getChipId (), HEX);
#elif defined (ARDUINO_ARCH_ESP32)
    jsonAdmin["arch"] = CN_ESP32;
    jsonAdmin["flashchipid"] = int64String (ESP.getEfuseMac (), HEX);
#endif

    // write to json file
    if (true == FileMgr.SaveConfigFile (F("/admininfo.json"), AdminJsonDoc))
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

    DynamicJsonDocument WebJsonDoc(STATUS_DOC_SIZE);
    WebJsonDoc.clear ();
    JsonObject status = WebJsonDoc.createNestedObject (CN_status);
    JsonObject system = status.createNestedObject (CN_system);

    system[F ("freeheap")] = ESP.getFreeHeap ();
    system[F ("uptime")] = millis ();
    system[F ("SDinstalled")] = FileMgr.SdCardIsInstalled ();
    system[F ("DiscardedRxData")] = DiscardedRxData;

    // Ask WiFi Stats
    // DEBUG_V ("NetworkMgr.GetStatus");
    NetworkMgr.GetStatus (system);

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

    if(WebJsonDoc.overflowed())
    {
        logcon(F("ERROR: Status Doc is too small"));
    }
    else
    {
        String Temp;
        serializeJson(WebJsonDoc, Temp);
        client->send (200, CN_applicationSLASHjson, Temp);
    }

    // DEBUG_END;

} // ProcessXJRequest

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
            logcon (String(F ("Upload Started: ")) + filename);
            efupdate.begin ();
        }

        // DEBUG_V ("Sending data to efupdate");
        // DEBUG_V (String ("data: 0x") + String (uint32(data), HEX));
        // DEBUG_V (String (" len: ") + String (len));

        if (!efupdate.process (data, len))
        {
            logcon (String(CN_stars) + F (" UPDATE ERROR: ") + String (efupdate.getError ()));
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
            logcon (F ("Upload Finished."));
            efupdate.end ();
            // LittleFS.begin ();

            extern bool reboot;
            reboot = true;
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
