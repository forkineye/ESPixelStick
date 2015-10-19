/******************************************************************
*  
*		Project: ESPixelStick - An ESP8266 and E1.31 based pixel (And Serial!) driver
*		Orginal ESPixelStickproject by 2015 Shelby Merrick
*
*		Brought to you by:
*              Bill Porter
*              www.billporter.info
*
*		See Readme for other info and version history
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
#include "ESerialDriver.h"

int ESerialDriver::begin(HardwareSerial *theSerial, serial_t type, uint16_t length){
	
		return begin(theSerial, type, length, 57600);
}
	

int ESerialDriver::begin(HardwareSerial *theSerial, serial_t type, uint16_t length, uint32_t baud){
	
	int retval = true;

	_type = type;
	_serial = theSerial;
	_size = length;

	if (type == SERIAL_RENARD)
		_serial->begin(baud);
	else if (type == SERIAL_DMX)
		_serial->begin(250000);
	else
		retval = false;

	return retval;
}
	
	
void ESerialDriver::startPacket(){
	
	//create a buffer and fill in header
	if (_type == SERIAL_RENARD){
		_ptr = (uint8_t*) malloc(_size+2);
		_ptr[0] = 0x7E;
		_ptr[1] = 0x80;
	}
	//create buffer	
	else if (_type == SERIAL_DMX)
		_ptr = (uint8_t*) malloc(_size);
	
}


void ESerialDriver::setValue(uint16_t address, uint8_t value){
	
	//avoid the special characters by rounding
	if (_type == SERIAL_RENARD){
		switch (value){
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
	//set value
	else if(_type == SERIAL_DMX)
		_ptr[address] = value;
		
	
}

void ESerialDriver::show(){
	
	if (_type == SERIAL_RENARD)
		_serial->write(_ptr, _size+2);
  else if(_type == SERIAL_DMX){
		// send the break by sending a slow 0 byte
		_serial->begin(125000);
		_serial->write(0);
		_serial->flush();
		_serial->begin(250000);
		_serial->write(0);
		_serial->write(_ptr, _size);
	}
		
	
	//_serial->flush();  //this may be needed later
	
	free(_ptr);
	
	
	
}