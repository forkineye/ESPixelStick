#pragma once
/*
* EthernetMgr.hpp - Output Management class
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

#include "../ESPixelStick.h"
#ifdef SUPPORT_ETHERNET

#include <Eth.h>

// forward declaration
/*****************************************************************************/
class fsm_Eth_state
{
public:
    virtual void Poll (void) = 0;
    virtual void Init (void) = 0;
    virtual void GetStateName (String& sName) = 0;
    virtual void OnConnect (void) = 0;
    virtual void OnDisconnect (void) = 0;
}; // fsm_Eth_state

class c_EthernetMgr
{
public:
    c_EthernetMgr ();
    virtual ~c_EthernetMgr ();

    void      Begin           (); ///< set up the operating environment based on the current config (or defaults)
    int       ValidateConfig  ();
    IPAddress getIpAddress    () { return CurrentIpAddress; }
    void      setIpAddress    (IPAddress NewAddress ) { CurrentIpAddress = NewAddress; }
    IPAddress getIpSubNetMask () { return CurrentSubnetMask; }
    void      setIpSubNetMask (IPAddress NewAddress) { CurrentSubnetMask = NewAddress; }
    String    getMacAddress   () { return CurrentMacAddress; }
    void      setMacAddress   (String NewAddress ) { CurrentMacAddress = NewAddress; }
    String    getHostname     () { return CurrentHostname; }
    void      setHostname     (String NewHostname ) { CurrentHostname = NewHostname; }
    void      GetStatus       (JsonObject & jsonStatus);
    void      connectEth      ();
    void      connectEth      (const String & ssid, const String & passphrase);
    void      reset           ();
    void      Poll ();

    void      SetFsmState         (fsm_Eth_state * NewState) { pCurrentFsmState = NewState; }
    void      AnnounceState       ();
    void      SetFsmStartTime     (uint32_t NewStartTime)    { FsmTimerEthStartTime = NewStartTime; }
    uint32_t  GetFsmStartTime     (void)                     { return FsmTimerEthStartTime; }
    bool      IsConnected         () {return IsEthConnected() || IsEthConnected();}
    void      NetworkStateChanged (bool NetworkState);

private:

#define PollInterval 1000

    IPAddress           CurrentIpAddress  = IPAddress (0, 0, 0, 0);
    IPAddress           CurrentSubnetMask = IPAddress (0, 0, 0, 0);
    String              CurrentMacAddress = "";
    time_t              NextPollTime = 0;
    bool                ReportedIsEthConnected = false;

    IPAddress   ip = (uint32_t)0;
    IPAddress   netmask = (uint32_t)0;
    IPAddress   gateway = (uint32_t)0;
    bool        UseDhcp = true;           ///< Use DHCP?

    void SetUpIp ();

    void onEthConnect    (const WiFiEvent_t event, const WiFiEventInfo_t info);
    void onEthDisconnect (const WiFiEvent_t event, const WiFiEventInfo_t info);

protected:
    friend class fsm_Eth_state_Boot;
    friend class fsm_Eth_state_ConnectingToEth;
    friend class fsm_Eth_state_ConnectedToEth;
    friend class fsm_Eth_state;
    fsm_Eth_state * pCurrentFsmState = nullptr;
    uint32_t        FsmTimerEthStartTime = 0;

}; // c_EthernetMgr

/*****************************************************************************/
/*
*	Generic fsm base class.
*/
/*****************************************************************************/
/*****************************************************************************/
// Wait for polling to start.
class fsm_Eth_state_Boot : public fsm_Eth_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Boot"); }
    virtual void OnConnect (void)             { /* ignore */ }
    virtual void OnDisconnect (void)          { /* ignore */ }

}; // fsm_Eth_state_Boot

/*****************************************************************************/
class fsm_Eth_state_ConnectingToEth : public fsm_Eth_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting to Ethernet"); }
    virtual void OnConnect (void);
    virtual void OnDisconnect (void)          { LOG_PORT.print ("."); }

}; // fsm_Eth_state_ConnectingToEth

/*****************************************************************************/
class fsm_Eth_state_ConnectedToEth : public fsm_Eth_state
{
public:
    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Connected To Ethernet"); }
    virtual void OnConnect (void) {}
    virtual void OnDisconnect (void);

}; // fsm_Eth_state_ConnectedToEth

extern c_EthernetMgr EthernetMgr;

#endif // def SUPPORT_ETHERNET
