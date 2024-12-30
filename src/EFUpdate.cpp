/*
* EFUpdate.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2016, 2025 Shelby Merrick
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

#include "ESPixelStick.h"
#include <FS.h>
#include <LittleFS.h>

#include <lwip/def.h>
#include "EFUpdate.h"

#ifdef ARDUINO_ARCH_ESP32
#   include <Update.h>
#   include <esp_task_wdt.h>
// #   include <esp_app_format.h>
#endif

#ifndef U_SPIFFS
/*
 * Arduino 8266 libraries removed U_SPIFFS on master, replacing it with U_FS to allow for other FS types -
 * See https://github.com/esp8266/Arduino/commit/a389a995fb12459819e33970ec80695f1eaecc58#diff-6c6d762c616bd0b92156f152d128ad51
 *
 * Substitute the value here, while not breaking things for people using older SDKs.
 */
#	define U_SPIFFS U_FS
#endif


void EFUpdate::begin() {
    // DEBUG_START;
    _maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    _state = State::HEADER;
    _loc = 0;
    _error = EFUPDATE_ERROR_OK;
    _errorMsg = "";
    Update.onProgress(
        [this] (size_t progress, size_t total)
        {
            LOG_PORT.println(String("\033[Fprogress: ") + String(progress));
        }
    );
    // DEBUG_END;
}

bool EFUpdate::process(uint8_t *data, uint32_t len) {
    // DEBUG_START;
    uint32_t index = 0;
    bool ConfigChanged = true;

    while (!hasError() && (index < len))
    {
        // DEBUG_V (String ("  len: 0x") + String (len, HEX));
        // DEBUG_V (String ("index: 0X") + String (index, HEX));

        switch (_state)
        {
            case State::HEADER:
            {
                // DEBUG_V (String ("  len: 0x") + String (len, HEX));
                // DEBUG_V (String ("index: ") + String (index));
                // DEBUG_V ("Process HEADER record");
                _header.raw[_loc++] = data[index++];
                // DEBUG_V ();
                if (_loc == sizeof(efuheader_t)) {
                    // DEBUG_V (String("signature: 0x") + String(_header.signature, HEX));
                    if (_header.signature == EFU_ID) {
                        // DEBUG_V ();
                        _header.version = ntohs(_header.version);
                        // DEBUG_V (String("version: ") + String(_header.version));
                        memset(&_record, 0, sizeof(efurecord_t));
                        _loc = 0;
                        _state = State::RECORD;
                    } else {
                        logcon (F("FAIL: EFUPDATE_ERROR_SIG"));
                        _state = State::FAIL;
                        _error = EFUPDATE_ERROR_SIG;
                        _errorMsg = F("Invalid EFU Signature");
                    }
                }
                // DEBUG_V ();
                break;
            }
            case State::RECORD:
            {
                // DEBUG_V ("Process Data RECORD Type");
                // DEBUG_V (String ("              len: 0x") + String (len, HEX));
                // DEBUG_V (String ("            index: ") + String (index));
                // DEBUG_V (String ("             Data: 0x") + String (data[index], HEX));
                // DEBUG_V (String ("             Data: ") + String (data[index]));

                _record.raw[_loc++] = data[index++];
                if (_loc == sizeof(efurecord_t)) {
                    // DEBUG_V ();
                    _record.type = RecordType(ntohs((uint16_t)_record.type));
                    _record.size = ntohl(_record.size);
                    _loc = 0;
                    // DEBUG_V (String("_record.type: ") + uint32_t(_record.type));
                    // DEBUG_V (String("_record.type: 0x") + String(uint32_t(_record.type), HEX));
                    // DEBUG_V (String("_record.size: ") + _record.size);
                    // DEBUG_V (String("_record.size: 0x") + String(_record.size, HEX));
                    if (_record.type == RecordType::SKETCH_IMAGE) {
                        logcon ("Starting Sketch Image Update\n");
                        // Begin sketch update
                        if (!Update.begin(_record.size, U_FLASH)) {
                            logcon (F("Update.begin FAIL"));
                            _state = State::FAIL;
                            _error = Update.getError();
                            ConvertErrorToString();
                        } else {
                            /// DEBUG_V ("PASS");
                            _state = State::DATA;
                            // esp_efu_header_t *esp_image_header = (efuheader_t*)&data[index];
                            // DEBUG_V(String("            magic: 0x") + String(esp_image_header->magic, HEX));
                            // DEBUG_V(String("    segment_count: 0x") + String(esp_image_header->segment_count, HEX));
                            // DEBUG_V(String("         spi_mode: 0x") + String(esp_image_header->spi_mode, HEX));
                            // DEBUG_V(String("        spi_speed: 0x") + String(esp_image_header->spi_speed, HEX));
                            // DEBUG_V(String("         spi_size: 0x") + String(esp_image_header->spi_size, HEX));
                            // DEBUG_V(String("       entry_addr: 0x") + String(esp_image_header->entry_addr, HEX));
                            // DEBUG_V(String("           wp_pin: 0x") + String(esp_image_header->wp_pin, HEX));
                            // DEBUG_V(String("      spi_pin_drv: 0x") + String(esp_image_header->spi_pin_drv, HEX));
                            // DEBUG_V(String("          chip_id: 0x") + String(esp_image_header->chip_id, HEX));
                            // DEBUG_V(String("     min_chip_rev: 0x") + String(esp_image_header->min_chip_rev, HEX));
                            // DEBUG_V(String("min_chip_rev_full: 0x") + String(esp_image_header->min_chip_rev_full, HEX));
                            // DEBUG_V(String("max_chip_rev_full: 0x") + String(esp_image_header->max_chip_rev_full, HEX));
                            // esp_image_header->min_chip_rev_full = 0;
                        }
#ifdef ARDUINO_ARCH_ESP8266
                        Update.runAsync (true);
#endif
                        // DEBUG_V ();
                    } else if (_record.type == RecordType::FS_IMAGE) {
                        logcon ("Starting update of FS IMAGE\n");
                        // DEBUG_V("Begin file system update");
#ifdef ARDUINO_ARCH_ESP8266
                        LittleFS.end();
#endif
                        // DEBUG_V ();
                        if (!Update.begin(_record.size, U_SPIFFS)) {
                            logcon (F("begin U_SPIFFS failed"));
                            _state = State::FAIL;
                            _error = Update.getError();
                            ConvertErrorToString();
                            // DEBUG_V ();
                        } else {
                            // DEBUG_V ("begin U_SPIFFS");
                            _state = State::DATA;
                        }
#ifdef ARDUINO_ARCH_ESP8266
                        Update.runAsync (true);
#endif
                    } else {
                        logcon (F("Unknown Record Type"));
                        _state = State::FAIL;
                        _error = EFUPDATE_ERROR_REC;
                        _errorMsg = F("Unknown Record Type");
                    }
                }
                // DEBUG_V ();
                break;
            }
            case State::DATA:
            {
                // DEBUG_V ("DATA");
                uint32_t toWrite;

                toWrite = (_record.size - _loc < len) ? _record.size - _loc : len - index;
                // DEBUG_V ("Call Update.write");
                // DEBUG_V (String ("toWrite: 0x") + String (toWrite, HEX));
                // DEBUG_V (String ("   data: 0x") + String (uint32_t(data) + index, HEX));
                FeedWDT();

                Update.write(&data[index], toWrite);
                // DEBUG_V ("write done");
                index = index + toWrite;
                _loc = _loc + toWrite;

                if (_record.size == _loc) {
                    // DEBUG_V ("Call Update.end");
                    Update.end(true);
                    logcon ("Data Transfer Complete");
                    memset(&_record, 0, sizeof(efurecord_t));
                    _loc = 0;
                    _state = State::RECORD;
                }
                // DEBUG_V ();
                break;
            }
            case State::FAIL:
            {
                // DEBUG_V ("Enter FAIL state");
                index = len;
                ConfigChanged = false;
                break;
            }
            case State::IDLE:
            {
                // dont do anything
                break;
            }
        }
    }
    // DEBUG_END;

    return ConfigChanged;
}

