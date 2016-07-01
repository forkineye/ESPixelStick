#ifndef PAGE_CONFIG_SERIAL_H_
#define PAGE_CONFIG_SERIAL_H_

void send_config_renard_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "devid") urldecode(p->value()).toCharArray(config.id, sizeof(config.id));
            if (p->name() == "universe") config.universe = p->value().toInt(); 
            if (p->name() == "channel_start") config.channel_start = p->value().toInt();
            if (p->name() == "channel_count") config.channel_count = p->value().toInt();
            if (p->name() == "renard_baud") config.renard_baud = p->value().toInt();
        }
        updateRenardConfig();
        saveConfig();

        AsyncWebServerResponse *response = request->beginResponse(303);
        response->addHeader("Location", request->url());
        request->send(response);
    } else {
        request->send(400);
    }
}

void send_config_renard_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "devid|input|" + (String)config.id + "\n";
    values += "universe|input|" + (String)config.universe + "\n";
    values += "channel_start|input|" + (String)config.channel_start + "\n";
    values += "channel_count|input|" + (String)config.channel_count + "\n";
    values += "renard_baud|opt|" + String("38400|") + (String)38400 + "\n";
    values += "renard_baud|opt|" + String("57600|") + (String)57600 + "\n";
    values += "renard_baud|opt|" + String("115200|") + (String)115200 + "\n";
    values += "renard_baud|opt|" + String("230400|") + (String)230400 + "\n";
    values += "renard_baud|opt|" + String("250000|") + (String)250000 + "\n";
    values += "renard_baud|input|" + (String)config.renard_baud + "\n";
    values += "title|div|" + (String)config.id + " - Renard Config\n";
    request->send(200, "text/plain", values);
}

void send_config_dmx_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        config.dmx_passthru = false;
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "devid") urldecode(p->value()).toCharArray(config.id, sizeof(config.id));
            if (p->name() == "universe") config.universe = p->value().toInt(); 
            if (p->name() == "channel_start") config.channel_start = p->value().toInt();
            if (p->name() == "channel_count") config.channel_count = p->value().toInt();
            if (p->name() == "dmx_passthru") config.dmx_passthru = true;
        }
        updateDMXConfig();
        saveConfig();

        AsyncWebServerResponse *response = request->beginResponse(303);
        response->addHeader("Location", request->url());
        request->send(response);
    } else {
        request->send(400);
    }
}

void send_config_dmx_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "devid|input|" + (String)config.id + "\n";
    values += "universe|input|" + (String)config.universe + "\n";
    values += "channel_start|input|" + (String)config.channel_start + "\n";
    values += "channel_count|input|" + (String)config.channel_count + "\n";
    values += "dmx_passthru|chk|" + (String)(config.dmx_passthru ? "checked" : "") + "\n";
    values += "title|div|" + (String)config.id + " - DMX512 Config\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_CONFIG_SERIAL_H_ */
