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
#include "RenardDriver.h"

int RenardDriver::begin(HardwareSerial *theSerial, uint16_t length) {
    return begin(theSerial, length, 57600);
}

int RenardDriver::begin(HardwareSerial *theSerial, uint16_t length, uint32_t baud) {
    int retval = true;

    _serial = theSerial;
    _size = length;
    _serial->begin(baud);

    return retval;
}

void RenardDriver::startPacket() {
    // create a buffer and fill in header for Renard
    _ptr = (uint8_t*) malloc(_size+2);
    _ptr[0] = 0x7E;
    _ptr[1] = 0x80;
}

void RenardDriver::setValue(uint16_t address, uint8_t value) {
    // avoid the special characters by rounding
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
}

void RenardDriver::show() {
    _serial->write(_ptr, _size+2);
    free(_ptr);
}
