#ifndef PAGE_CONFIG_SERIAL_H_
#define PAGE_CONFIG_SERIAL_H_

void send_config_serial_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "devid") config.id = p->value();
            if (p->name() == "universe") config.universe = p->value().toInt(); 
            if (p->name() == "channel_start") config.channel_start = p->value().toInt();
            if (p->name() == "channel_count") config.channel_count = p->value().toInt();
            if (p->name() == "mode") config.serial_type = SerialType(p->value().toInt());
            if (p->name() == "baudrate") config.baudrate = BaudRate(p->value().toInt());
        }
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
    values += "devid|input|" + config.id + "\n";
    values += "universe|input|" + (String)config.universe + "\n";
    values += "channel_start|input|" + (String)config.channel_start + "\n";
    values += "channel_count|input|" + (String)config.channel_count + "\n";
    values += "mode|opt|" + String("DMX512|") + String(static_cast<uint8_t>(SerialType::DMX512)) + "\n";
    values += "mode|opt|" + String("Renard|") + String(static_cast<uint8_t>(SerialType::RENARD)) + "\n";
    values += "mode|input|" + String(static_cast<uint8_t>(config.serial_type)) + "\n";
    values += "baudrate|opt|" + String("38400|") + String(static_cast<uint32_t>(BaudRate::BR_38400)) + "\n";
    values += "baudrate|opt|" + String("57600|") + String(static_cast<uint32_t>(BaudRate::BR_57600)) + "\n";
    values += "baudrate|opt|" + String("115200|") + String(static_cast<uint32_t>(BaudRate::BR_115200)) + "\n";
    values += "baudrate|opt|" + String("230400|") + String(static_cast<uint32_t>(BaudRate::BR_230400)) + "\n";
    values += "baudrate|opt|" + String("250000|") + String(static_cast<uint32_t>(BaudRate::BR_250000)) + "\n";
    values += "baudrate|opt|" + String("460800|") + String(static_cast<uint32_t>(BaudRate::BR_460800)) + "\n";
    values += "baudrate|input|" + String(static_cast<uint32_t>(config.baudrate)) + "\n";
    values += "title|div|" + config.id + " - Serial Config\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_CONFIG_SERIAL_H_ */
