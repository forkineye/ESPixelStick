/*
 * DisplayOLED.h
 *
 * Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
 * Copyright (c) 2018, 2025 Shelby Merrick
 * http://www.forkineye.com
 */

 #ifndef DISPLAY_OLED_H
 #define DISPLAY_OLED_H
 
 #include "ESPixelStick.h"
 #include <ArduinoJson.h>
 #include <U8g2lib.h>
 #include <Preferences.h>
 #include "network/NetworkMgr.hpp"
 #include "FPPDiscovery.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/semphr.h"



 // Constants for timing and display
 #define OLED_TASK_DELAY_MS         250
 #define TOAST_DURATION_MS          5000
 #define TOAST_FLASH_INTERVAL_MS    500
 #define REBOOT_FLASH_INTERVAL_MS   400     // <- Added for reboot flash
 #define NETWORK_UPDATE_INTERVAL_MS 10000
 #define BUTTON_DEBOUNCE_DELAY_MS   1000
 
 // Enum for OLED display pages
 enum class DisplayPage
 {
     NETWORK_INFO,
     RUNNING_STATUS,
     UPLOAD_STATUS
 };
 
 // OLED display object (extern)

 
 class c_OLED
 {
 public:
     c_OLED();
     void Begin();
     void InitNow();
     void End();  // Proper cleanup
     void Update(bool forceUpdate = false);
     void UpdateNetworkInfo(bool forceUpdate = false);
     void UpdateUploadStatus(const String &filename, int progress);
     void UpdateRunningStatus();
     void Flip();
     void Poll();
     void ShowToast(const String &message);
     void ShowRebootScreen(); 
     bool flipState;
     bool isUploading;
     String uploadFilename;
     int uploadProgress;
     unsigned long lastUploadUpdate;
     bool isRebooting;
     void GetDriverName(String &name) const
     {
         name = "OLED";
     }
 
     // Button interrupt handler
     static void IRAM_ATTR buttonISR();
 
 private:
     // Toast state
     bool isToastActive = false;
     unsigned long toastStartTime = 0;
     String toastMessage; 
     int getSignalStrength(int rssi); 
     DisplayPage currentPage;
     unsigned long lastPageSwitchTime;
     unsigned long lastNetworkUpdate;
     const unsigned long pageSwitchInterval = 7500;
     String dispIP;
     String dispHostName;
     int dispRSSI;
 
     // Preferences instance for persistent storage
     Preferences preferences; 
     // Reboot screen state
     

 };
 
 extern c_OLED OLED;
 
 #endif // DISPLAY_OLED_H


