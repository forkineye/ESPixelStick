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

#include <Arduino.h>
#include <ArduinoJson.h>
#include "SerialDriver.h"

int SerialDriver::begin(HardwareSerial *theSerial, SerialType type,
        uint16_t length) {
    return begin(theSerial, type, length, BaudRate::BR_57600);
}

int SerialDriver::begin(HardwareSerial *theSerial, SerialType type,
        uint16_t length, BaudRate baud) {
    int retval = true;

    _type = type;
    _serial = theSerial;
    _size = length;

    if (type == SerialType::RENARD)
        _serial->begin(static_cast<uint32_t>(baud));
    else if (type == SerialType::DMX512)
        _serial->begin(static_cast<uint32_t>(BaudRate::BR_250000));
    else
        retval = false;

    return retval;
}

void SerialDriver::startPacket() {
    // Create a buffer and fill in header
    if (_type == SerialType::RENARD) {
        _ptr = static_cast<uint8_t *>(malloc(_size + 2));
        _ptr[0] = 0x7E;
        _ptr[1] = 0x80;
    // Create buffer
    } else if (_type == SerialType::DMX512) {
        _ptr = static_cast<uint8_t *>(malloc(_size));
    }
}

void SerialDriver::setValue(uint16_t address, uint8_t value) {
    // Avoid the special characters by rounding
    if (_type == SerialType::RENARD) {
        switch (value) {
            case 0x7d:
                _ptr[address+2] = 0x7c;
                break;
            case 0x7e:
            case 0x7f:
                _ptr[address+2] = 0x80;
                break;
            default:
                _ptr[address+2] = value;
                break;
        }
    // Set value
    } else if (_type == SerialType::DMX512) {
        _ptr[address] = value;
    }
}

/*  
  Updated begins with serial 8n1/8n2 enabling functionality as a RS485 Chip
  replacement on Lynx Express, LOR CTB16PC, LOR CMB24D, etc.
  -Grayson Lough (Lights on Grassland)
*/
void SerialDriver::show() {
    if (_type == SerialType::RENARD) {
        _serial->write(_ptr, _size+2);
    } else if (_type == SerialType::DMX512) {
        // Send the break by sending a slow 0 byte
        _serial->begin(125000, SERIAL_8N1);
        _serial->write(0);
        _serial->flush();
        // Send the data
        _serial->begin(250000, SERIAL_8N2);
        _serial->write(0);
        _serial->write(_ptr, _size);
    }
    free(_ptr);
}