bool EFUpdate::hasError() {
    // DEBUG_V("Test For error");
    return _error != EFUPDATE_ERROR_OK;
}

uint8_t EFUpdate::getError(String & msg)
{
    // DEBUG_V ();
    msg = _errorMsg;
    return _error;
}

void EFUpdate::ConvertErrorToString()
{
    switch (_error)
    {
        case UPDATE_ERROR_OK:
        {
            _errorMsg = F("OK");
            break;
        }
        case UPDATE_ERROR_WRITE:
        {
            _errorMsg = F("Error writting to Flash");
            break;
        }
        case UPDATE_ERROR_ERASE:
        {
            _errorMsg = F("Error Erasing Flash");
            break;
        }
        case UPDATE_ERROR_READ:
        {
            _errorMsg = F("Could not read from FLASH");
            break;
        }
        case UPDATE_ERROR_SPACE:
        {
            _errorMsg = F("Not enough space in partition");
            break;
        }
        case UPDATE_ERROR_SIZE:
        {
            _errorMsg = F("File Size mismatch");
            break;
        }
        case UPDATE_ERROR_STREAM:
        {
            _errorMsg = F("Stream writer failed");
            break;
        }
        case UPDATE_ERROR_MD5:
        {
            _errorMsg = F("MD5 checksum failed");
            break;
        }
        case UPDATE_ERROR_MAGIC_BYTE:
        {
            _errorMsg = F("Magic Byte Mismatch");
            break;
        }
#ifdef ARDUINO_ARCH_ESP32
        case UPDATE_ERROR_ACTIVATE:
        {
            _errorMsg = F("Could Not activate the alternate partition");
            break;
        }
        case UPDATE_ERROR_NO_PARTITION:
        {
            _errorMsg = F("No partition defined for target");
            break;
        }
        case UPDATE_ERROR_BAD_ARGUMENT:
        {
            _errorMsg = F("Invalid argument");
            break;
        }
        case UPDATE_ERROR_ABORT:
        {
            _errorMsg = F("Operation Aborted");
            break;
        }
#endif // def ARDUINO_ARCH_ESP32
        default:
        {
            _errorMsg = F("Unknown Error Code");
            break;
        }
    }
} // ConvertErrorToString

bool EFUpdate::end() {
    // DEBUG_V ();
    if (_state == State::FAIL)
        return false;
    else
        return true;
}
