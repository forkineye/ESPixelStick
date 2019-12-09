/*
* _Output.h - Abstract output class
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

#ifndef _OUTPUT_H_
#define _OUTPUT_H_

#include <Arduino.h>
#include <ArduinoJson.h>

class _Output {
  public:
    virtual ~_Output() {}
    virtual void destroy() = 0;           ///< Call to destroy and shutdown input

    virtual const char* getKey() = 0;     ///< Returns KEY of module
    virtual const char* getBrief() = 0;   ///< Returns brief description of module

    virtual uint8_t getTupleSize() = 0;   ///< Returns the tuple size this output uses
    virtual uint16_t getTupleCount() = 0; ///< Returns the number of tuples configured for this output

    virtual void validate() = 0;          ///< Validates configuration
    virtual void load() = 0;              ///< Loads configuration
    virtual void save() = 0;              ///< Saves configuration

    virtual void init() = 0;              ///< Call from setup(), initializes and starts input
    virtual void render() = 0;            ///< Call from loop(), renders output data

    virtual boolean deserialize(DynamicJsonDocument &json) = 0; ///< Sets configuration from JSON
    virtual String serialize(boolean pretty = false) = 0;       ///< Gets configuration as JSON

    void setBuffer(uint8_t *buff) {
        showBuffer = buff;
    }

  protected:
    uint8_t *showBuffer;    ///< Show data buffer
};

#endif /* _OUTPUT_H_ */