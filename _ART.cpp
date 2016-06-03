/*
* ART.cpp
*
* Project: ART - E.131 (sART) library for Arduino
* Copyright (c) 2015 Shelby Merrick
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

#include "_ART.h"
#include <string.h>

#ifdef INT_ESP8266
#include "lwip/ip_addr.h"
#include "lwip/igmp.h"
#endif

/* E1.17 ART Packet Identifier */
#ifdef ARDUINO_ARCH_AVR
const PROGMEM byte ART::ART_ID[8] = { 0x41, 0x72, 0x74, 0x2d, 0x4e, 0x65, 0x74, 0x00};
#else
const byte ART::ART_ID[8] = { 0x41, 0x72, 0x74, 0x2d, 0x4e, 0x65, 0x74, 0x00};
#endif

/* Constructor */
ART::ART() {
#ifdef NO_DOUBLE_BUFFER
    memset(pbuff1.raw, 0, sizeof(pbuff1.raw));
    packet = &pbuff1;
    pwbuff = packet;
#else
    memset(pbuff1.raw, 0, sizeof(pbuff1.raw));
    memset(pbuff2.raw, 0, sizeof(pbuff2.raw));
    packet = &pbuff1;
    pwbuff = &pbuff2;
#endif
    
    sequence = 0;
    stats.num_packets = 0;
    stats.sequence_errors = 0;
    stats.packet_errors = 0;
}

void ART::initUnicast() {
    delay(100);
    udp.begin(ART_DEFAULT_PORT);
    if (Serial) {
        Serial.print(F("- Unicast port: "));
        Serial.println(ART_DEFAULT_PORT);
    }
}

/* Senseless, doese not work */
void ART::initMulticast(uint16_t universe, uint16_t n) {
    delay(100);
    IPAddress localIP = WiFi.localIP();
	  //uint8_t localIPByte [4] = {};
	  //localIPByte = &((byte) localIP);
    IPAddress subnet = WiFi.subnetMask();
    Serial.print("Subnet: ");
    Serial.println(subnet);
    IPAddress address = IPAddress(((uint8_t)subnet[0] == (uint8_t)255) ? (uint8_t)localIP[0] : 255, ((uint8_t)subnet[1] == 255) ? (uint8_t)localIP[1] : 255, ((uint8_t)subnet[2] == 255) ? (uint8_t)localIP[2] : 255, ((uint8_t)subnet[3] == 255) ? (uint8_t)localIP[3] : 255);
    Serial.print("Bradcast Address: ");
    Serial.println(address);

#ifdef INT_ESP8266
    ip_addr_t ifaddr;
    ip_addr_t multicast_addr;
    ifaddr.addr = (uint32_t) localIP;
/*
    for (uint16_t i = 1; i < n; i++) {
        multicast_addr.addr = (uint32_t) IPAddress((uint8_t)localIP[0], (uint8_t)localIP[1], (uint8_t)localIP[2], 255);
        igmp_joingroup(&ifaddr, &multicast_addr);
    }
*/
    multicast_addr.addr = (uint32_t) IPAddress((uint8_t)localIP[0], (uint8_t)localIP[1], (uint8_t)localIP[2], 255);
    igmp_joingroup(&ifaddr, &multicast_addr);
    
    udp.beginMulticast(localIP, address, ART_DEFAULT_PORT);
#endif

    if (Serial) {
        Serial.print(F("- Universe from: "));
        Serial.println(universe);
        Serial.print(F("- Universes total: "));
        Serial.println(n);
        Serial.print(F("- First multicast address: "));
        Serial.println(address);
    }
}

/****** START - Wireless ifdef block ******/
#if defined (INT_ESP8266) || defined (INT_WIFI)

/* WiFi Initialization */
int ART::initWiFi(const char *ssid, const char *passphrase) {
    /* Switch to station mode and disconnect just in case */
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (Serial) {
        Serial.println("");
        Serial.print(F("Connecting to "));
        Serial.print(ssid);
    }
  
    if (passphrase != NULL)
        WiFi.begin(ssid, passphrase);
    else
        WiFi.begin(ssid);

    uint32_t timeout = millis();
    uint8_t retval = 1;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (Serial)
            Serial.print(".");
        if (millis() - timeout > WIFI_CONNECT_TIMEOUT) {
            retval = 0;
            if (Serial) {
                Serial.println("");
                Serial.println(F("*** Failed to connect ***"));
            }
            break;
        }
    }

    return retval;
}

