#pragma once
/*
* WiFiMgr.hpp - Output Management class
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

#ifdef ARDUINO_ARCH_ESP8266
#   include <ESP8266WiFi.h>
#else
#   include <WiFi.h>
#endif // def ARDUINO_ARCH_ESP8266

// forward declaration
/*****************************************************************************/
class fsm_WiFi_state
{
public:
    virtual void Poll (void) = 0;
    virtual void Init (void) = 0;
    virtual void GetStateName (String& sName) = 0;
    virtual void OnConnect (void) = 0;
    virtual void OnDisconnect (void) = 0;
}; // fsm_WiFi_state


class c_WiFiMgr
{
public:
    c_WiFiMgr ();
    virtual ~c_WiFiMgr ();

    void      Begin           (config_t* NewConfig); ///< set up the operating environment based on the current config (or defaults)
    int       ValidateConfig  (config_t * NewConfig);
    IPAddress getIpAddress    () { return CurrentIpAddress; }
    void      setIpAddress    (IPAddress NewAddress ) { CurrentIpAddress = NewAddress; }
    IPAddress getIpSubNetMask () { return CurrentSubnetMask; }
    void      setIpSubNetMask (IPAddress NewAddress) { CurrentSubnetMask = NewAddress; }
    void      GetStatus       (JsonObject & jsonStatus);
    void      connectWifi     ();
    void      reset           ();
    void      Poll ();

    void      SetFsmState     (fsm_WiFi_state * NewState) { pCurrentFsmState = NewState; }
    void      AnnounceState   ();
    void      SetFsmStartTime (uint32_t NewStartTime)    { FsmTimerWiFiStartTime = NewStartTime; }
    uint32_t  GetFsmStartTime (void)                     { return FsmTimerWiFiStartTime; }
    config_t* GetConfigPtr    () { return config; }
    bool      IsWiFiConnected ();

private:

#define PollInterval 1000

#ifdef ARDUINO_ARCH_ESP8266
    WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
    WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
#endif
    config_t           *config = nullptr;                           // Current configuration
    IPAddress           CurrentIpAddress  = IPAddress (0, 0, 0, 0);
    IPAddress           CurrentSubnetMask = IPAddress (0, 0, 0, 0);
    time_t              NextPollTime = 0;

    void SetUpIp ();

#ifdef ARDUINO_ARCH_ESP8266
    void onWiFiConnect (const WiFiEventStationModeGotIP& event);
    void onWiFiDisconnect (const WiFiEventStationModeDisconnected& event);
#else
    void onWiFiConnect (const WiFiEvent_t event, const WiFiEventInfo_t info);
    void onWiFiDisconnect (const WiFiEvent_t event, const WiFiEventInfo_t info);
#endif

protected:
    friend class fsm_WiFi_state_Boot;
    friend class fsm_WiFi_state_ConnectingUsingConfig;
    friend class fsm_WiFi_state_ConnectingUsingDefaults;
    friend class fsm_WiFi_state_ConnectedToAP;
    friend class fsm_WiFi_state_ConnectingAsAP;
    friend class fsm_WiFi_state_ConnectedToSta;
    friend class fsm_WiFi_state_ConnectionFailed;
    friend class fsm_WiFi_state;
    fsm_WiFi_state * pCurrentFsmState = nullptr;
    uint32_t         FsmTimerWiFiStartTime = 0;

}; // c_WiFiMgr


/*****************************************************************************/
/*
*	Generic fsm base class.
*/
/*****************************************************************************/
/*****************************************************************************/
// Wait for polling to start.
class fsm_WiFi_state_Boot : public fsm_WiFi_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Boot"); }
    virtual void OnConnect (void)             { /* ignore */ }
    virtual void OnDisconnect (void)          { /* ignore */ }

}; // fsm_WiFi_state_Boot

/*****************************************************************************/
class fsm_WiFi_state_ConnectingUsingConfig : public fsm_WiFi_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting Using Config Credentials"); }
    virtual void OnConnect (void);
    virtual void OnDisconnect (void)          { LOG_PORT.print ("."); }

}; // fsm_WiFi_state_ConnectingUsingConfig

/*****************************************************************************/
class fsm_WiFi_state_ConnectingUsingDefaults : public fsm_WiFi_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting Using Default Credentials"); }
    virtual void OnConnect (void);
    virtual void OnDisconnect (void)          { LOG_PORT.print ("."); }

}; // fsm_WiFi_state_ConnectingUsingConfig

/*****************************************************************************/
class fsm_WiFi_state_ConnectedToAP : public fsm_WiFi_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connected To AP"); }
    virtual void OnConnect (void) {}
    virtual void OnDisconnect (void);

}; // fsm_WiFi_state_ConnectedToAP

/*****************************************************************************/
class fsm_WiFi_state_ConnectingAsAP : public fsm_WiFi_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting As AP"); }
    virtual void OnConnect (void);
    virtual void OnDisconnect (void)          { LOG_PORT.print ("."); }

}; // fsm_WiFi_state_ConnectingAsAP

/*****************************************************************************/
class fsm_WiFi_state_ConnectedToSta : public fsm_WiFi_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connected To STA"); }
    virtual void OnConnect (void) {}
    virtual void OnDisconnect (void);

}; // fsm_WiFi_state_ConnectedToSta

/*****************************************************************************/
class fsm_WiFi_state_ConnectionFailed : public fsm_WiFi_state
{
public:
    virtual void Poll (void) {}
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connection Failed"); }
    virtual void OnConnect (void) {}
    virtual void OnDisconnect (void) {}

}; // fsm_WiFi_state_Rebooting

extern c_WiFiMgr WiFiMgr;
