#pragma once
/*
* WiFiDriver.hpp - Output Management class
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

#ifdef ARDUINO_ARCH_ESP8266
#   include <ESP8266WiFi.h>
#else
#   include <WiFi.h>
#endif // def ARDUINO_ARCH_ESP8266

// forward declaration for the compiler
class c_WiFiDriver;

// forward declaration
/*****************************************************************************/
class fsm_WiFi_state
{
protected:
    c_WiFiDriver* pWiFiDriver = nullptr;
public:
    fsm_WiFi_state() {}
    virtual ~fsm_WiFi_state() {}

    virtual void Poll (void) = 0;
    virtual void Init (void) = 0;
    virtual void GetStateName (String& sName) = 0;
    virtual void OnConnect (void) = 0;
    virtual void OnDisconnect (void) = 0;
    void GetDriverName (String & Name) { Name = CN_WiFiDrv; }
    void SetParent (c_WiFiDriver * parent) { pWiFiDriver = parent; }
}; // fsm_WiFi_state

class c_WiFiDriver
{
public:
    c_WiFiDriver ();
    virtual ~c_WiFiDriver ();

    void Begin ();
    void GetConfig (JsonObject & json);
    void GetStatus (JsonObject & json);
    bool SetConfig (JsonObject & json);

    IPAddress getIpAddress    () { return CurrentIpAddress; }
    void      setIpAddress    (IPAddress NewAddress ) { CurrentIpAddress = NewAddress; }
    IPAddress getIpSubNetMask () { return CurrentSubnetMask; }
    void      setIpSubNetMask (IPAddress NewAddress) { CurrentSubnetMask = NewAddress; }
    void      connectWifi     (const String & ssid, const String & passphrase);
    void      reset           ();
    void      Poll ();

    void      SetFsmState (fsm_WiFi_state* NewState);
    void      AnnounceState   ();
    FastTimer &GetFsmTimer (void) { return FsmTimer; }
    bool      IsWiFiConnected () { return ReportedIsWiFiConnected; }
    void      SetIsWiFiConnected (bool value) { ReportedIsWiFiConnected = value; }
    void      GetDriverName   (String & Name) { Name = CN_WiFiDrv; }
    uint32_t  Get_sta_timeout  () { return sta_timeout; }
    uint32_t  Get_ap_timeout   () { return ap_timeout; }
    bool      Get_ap_fallbackIsEnabled () { return ap_fallbackIsEnabled; }
    bool      Get_ap_StayInApMode () { return StayInApMode; }
    bool      Get_RebootOnWiFiFailureToConnect () { return RebootOnWiFiFailureToConnect; }
    String    GetConfig_ssid () { return ssid; }
    String    GetConfig_apssid () { return ap_ssid; }
    String    GetConfig_passphrase () { return passphrase; }
    void      GetHostname (String& name);
    void      SetHostname (String & name);
    void      Disable ();
    void      Enable ();

private:
#define DEFAULT_SSID_NOT_SET "DEFAULT_SSID_NOT_SET"
    int       ValidateConfig ();

#ifdef ARDUINO_ARCH_ESP8266
    WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
    WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
#endif
    // config_t           *config = nullptr;                           // Current configuration
    IPAddress   CurrentIpAddress  = IPAddress (0, 0, 0, 0);
    IPAddress   CurrentSubnetMask = IPAddress (0, 0, 0, 0);
    FastTimer   NextPoll;
    uint32_t    PollInterval = 1000;
    bool        ReportedIsWiFiConnected = false;

    String      ssid;
    String      passphrase;
    String      ap_ssid;
    String      ap_passphrase;
    IPAddress   ip = INADDR_NONE;
    IPAddress   netmask = INADDR_NONE;
    IPAddress   gateway = INADDR_NONE;
    IPAddress   primaryDns = INADDR_NONE;
    IPAddress   secondaryDns = INADDR_NONE;
    bool        UseDhcp = true;
    uint8_t     ap_channelNumber = 1;
    bool        ap_fallbackIsEnabled = true;
    uint32_t    ap_timeout = AP_TIMEOUT;      ///< How long to wait in AP mode with no connection before rebooting
    uint32_t    sta_timeout = CLIENT_TIMEOUT; ///< Timeout when connection as client (station)
    bool        StayInApMode = false;

#ifdef SUPPORT_ETHERNET
    bool        RebootOnWiFiFailureToConnect = false;
#else
    bool        RebootOnWiFiFailureToConnect = true;
#endif // def SUPPORT_ETHERNET

