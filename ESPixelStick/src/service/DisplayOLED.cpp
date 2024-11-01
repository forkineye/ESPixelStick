/*
 * DisplayOLED.cpp
 *
 * Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
 * Copyright (c) 2018, 2024 Shelby Merrick
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

#include "../ESPixelStick.h"
#ifdef USE_OLED

// OLED - Standard 128x32 i2c
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DisplayOLED.h"
#include <TimeLib.h>
#include "../network/NetworkMgr.hpp"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void c_OLED::Begin()
{
    // DEBUG_START;
    // DEBUG_V("OLED BEGIN");
    // DEBUG_V(String("OLED Enabled: ") + String("WIDTH: " + SCREEN_WIDTH + " HEIGHT: " + SCREEN_HEIGHT + "ADDRESS: " +SCREEN_ADDRESS);

    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setRotation(2);
    display.setCursor(0, 3);
    display.println(F("Loading...  "));
    display.display();
    display.startscrollright(0x00,0x0F);
    logcon(String(F("OLED Loaded")));
    // DEBUG_END;
} // Begin

void c_OLED::Update()
{
    // DEBUG_START;
    if (now() > updateTimer_OLED) // Lets check if OLED needs an update only once every 10 seconds
    {
        updateTimer_OLED += 10;
        display.stopscroll();
        String currIP = NetworkMgr.GetlocalIP ().toString ();
        String currHost;
        NetworkMgr.GetHostname(currHost);
        int rssi = constrain(WiFi.RSSI(), -100, -50);
        int quality = map(rssi, -100, -50, 0, 100);
        if (!dispIP.equals(currIP) || !dispHostName.equals(currHost) || dispRSSI != quality)
        {
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 2);
            display.println("IP: " + currIP);
            display.println("HOST: " + currHost);
            display.print("SIGNAL: ");
            display.print(quality);
            display.println("%");
            display.display();
            dispIP = currIP;
            dispHostName = currHost;
            dispRSSI = quality;
            logcon(String(F("OLED UPDATED")));
        }
    }
    // DEBUG_END;
} // Update

c_OLED OLED;
#endif // def USE_OLED