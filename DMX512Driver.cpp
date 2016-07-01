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
#include "DMX512Driver.h"

int DMX512Driver::begin(HardwareSerial *theSerial, uint16_t length) {
    int retval = true;

    _serial = theSerial;
    _size = length;
    _serial->begin(250000);

    return retval;
}

void DMX512Driver::startPacket() {
    // create a buffer for DMX
    _ptr = (uint8_t*) malloc(_size);
}

void DMX512Driver::setValue(uint16_t address, uint8_t value) {
    _ptr[address] = value;
}

/* 
   Updated begins with serial 8n1/8n2 enabling functionality as a 
   RS485 Chip replacement on Lynx Express, LOR CTB16PC, LOR CMB24D, etc.
   -Grayson Lough (Lights on Grassland)
*/
void DMX512Driver::show() {
    // send the break by sending a slow 0 byte
    _serial->begin(125000, SERIAL_8N1);
    _serial->write(0);
    _serial->flush();

    // send the data
    _serial->begin(250000, SERIAL_8N2);
    _serial->write(0);
    _serial->write(_ptr, _size);

    free(_ptr);
}