    void SetUpIp ();

#ifdef ARDUINO_ARCH_ESP8266
    void onWiFiConnect (const WiFiEventStationModeGotIP& event);
    void onWiFiDisconnect (const WiFiEventStationModeDisconnected& event);
#else
    void onWiFiConnect    (const WiFiEvent_t event, const WiFiEventInfo_t info);
    void onWiFiDisconnect (const WiFiEvent_t event, const WiFiEventInfo_t info);

    void onWiFiStaConn    (const WiFiEvent_t event, const WiFiEventInfo_t info);
    void onWiFiStaDisc    (const WiFiEvent_t event, const WiFiEventInfo_t info);
#endif

protected:
    friend class fsm_WiFi_state_Boot;
    friend class fsm_WiFi_state_ConnectingUsingConfig;
    friend class fsm_WiFi_state_ConnectingUsingDefaults;
    friend class fsm_WiFi_state_ConnectedToAP;
    friend class fsm_WiFi_state_ConnectingAsAP;
    friend class fsm_WiFi_state_ConnectedToSta;
    friend class fsm_WiFi_state_ConnectionFailed;
    friend class fsm_WiFi_state_Disabled;
    friend class fsm_WiFi_state;
    fsm_WiFi_state * pCurrentFsmState = nullptr;
    FastTimer        FsmTimer;

}; // c_WiFiDriver


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
    fsm_WiFi_state_Boot() {}
    virtual ~fsm_WiFi_state_Boot() {}

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
    fsm_WiFi_state_ConnectingUsingConfig() {}
    virtual ~fsm_WiFi_state_ConnectingUsingConfig() {}

    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting Using Config Credentials"); }
    virtual void OnConnect (void);
    virtual void OnDisconnect (void) {}

}; // fsm_WiFi_state_ConnectingUsingConfig

/*****************************************************************************/
class fsm_WiFi_state_ConnectingUsingDefaults : public fsm_WiFi_state
{
public:
    fsm_WiFi_state_ConnectingUsingDefaults() {}
    virtual ~fsm_WiFi_state_ConnectingUsingDefaults() {}

    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting Using Default Credentials"); }
    virtual void OnConnect (void);
    virtual void OnDisconnect (void) {}

}; // fsm_WiFi_state_ConnectingUsingConfig

/*****************************************************************************/
class fsm_WiFi_state_ConnectedToAP : public fsm_WiFi_state
{
public:
    fsm_WiFi_state_ConnectedToAP() {}
    virtual ~fsm_WiFi_state_ConnectedToAP() {}

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
    fsm_WiFi_state_ConnectingAsAP() {}
    virtual ~fsm_WiFi_state_ConnectingAsAP() {}

    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting As AP"); }
    virtual void OnConnect (void);
    virtual void OnDisconnect (void) {}

}; // fsm_WiFi_state_ConnectingAsAP

/*****************************************************************************/
class fsm_WiFi_state_ConnectedToSta : public fsm_WiFi_state
{
public:
    fsm_WiFi_state_ConnectedToSta() {}
    virtual ~fsm_WiFi_state_ConnectedToSta() {}

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
    fsm_WiFi_state_ConnectionFailed() {}
    virtual ~fsm_WiFi_state_ConnectionFailed() {}

    virtual void Poll(void) {}
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connection Failed"); }
    virtual void OnConnect (void) {}
    virtual void OnDisconnect (void) {}

}; // fsm_WiFi_state_Rebooting

/*****************************************************************************/
class fsm_WiFi_state_Disabled : public fsm_WiFi_state
{
public:
    fsm_WiFi_state_Disabled() {}
    virtual ~fsm_WiFi_state_Disabled() {}

    virtual void Poll (void) {}
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Disabled"); }
    virtual void OnConnect (void) {}
    virtual void OnDisconnect (void) {}

}; // fsm_WiFi_state_Disabled
