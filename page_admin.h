#ifndef PAGE_ADMIN_H_
#define PAGE_ADMIN_H_

#include "EFUpdate.h"

const char REBOOT[] = R"=====(<meta http-equiv="refresh" content="2; url=/"><strong>Rebooting...</strong>)=====";

void send_admin_html(AsyncWebServerRequest *request) {
    if (request->hasParam("reboot", true)) {
        request->send(200, "text/html", REBOOT);
        reboot = true;
    } else if (request->hasParam("update"), true) {
        if (request->hasParam("updateFile", true, true)) {
            if (Update.hasError()) {
                request->send(200, "text/plain", "Error: " + String(Update.getError()));
            } else {
                request->send(200, "text/html", REBOOT);
                reboot = true;
            }
        } else {
            request->send(200, "text/plain", "No File specified for update.");
        }
    } else {
        request->send(400);
    }
}

void handle_fw_upload(AsyncWebServerRequest *request, String filename,
        size_t index, uint8_t *data, size_t len, bool final) {
    static EFUpdate update;

    if (!index) {
        WiFiUDP::stopAll();
        LOG_PORT.print(F("* Upload Started: "));
        LOG_PORT.println(filename.c_str());
        update.begin();
    }

    if (!update.process(data, len)) {
        LOG_PORT.println(F("*** UPDATE FAILURE ***"));
    }

    if (final) {
        LOG_PORT.println(F("* Upload Finished."));
        update.end();
        SPIFFS.begin();
        saveConfig();
    }
}

void send_admin_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "version|div|" + (String)VERSION + "\n";
    values += "title|div|" + config.id + " - Admin\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_ADMIN_H_ */
