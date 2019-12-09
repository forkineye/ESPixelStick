/*
* FileIO.h - Static methods for common file I/O routines
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

#ifndef _FILEIO_H_
#define _FILEIO_H_

#include <Arduino.h>
#include <ArduinoJson.h>

/// Deserialization callback for I/O modules
typedef std::function<void(DynamicJsonDocument &json)> DeserializationHandler;

/// Contains static methods used for common File IO routines
class FileIO {
  public:
    /// Load configuration file
    /** Loads JSON configuration file via SPIFFS.
     *  Returns true on success.
     */
    static boolean loadConfig(const char *filename, DeserializationHandler dsHandler) {
        boolean retval = false;
        File file = SPIFFS.open(filename, "r");
        if (!file) {
            LOG_PORT.printf("Configuration file %s not found.\n", filename);
        } else {
            // Parse CONFIG_FILE json
            size_t size = file.size();
            if (size > CONFIG_MAX_SIZE) {
                LOG_PORT.printf("*** Configuration file %s too large ***\n", filename);
            }

            std::unique_ptr<char[]> buf(new char[size]);
            file.readBytes(buf.get(), size);

            DynamicJsonDocument json(1024);
            DeserializationError error = deserializeJson(json, buf.get());
            if (error) {
                LOG_PORT.printf("*** JSON Deserialzation Error: %s ***\n", filename);
            }

            dsHandler(json);

            LOG_PORT.printf("Configuration file %s loaded.\n", filename);
            retval = true;
        }
        return retval;
    }

    /// Save configuration file
    /** Saves configuration file via SPIFFS
     * Returns true on success.
     */
    static boolean saveConfig(const char *filename, String jsonString) {
        boolean retval = false;
        File file = SPIFFS.open(filename, "w");
        if (!file) {
            LOG_PORT.printf("*** Error saving %s ***\n", filename);
        } else {
            file.println(jsonString);
            LOG_PORT.printf("Configuration file %s saved.\n", filename);
            retval = true;
        }
        return retval;
    }

    /// Checks if value is empty and sets key to value if they're different. Returns true if key was set
    static boolean setFromJSON(String &key, JsonVariant val) {
        if (!val.isNull() && !val.as<String>().equals(key)) {
    //LOG_PORT.printf("**** Setting '%s' ****\n", val.c_str());
            key = val.as<String>();
            return true;
        } else {
            return false;
        }
    }

    static boolean setFromJSON(boolean &key, JsonVariant val) {
        if (!val.isNull() && (val.as<boolean>() != key)) {
            key = val;
            return true;
        } else {
            return false;
        }
    }

    static boolean setFromJSON(uint8_t &key, JsonVariant val) {
        if (!val.isNull() && (val.as<uint8_t>() != key)) {
            key = val;
            return true;
        } else {
            return false;
        }
    }

    static boolean setFromJSON(uint16_t &key, JsonVariant val) {
        if (!val.isNull() && (val.as<uint16_t>() != key)) {
            key = val;
            return true;
        } else {
            return false;
        }
    }

    static boolean setFromJSON(uint32_t &key, JsonVariant val) {
        if (!val.isNull() && (val.as<uint32_t>() != key)) {
            key = val;
            return true;
        } else {
            return false;
        }
    }

    static boolean setFromJSON(float &key, JsonVariant val) {
        if (!val.isNull() && (val.as<float>() != key)) {
            key = val;
            return true;
        } else {
            return false;
        }
    }
};

#endif /* _FILEIO_H_ */