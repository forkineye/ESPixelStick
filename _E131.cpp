/*
* E131.cpp
*
* Project: E131 - E.131 (sACN) library for Arduino
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

#include "_E131.h"
#include <string.h>

/* E1.17 ACN Packet Identifier */
#ifdef ARDUINO_ARCH_AVR
const PROGMEM byte E131::ACN_ID[12] = { 0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00 };
#else
const byte E131::ACN_ID[12] = { 0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00 };
#endif

/* Constructor */
E131::E131() {
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

void E131::initUnicast() {
    delay(100);
    udp.begin(E131_DEFAULT_PORT);
    if (Serial) {
        Serial.print(F("- Unicast port: "));
        Serial.println(E131_DEFAULT_PORT);
    }
}

void E131::initMulticast(uint16_t universe) {
    delay(100);
    IPAddress address = IPAddress(239, 255, ((universe >> 8) & 0xff), ((universe >> 0) & 0xff));
#ifdef INT_ESP8266
    udp.beginMulticast(WiFi.localIP(), address, E131_DEFAULT_PORT);
#endif
    if (Serial) {
        Serial.print(F("- Universe: "));
        Serial.println(universe);
        Serial.print(F("- Multicast address: "));
        Serial.println(address);
    }
}

/****** START - Wireless ifdef block ******/
#if defined (INT_ESP8266) || defined (INT_WIFI)

/* WiFi Initialization */
int E131::initWiFi(const char *ssid, const char *passphrase) {
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

void E131::begin(e131_listen_t type, uint16_t universe) {
	if (type == E131_UNICAST)
		initUnicast();
	if (type == E131_MULTICAST)
		initMulticast(universe);
}

int E131::begin(const char *ssid, const char *passphrase) {
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

int E131::begin(const char *ssid, const char *passphrase, 
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
int E131::beginMulticast(const char *ssid, const char *passphrase, uint16_t universe) {
    if (initWiFi(ssid, passphrase)) {
        if (Serial) {
            Serial.println("");
            Serial.print(F("Connected DHCP with IP: "));
            Serial.println(WiFi.localIP());
        }    
        initMulticast(universe);
    }
    return WiFi.status();
}

int E131::beginMulticast(const char *ssid, const char *passphrase, uint16_t universe, 
        IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns) {
    if (initWiFi(ssid, passphrase)) {
        WiFi.config(ip, dns, gateway, netmask);
        if (Serial) {
            Serial.println("");
            Serial.print(F("Connected with Static IP: "));
            Serial.println(WiFi.localIP());
        }        
        initMulticast(universe);
    }
    return WiFi.status();   
}
#endif
/****** END - ESP8266 ifdef block ******/

/****** START - Ethernet ifdef block ******/
#if defined (INT_ETHERNET)  

/* Unicast Ethernet Initializers */
int E131::begin(uint8_t *mac) {
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

void E131::begin(uint8_t *mac, 
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
int E131::beginMulticast(uint8_t *mac, uint16_t universe) {
    //TODO: Add ethernet multicast support
}

void E131::beginMulticast(uint8_t *mac, uint16_t universe,
        IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns) {
    //TODO: Add ethernet multicast support
}
#endif
/****** END - Ethernet ifdef block ******/

void E131::dumpError(e131_error_t error) {
    switch (error) {
        case ERROR_ACN_ID:
            Serial.print(F("INVALID PACKET ID: "));
            for (int i = 0; i < sizeof(ACN_ID); i++)
                Serial.print(pwbuff->acn_id[i], HEX);
            Serial.println("");
            break;
        case ERROR_PACKET_SIZE:
            Serial.println(F("INVALID PACKET SIZE: "));
            break;
        case ERROR_VECTOR_ROOT:
            Serial.print(F("INVALID ROOT VECTOR: 0x"));
            Serial.println(htonl(pwbuff->root_vector), HEX);
            break;
        case ERROR_VECTOR_FRAME:
            Serial.print(F("INVALID FRAME VECTOR: 0x"));
            Serial.println(htonl(pwbuff->frame_vector), HEX);
            break;
        case ERROR_VECTOR_DMP:
            Serial.print(F("INVALID DMP VECTOR: 0x"));
            Serial.println(pwbuff->dmp_vector, HEX);
    }
}
