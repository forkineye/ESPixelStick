/******************************************************************
*  
*       Project: ESPixelStick - An ESP8266 and E1.31 based pixel (And Serial!) driver
*       Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*       Brought to you by:
*              Bill Porter
*              www.billporter.info
*
*       See Readme for other info and version history
*   
*  
*This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
<http://www.gnu.org/licenses/>
*
*This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License. 
*To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or
*send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
******************************************************************/


#ifndef SERIALDRIVER_H_
#define SERIALDRIVER_H_

#include "HardwareSerial.h"

/* UART for Renard / DMX output */
#define SEROUT_UART 1

#if SEROUT_UART == 0
#define SEROUT_PORT        Serial
#elif SEROUT_UART == 1
#define SEROUT_PORT        Serial1
#else
#error "Invalid SEROUT_UART specified"
#endif

/* DMX minimum timings per E1.11 */
#define DMX_BREAK 92
#define DMX_MAB 12

/* Serial Types */
enum class SerialType : uint8_t {
    DMX512,
    RENARD
};

enum class BaudRate : uint32_t {
    BR_38400 = 38400,
    BR_57600 = 57600,
    BR_115200 = 115200,
    BR_230400 = 230400,
    BR_250000 = 250000,
    BR_460800 = 460800
};

class SerialDriver {
 public:
    int begin(HardwareSerial *theSerial, SerialType type, uint16_t length);
    int begin(HardwareSerial *theSerial, SerialType type, uint16_t length,
            BaudRate baud);
    void startPacket();
    void show();
    uint8_t* getData();

    /* Set the value */
    inline void setValue(uint16_t address, uint8_t value) {
    // Avoid the special characters by rounding
        if (_type == SerialType::RENARD) {
            switch (value) {
                case 0x7d:
                    _serialdata[address + 2] = 0x7c;
                    break;
                case 0x7e:
                case 0x7f:
                    _serialdata[address + 2] = 0x80;
                    break;
                default:
                    _serialdata[address + 2] = value;
                    break;
            }
        } else if (_type == SerialType::DMX512) {
            _serialdata[address + 1] = value;
        }
    }

    /* Drop the update if our refresh rate is too high */
    inline bool canRefresh() {
        return (micros() - startTime) >= frameTime;
    }

 private:
    SerialType      _type;          // Output Serial type
    HardwareSerial  *_serial;       // The Serial Port
    uint16_t        _size;          // Size of buffer
    uint8_t         *_serialdata;   // Serial data buffer
    uint8_t         *_asyncdata;    // Async buffer
    uint32_t        frameTime;      // Time it takes for a frame TX to complete
    uint32_t        startTime;      // When the last frame TX started


    /* Fill the FIFO */
    static const uint8_t* ICACHE_RAM_ATTR fillFifo(const uint8_t *buff, const uint8_t *tail);

    /* Serial interrupt handler */
    static void ICACHE_RAM_ATTR serial_handle(void *param);

    /* Returns number of bytes waiting in the TX FIFO of SEROUT_UART */
    static inline uint8_t getFifoLength() {
        return (ESP8266_REG(U0F+(0xF00*SEROUT_UART)) >> USTXC) & 0xff;
    }

    /* Append a byte to the TX FIFO of SEROUT_UART */
    static inline void enqueue(uint8_t byte) {
        ESP8266_REG(U0F+(0xF00*SEROUT_UART)) = byte;
    }
};

#endif /* SERIALDRIVER_H_ */
