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

typedef std::function<void(DynamicJsonDocument &json)> DeserializationHandler;

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

    /// Load configuration file
    /** Loads JSON configuration file via SPIFFS.
     *  Returns true on success.
     */
    boolean load(String filename, DeserializationHandler dsHandler) {
        boolean retval = false;
        File file = SPIFFS.open(filename, "r");
        if (!file) {
            LOG_PORT.printf("Configuration file %s not found.\n", filename.c_str());
        } else {
            // Parse CONFIG_FILE json
            size_t size = file.size();
            if (size > CONFIG_MAX_SIZE) {
                LOG_PORT.printf("*** Configuration file %s too large ***\n", filename.c_str());
            }

            std::unique_ptr<char[]> buf(new char[size]);
            file.readBytes(buf.get(), size);

            DynamicJsonDocument json(1024);
            DeserializationError error = deserializeJson(json, buf.get());
            if (error) {
                LOG_PORT.printf("*** JSON Deserialzation Error: %s\n***", filename.c_str());
            }

            dsHandler(json);

            LOG_PORT.printf("Configuration file %s loaded.\n", filename.c_str());
            retval = true;
        }
        return retval;
    }

    /// Save configuration file
    /** Saves configuration file via SPIFFS
     * Returns true on success.
     */
    boolean save(String filename, String jsonString) {
        boolean retval = false;
        File file = SPIFFS.open(filename, "w");
        if (!file) {
            LOG_PORT.printf("*** Error saving %s ***\n", filename.c_str());
        } else {
            file.println(jsonString);
            LOG_PORT.printf("Configuration file %s saved.\n", filename.c_str());
            retval = true;
        }
        return retval;
    }
};

#endif /* _INPUT_H_ */