void ART::begin(listen_t type, uint16_t universe, uint16_t n) {
  if (type == UNICAST)
    initUnicast();
  if (type == MULTICAST)
    initMulticast(universe, n);
}

int ART::begin(const char *ssid, const char *passphrase) {
    if (initWiFi(ssid, passphrase)) {
        if (Serial) {
            Serial.println("");
            Serial.print(F("Connected DHCP with IP: "));
            Serial.println(WiFi.localIP());
        }
        initUnicast();
    }
    return WiFi.status();
}

int ART::begin(const char *ssid, const char *passphrase, 
                IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns) {
    if (initWiFi(ssid, passphrase)) {
        WiFi.config(ip, gateway, netmask, dns);
        if (Serial) {
            Serial.println("");
            Serial.print(F("Connected with Static IP: "));
            Serial.println(WiFi.localIP());
        }    
        initUnicast();
    }
    return WiFi.status();   
}

#endif  
/****** END - Wireless ifdef block ******/

/****** START - ESP8266 ifdef block ******/
#if defined (INT_ESP8266)  
int ART::beginMulticast(const char *ssid, const char *passphrase, uint16_t universe, uint16_t n) {
    if (initWiFi(ssid, passphrase)) {
        if (Serial) {
            Serial.println("");
            Serial.print(F("Connected DHCP with IP: "));
            Serial.println(WiFi.localIP());
        }    
        initMulticast(universe, n);
    }
    return WiFi.status();
}

int ART::beginMulticast(const char *ssid, const char *passphrase, uint16_t universe, uint16_t n,
        IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns) {
    if (initWiFi(ssid, passphrase)) {
        WiFi.config(ip, dns, gateway, netmask);
        if (Serial) {
            Serial.println("");
            Serial.print(F("Connected with Static IP: "));
            Serial.println(WiFi.localIP());
        }        
        initMulticast(universe, n);
    }
    return WiFi.status();   
}
#endif
/****** END - ESP8266 ifdef block ******/

/****** START - Ethernet ifdef block ******/
#if defined (INT_ETHERNET)  

/* Unicast Ethernet Initializers */
int ART::begin(uint8_t *mac) {
    int retval = 0;

    if (Serial) {
        Serial.println("");
        Serial.println(F("Requesting Address via DHCP"));
        Serial.print(F("- MAC: "));
        for (int i = 0; i < 6; i++)
            Serial.print(mac[i], HEX);
        Serial.println("");
    }

    retval = Ethernet.begin(mac);

    if (Serial) {
        if (retval) {
            Serial.print(F("- IP Address: "));
            Serial.println(Ethernet.localIP());
        } else {
            Serial.println(F("** DHCP FAILED"));
        }
    }

    if (retval)
        initUnicast();

    return retval;
}

void ART::begin(uint8_t *mac, 
        IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns) {
    Ethernet.begin(mac, ip, dns, gateway, netmask);
    if (Serial) {
        Serial.println("");
        Serial.println(F("Static Configuration"));
        Serial.println(F("- MAC: "));
        for (int i = 0; i < 6; i++)
            Serial.print(mac[i], HEX);
        Serial.print(F("- IP Address: "));
        Serial.println(Ethernet.localIP());
    }

    initUnicast();
}

/* Multicast Ethernet Initializers */
int ART::beginMulticast(uint8_t *mac, uint16_t universe, uint16_t n) {
    //TODO: Add ethernet multicast support
}

void ART::beginMulticast(uint8_t *mac, uint16_t universe, uint16_t n,
        IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns) {
    //TODO: Add ethernet multicast support
}
#endif
/****** END - Ethernet ifdef block ******/

void ART::dumpError(art_error_t error) {
    switch (error) {
        case ERROR_ART_ID:
            Serial.print(F("INVALID PACKET ID: "));
            for (int i = 0; i < sizeof(pwbuff->art_id); i++)
                Serial.print(pwbuff->art_id[i], HEX);
            Serial.println("");
            Serial.print(F("Should be: "));
            for (int i = 0; i < sizeof(ART_ID); i++)
                Serial.print(ART_ID[i], HEX);
            Serial.println("");
            break;
        case ERROR_DATA_SIZE:
            Serial.println(F("INVALID PACKET SIZE: "));
            break;
        case ERROR_OPCODE:
            Serial.print(F("INVALID OPCODE: 0x"));
            Serial.println(htonl(pwbuff->opcode), HEX);
            break;
        case ERROR_PROTOCOL:
            Serial.print(F("INVALID PROTOCOL: 0x"));
            Serial.println(htonl(pwbuff->protocol), HEX);

    }
}
