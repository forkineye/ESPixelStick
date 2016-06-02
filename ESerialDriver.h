/******************************************************************
*  
*    Project: ESPixelStick - An ESP8266 and E1.31 based pixel (And Serial!) driver
*   Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*   Brought to you by:
*              Bill Porter
*              www.billporter.info
*
*   See Readme for other info and version history
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




#ifndef ESERIALDRIVER_H
#define ESERIALDRIVER_H

#include "HardwareSerial.h"


/* Serial Types */
typedef enum {
    SERIAL_RENARD,
    SERIAL_DMX
} serial_t;

class ESerialDriver {
    public:
    int begin(HardwareSerial *theSerial, serial_t, uint16_t);
    int begin(HardwareSerial *theSerial, serial_t, uint16_t, uint32_t);
    void startPacket();
    void setValue(uint16_t, uint8_t);
    void show();

    private:
    serial_t _type;            //Output Serial type
    HardwareSerial *_serial;  //The Serial Port
    uint16_t _size;           //Size of buffer    
    uint8_t * _ptr;           //pointer for buffer
};

#endif
