/*
* _Input.h - Abstract input class
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2019 Shelby Merrick
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

#ifndef _INPUT_H_
#define _INPUT_H_

#include <Arduino.h>
#include "../ESPixelStick.h"
#include "../FileIO.h"

class _Input {
  public:
    virtual ~_Input() {}
    virtual void destroy() = 0;     ///< Call to destroy and shutdown input

    virtual String getKey() = 0;    ///< Returns KEY of module
    virtual String getBrief() = 0;  ///< Returns brief description of module

    virtual void validate() = 0;    ///< Validates configuration
    virtual void load() = 0;        ///< Loads configuration
    virtual void save() = 0;        ///< Saves configuration

    virtual void init() = 0;        ///< Call from setup(), initializes and starts input
    virtual void process() = 0;     ///< Call from loop(), process incoming data

    void setBuffer(uint8_t *buff, uint16_t size) {
        showBuffer = buff;
        showBufferSize = size;
    }

  protected:
    uint8_t *showBuffer;        ///< Show data buffer
    uint16_t showBufferSize;    ///< Size of show data buffer

    virtual void deserialize(DynamicJsonDocument &json) = 0;
    virtual String serialize() = 0;
};

#endif /* _INPUT_H_ */