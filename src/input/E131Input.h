/*
* E131Input.h - Code to wrap ESPAsyncE131 for input
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

#ifndef E131INPUT_H_
#define E131INPUT_H_

#include <ESPAsyncE131.h>
#include "_Input.h"

class E131Input : public _Input {
  private:
    static const uint16_t   UNIVERSE_MAX = 512;
    static const String     CONFIG_FILE;

    ESPAsyncE131  *e131;        ///< ESPAsyncE131 pointer

    /// JSON configuration parameters
    uint16_t    universe;       ///< Universe to listen for
    uint16_t    universe_limit; ///< Universe boundary limit
    uint16_t    channel_start;  ///< Channel to start listening at - 1 based
    boolean     multicast;      ///< Enable multicast listener

    /// from sketch globals
    uint16_t    channel_count;  ///< Number of channels. Derived from output module configuration.
    uint16_t    uniLast = 1;    ///< Last Universe to listen for
    uint8_t     *seqTracker;    ///< Current sequence numbers for each Universe
    uint32_t    *seqError;      ///< Sequence error tracking for each universe

    void deserialize(DynamicJsonDocument &json);
    String serialize();

    void multiSub();

  public:
    static const String KEY;

    ~E131Input();
    void destroy();

    String getKey();
    String getBrief();

    void validate();
    void load();
    void save();

    void init();
    void process();
};

#endif /* E131INPUT_H_ */