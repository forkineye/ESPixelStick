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
    static boolean loadConfig(String filename, DeserializationHandler dsHandler) {
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
                LOG_PORT.printf("*** JSON Deserialzation Error: %s ***\n", filename.c_str());
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
    static boolean saveConfig(String filename, String jsonString) {
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

#endif /* _FILEIO_H_ */