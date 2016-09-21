#ifndef PAGE_ADMIN_H_
#define PAGE_ADMIN_H_

#include "EFUpdate.h"

const char REBOOT[] = R"=====(<meta http-equiv="refresh" content="2; url=/"><strong>Rebooting...</strong>)=====";
EFUpdate efupdate;

void send_admin_html(AsyncWebServerRequest *request) {
    if (request->hasParam("reboot", true)) {
        request->send(200, "text/html", REBOOT);
        reboot = true;
    } else if (request->hasParam("update"), true) {
        if (request->hasParam("updateFile", true, true)) {
            if (efupdate.hasError()) {
                request->send(200, "text/plain", "Update Error: " + String(efupdate.getError()));
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
    if (!index) {
        WiFiUDP::stopAll();
        LOG_PORT.print(F("* Upload Started: "));
        LOG_PORT.println(filename.c_str());
        efupdate.begin();
    }

    if (!efupdate.process(data, len)) {
        LOG_PORT.print(F("*** UPDATE ERROR: "));
        LOG_PORT.println(String(efupdate.getError()));

    }

    if (final) {
        LOG_PORT.println(F("* Upload Finished."));
        efupdate.end();
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
