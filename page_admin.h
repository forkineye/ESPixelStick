#ifndef PAGE_ADMIN_H_
#define PAGE_ADMIN_H_

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
    if (!index) {
        Serial.printf("\nUpload Started: %s\n", filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        Serial.printf("Free space: %u\n", maxSketchSpace);
        if (!Update.begin(maxSketchSpace, U_FLASH))
            Update.printError(LOG_PORT);
    }

    Update.write(data, len);
    if (final) {
        Serial.printf("\nUpload Finished: %s, %u bytes\n", filename.c_str(), index + len);
        Update.end(true);
    }
}

void send_admin_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "version|div|" + (String)VERSION + "\n";
    values += "title|div|" + config.id + " - Admin\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_ADMIN_H_ */
