/*
* ART.h
*
* Project: ART - ART(ArtNet) library for Arduino
* Copyright (c) 2016 Rene Glitza
* 
* Based on E1.31 Library from Shelby Merrick
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

#ifndef _ART_h
#define _ART_h

#include "Arduino.h"
#include "util.h"

/* Network interface detection.  WiFi for ESP8266 and Ethernet for AVR */
#if defined (ARDUINO_ARCH_ESP8266) 
#   include <ESP8266WiFi.h>
#   include <ESP8266WiFiMulti.h>
#   include <WiFiUdp.h>
#  define _UDP WiFiUDP
# define INT_ESP8266
# define INT_WIFI
#elif defined (ARDUINO_ARCH_AVR)
# include <Ethernet.h>
# include <EthernetUdp.h>
# include <avr/pgmspace.h>
# define _UDP EthernetUDP
# define INT_ETHERNET
#   define NO_DOUBLE_BUFFER
#endif

/* Defaults */
#define ART_DEFAULT_PORT 6454
#define WIFI_CONNECT_TIMEOUT 10000  /* 10 seconds */

/* E1.31 Packet Offsets */
#define ART_ROOT_ID 0
#define ART_ROOT_ID_SIZE 8
#define ART_ROOT_OPCODE 8
#define ART_ROOT_OPCODE_SIZE 2
#define ART_ROOT_OPCODE_VALUE 0x500000
#define ART_ROOT_PROTOCOL 10
#define ART_ROOT_PROTOCOL_SIZE 2
#define ART_ROOT_PROTOCOL_VALUE 0xE0000
#define ART_ROOT_PACKET_SIZE 572

#define ART_FRAME_SEQ 12
#define ART_FRAME_PHY 13
#define ART_FRAME_UNIVERSE 14
#define ART_FRAME_FLENGTH 16

#define ART_DMP_DATA 18

/* ArtNet Packet Structure */
typedef union {
    struct {
        /* Root Layer */
        uint8_t  art_id[8];
        uint16_t opcode;
        uint16_t protocol;

        /* Frame Layer */
        uint8_t  sequence_number;
		    uint8_t  physical;
        uint8_t  universe;
		    uint16_t data_length;

        /* DMP Layer */
		    uint8_t  data_values[512];
    } __attribute__((packed));

    uint8_t raw[ART_ROOT_PACKET_SIZE]; //TODO Pakcet Size
} art_packet_t;

/* Status structure */
typedef struct {
    uint32_t    num_packets;
    uint32_t    sequence_errors;
    uint32_t    packet_errors;
} art_stats_t;

/* Error Types */
typedef enum {
    ERROR_NO,
    ERROR_ART_ID,
    ERROR_DATA_SIZE,
    ERROR_OPCODE,
    ERROR_PROTOCOL
} art_error_t;

/* E1.31 Listener Types */
typedef enum {
  UNICAST,
  MULTICAST
} listen_t;

class ART {
  
  
  private:
        /* Constants for packet validation */
        static const uint8_t ART_ID[];
        static const uint32_t PACKET_SIZE = ART_ROOT_PACKET_SIZE;
        static const uint32_t OPCODE = ART_ROOT_OPCODE_VALUE;
        static const uint8_t PROTOCOL = ART_ROOT_PROTOCOL_VALUE;
        
        art_packet_t pbuff1;   /* Packet buffer */
#ifndef NO_DOUBLE_BUFFER
        art_packet_t pbuff2;   /* Double buffer */
#endif
        art_packet_t *pwbuff;  /* Pointer to working packet buffer */
        uint8_t       sequence; /* Sequence tracker */
        _UDP udp;               /* UDP handle */

        /* Internal Initializers */
        int initWiFi(const char *ssid, const char *passphrase);
        int initEthernet(uint8_t *mac, IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns);
        void initUnicast();
        void initMulticast(uint16_t universe, uint16_t n);

  public:
        uint8_t       *data;                /* Pointer to DMX channel data */
        uint16_t      universe;             /* DMX Universe of last valid packet */
        art_packet_t *packet;              /* Pointer to last valid packet */
        art_stats_t  stats;                /* Statistics tracker */

        ART();

        /* Generic UDP listener, no physical or IP configuration */
      void begin(listen_t type, uint16_t universe = 1, uint16_t n = 1);

/****** START - Wireless ifdef block ******/
#if defined (INT_WIFI) || defined (INT_ESP8266)
        /* Unicast WiFi Initializers */
        int begin(const char *ssid, const char *passphrase);
        int begin(const char *ssid, const char *passphrase,
                IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns);
#endif
/****** END - Wireless ifdef block ******/

/****** START - ESP8266 ifdef block ******/
#if defined (INT_ESP8266)
        /* Multicast WiFi Initializers  -- ESP8266 Only */
        int beginMulticast(const char *ssid, const char *passphrase, uint16_t universe, uint16_t n);
        int beginMulticast(const char *ssid, const char *passphrase, uint16_t universe, uint16_t n,
                IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns);
#endif
/****** END - ESP8266 ifdef block ******/

/****** START - Ethernet ifdef block ******/
#if defined (INT_ETHERNET)    
        /* Unicast Ethernet Initializers */
        int begin(uint8_t *mac);
        void begin(uint8_t *mac,
                IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns);

        /* Multicast Ethernet Initializers */
        int beginMulticast(uint8_t *mac, uint16_t universe, uint16_t n);
        void beginMulticast(uint8_t *mac, uint16_t universe, uint16_t n,
                IPAddress ip, IPAddress netmask, IPAddress gateway, IPAddress dns);
#endif
/****** END - Ethernet ifdef block ******/

        /* Diag functions */
        void dumpError(art_error_t error);

        /* Main packet parser */
        inline uint16_t parsePacket() {
            art_error_t error;
            uint16_t retval = 0;

            int size = udp.parsePacket();
            if (size) {
                udp.readBytes(pwbuff->raw, size);
                error = validate();
                if (!error) {
#ifndef NO_DOUBLE_BUFFER
                    art_packet_t *swap = packet;
                    packet = pwbuff;
                    pwbuff = swap;
#endif
                    universe = packet->universe +1; //htons(packet->universe);
                    data = packet->data_values + 1;
                    retval = packet->data_length; // htons(packet->data_length) - 1;
                    if (packet->sequence_number != sequence++) {
                        stats.sequence_errors++;
                        sequence = packet->sequence_number + 1;
                    }
                    stats.num_packets++;
                } else {
                    if (Serial)
                        dumpError(error);
                    stats.packet_errors++;
                }
            }
            return retval;
        }

        /* Packet validater */
        inline art_error_t validate() {
#ifdef ARDUINO_ARCH_AVR
            if (memcmp_P(pwbuff->art_id, ART_ID, sizeof(pwbuff->art_id)))
#else
            if (memcmp(pwbuff->art_id, ART_ID, sizeof(pwbuff->art_id)))
#endif
                return ERROR_ART_ID;
            if (htonl(pwbuff->opcode) != ART_ROOT_OPCODE_VALUE)
                return ERROR_OPCODE;
            if (htonl(pwbuff->protocol) != ART_ROOT_PROTOCOL_VALUE)
                return ERROR_PROTOCOL;
            return ERROR_NO;
        }
};

#endif
