#pragma once
/*
* EthernetDriver.hpp - Output Management class
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
#ifdef SUPPORT_ETHERNET

// forward declaration
class c_EthernetDriver;

/*****************************************************************************/
class fsm_Eth_state
{
protected:
    c_EthernetDriver * pEthernetDriver = nullptr;
public:
    fsm_Eth_state() {}
    virtual ~fsm_Eth_state() {}

    virtual void Poll (void) = 0;
    virtual void Init (void) = 0;
    virtual void GetStateName (String& sName) = 0;
    virtual void OnConnect (void) = 0;
    virtual void OnGotIp (void) = 0;
    virtual void OnDisconnect (void) = 0;
    void SetParent (c_EthernetDriver* parent) { pEthernetDriver = parent; }
    void GetDriverName (String & value) { value = CN_EthDrv; }

}; // fsm_Eth_state

class c_EthernetDriver
{
public:
    c_EthernetDriver ();
    virtual ~c_EthernetDriver ();

    void            Begin               (); ///< set up the operating environment based on the current config (or defaults)
    void            GetConfig           (JsonObject & json);
    bool            SetConfig           (JsonObject & json);
    void            GetDriverName       (String & value) { value = CN_EthDrv; }

    int             ValidateConfig      ();
    IPAddress       GetIpAddress        ();
    IPAddress       GetIpSubNetMask     ();
    IPAddress       GetIpGateway        ();
    String          GetMacAddress       ();
    void            GetHostname         (String & Name);
    void            SetHostname         (String & Name) {}
    void            GetStatus           (JsonObject & jsonStatus);
    void            SetEthHostname      ();
    void            reset               ();
    void            Poll                ();
    bool            IsConnected         ();
    inline void     SetFsmState         (fsm_Eth_state * NewState) { pCurrentFsmState = NewState; }
    void            AnnounceState       ();
    FastTimer     & GetFsmTimer         () { return FsmTimer; }
    void            NetworkStateChanged (bool NetworkState);
    void            StartEth            ();
    void            InitPowerPin        ();
    inline uint32_t GetPowerPinActiveDelayMs () { return powerPinActiveDelayMs; }

private:

    void SetUpIp ();

    void onEventHandler         (const WiFiEvent_t event, const WiFiEventInfo_t info);

    FastTimer        NextPollTimer;
    uint32_t         PollInterval = 1000;
    bool             HasBeenPreviouslyConfigured = false;

    IPAddress        ip        = INADDR_NONE;
    IPAddress        netmask   = INADDR_NONE;
    IPAddress        gateway   = INADDR_NONE;
    IPAddress        primaryDns = INADDR_NONE;
    IPAddress        secondaryDns = INADDR_NONE;
    bool             UseDhcp   = true;
    uint32_t         phy_addr  = DEFAULT_ETH_ADDR;
    gpio_num_t       power_pin = DEFAULT_ETH_POWER_PIN;
    gpio_num_t       mdc_pin   = DEFAULT_ETH_MDC_PIN;
    gpio_num_t       mdio_pin  = DEFAULT_ETH_MDIO_PIN;
    eth_phy_type_t   phy_type  = DEFAULT_ETH_TYPE;
    eth_clock_mode_t clk_mode  = DEFAULT_ETH_CLK_MODE;
    bool             powerPinActiveValue = (HIGH == DEFAULT_ETH_POWER_PIN_ACTIVE);
    uint32_t         powerPinActiveDelayMs = 1000;

protected:
    friend class fsm_Eth_state_Boot;
    friend class fsm_Eth_state_ConnectingToEth;
    friend class fsm_Eth_state_WaitForIP;
    friend class fsm_Eth_state_GotIp;
    friend class fsm_Eth_state_DeviceInitFailed;
    friend class fsm_Eth_state;
    fsm_Eth_state * pCurrentFsmState = nullptr;
    FastTimer    FsmTimer;

}; // c_EthernetDriver

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
    fsm_Eth_state_Boot() {}
    virtual ~fsm_Eth_state_Boot() {}

    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Boot"); }
    virtual void OnConnect (void)             { /* ignore */ }
    virtual void OnGotIp (void)               { /* ignore */ }
    virtual void OnDisconnect (void)          { /* ignore */ }

}; // fsm_Eth_state_Boot

/*****************************************************************************/
class fsm_Eth_state_PoweringUp : public fsm_Eth_state
{
public:
    fsm_Eth_state_PoweringUp() {}
    virtual ~fsm_Eth_state_PoweringUp() {}

    virtual void Poll (void);
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Powering Up"); }
    virtual void OnConnect (void) {}
    virtual void OnGotIp (void) {}
    virtual void OnDisconnect (void) {}

}; // fsm_Eth_state_PoweringUp

/*****************************************************************************/
class fsm_Eth_state_ConnectingToEth : public fsm_Eth_state
{
public:
    fsm_Eth_state_ConnectingToEth() {}
    virtual ~fsm_Eth_state_ConnectingToEth() {}

    virtual void Poll(void);
    virtual void Init(void);
    virtual void GetStateName (String& sName) { sName = F ("Connecting"); }
    virtual void OnConnect (void);
    virtual void OnGotIp (void);
    virtual void OnDisconnect (void) {}

}; // fsm_Eth_state_ConnectingToEth

/*****************************************************************************/
class fsm_Eth_state_WaitForIP : public fsm_Eth_state
{
public:
    fsm_Eth_state_WaitForIP() {}
    virtual ~fsm_Eth_state_WaitForIP() {}

    virtual void Poll (void) {}
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Wait for IP Address"); }
    virtual void OnConnect (void) {}
    virtual void OnGotIp (void);
    virtual void OnDisconnect (void);

}; // fsm_Eth_state_WaitForIP

/*****************************************************************************/
class fsm_Eth_state_GotIp : public fsm_Eth_state
{
public:
    fsm_Eth_state_GotIp() {}
    virtual ~fsm_Eth_state_GotIp() {}

    virtual void Poll (void) {}
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Got IP"); }
    virtual void OnConnect (void) {}
    virtual void OnGotIp (void) {}
    virtual void OnDisconnect (void);

}; // fsm_Eth_state_GotIp

/*****************************************************************************/
class fsm_Eth_state_DeviceInitFailed : public fsm_Eth_state
{
public:
    fsm_Eth_state_DeviceInitFailed() {}
    virtual ~fsm_Eth_state_DeviceInitFailed() {}

    virtual void Poll (void) {}
    virtual void Init (void);
    virtual void GetStateName (String& sName) { sName = F ("Device Init Failed"); }
    virtual void OnConnect (void) {}
    virtual void OnGotIp (void) {}
    virtual void OnDisconnect (void) {}

}; // fsm_Eth_state_DeviceInitFailed

extern c_EthernetDriver EthernetDriver;

#endif // def SUPPORT_ETHERNET
