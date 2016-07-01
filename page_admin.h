#ifndef PAGE_ADMIN_H_
#define PAGE_ADMIN_H_

void send_admin_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        if (request->hasParam("save", true)) {
            for (uint8_t i = 0; i < request->params(); i++) {
                AsyncWebParameter *p = request->getParam(i);
                if (p->name() == "mode")
                    config.mode = OutputMode(p->value().toInt());
            }
            saveConfig();
        }

        request->send(200, "text/html",
                R"=====(<meta http-equiv="refresh" content="2; url=/"><strong>Rebooting...</strong>)=====");
        reboot = true;
    } else {
        request->send(400);
    }
}

void send_admin_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "version|div|" + (String)VERSION + "\n";
    values += "title|div|" + (String)config.id + " - Admin\n";
    values += "mode|opt|" + String("Pixel|") + String(static_cast<uint8_t>(OutputMode::PIXEL)) + "\n";
    values += "mode|opt|" + String("DMX512|") + String(static_cast<uint8_t>(OutputMode::DMX512)) + "\n";
    values += "mode|opt|" + String("Renard|") + String(static_cast<uint8_t>(OutputMode::RENARD)) + "\n";
    values += "mode|input|" + String(static_cast<uint8_t>(config.mode)) + "\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_ADMIN_H_ */
