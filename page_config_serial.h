#ifndef PAGE_CONFIG_SERIAL_H_
#define PAGE_CONFIG_SERIAL_H_

void send_config_serial_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "devid") urldecode(p->value()).toCharArray(config.name, sizeof(config.name));
            if (p->name() == "universe") config.universe = p->value().toInt(); 
            if (p->name() == "channel_start") config.channel_start = p->value().toInt();
            if (p->name() == "channel_count") config.channel_count = p->value().toInt();
            if (p->name() == "serial_type") config.serial_type = (serial_t)p->value().toInt();
            if (p->name() == "serial_baud") config.serial_baud = p->value().toInt();
        }
        updateSerialConfig();
        saveConfig();

        AsyncWebServerResponse *response = request->beginResponse(303);
        response->addHeader("Location", request->url());
        request->send(response);
    } else {
        request->send(400);
    }
}

void send_config_serial_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "devid|input|" + (String)config.name + "\n";
    values += "universe|input|" + (String)config.universe + "\n";
    values += "channel_start|input|" + (String)config.channel_start + "\n";
    values += "channel_count|input|" + (String)config.channel_count + "\n";
    values += "serial_type|opt|" + String("Renard|") + (String)SERIAL_RENARD + "\n";
    values += "serial_type|opt|" + String("DMX|") + (String)SERIAL_DMX + "\n";
    values += "serial_type|input|" + (String)config.serial_type + "\n";
    values += "serial_baud|opt|" + String("38400|") + (String)38400 + "\n";
    values += "serial_baud|opt|" + String("57600|") + (String)57600 + "\n";
    values += "serial_baud|opt|" + String("115200|") + (String)115200 + "\n";
    values += "serial_baud|opt|" + String("230400|") + (String)230400 + "\n";
    values += "serial_baud|opt|" + String("250000|") + (String)250000 + "\n";
    values += "serial_baud|input|" + (String)config.serial_baud + "\n";
    values += "title|div|" + (String)config.name + " - Serial Config\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_CONFIG_SERIAL_H_ */
