#ifndef PAGE_CONFIG_PIXEL_H_
#define PAGE_CONFIG_PIXEL_H_

void send_config_pixel_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        config.gamma = false;
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "devid") config.id = p->value();
            if (p->name() == "universe") config.universe = p->value().toInt(); 
            if (p->name() == "channel_start") config.channel_start = p->value().toInt();
            if (p->name() == "pixel_count") config.channel_count = p->value().toInt() * 3;
            if (p->name() == "pixel_type") config.pixel_type = PixelType(p->value().toInt());
            if (p->name() == "pixel_color") config.pixel_color = PixelColor(p->value().toInt());
                        if (p->name() == "gamma") config.gamma = true;
        }
        saveConfig();

        AsyncWebServerResponse *response = request->beginResponse(303);
        response->addHeader("Location", request->url());
        request->send(response);
    } else {
        request->send(400);
    }
}

void send_config_pixel_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "devid|input|" + config.id + "\n";
    values += "universe|input|" + (String)config.universe + "\n";
    values += "channel_start|input|" + (String)config.channel_start + "\n";
    values += "pixel_count|input|" + String(config.channel_count / 3) + "\n";
    values += "pixel_type|opt|" + String("WS2811 800kHz|") + String(static_cast<uint8_t>(PixelType::WS2811)) + "\n";
    values += "pixel_type|opt|" + String("GE Color Effects|") + String(static_cast<uint8_t>(PixelType::GECE)) + "\n";
    values += "pixel_type|input|" + String(static_cast<uint8_t>(config.pixel_type)) + "\n";
    values += "pixel_color|opt|" + String("RGB|") + String(static_cast<uint8_t>(PixelColor::RGB)) + "\n";
    values += "pixel_color|opt|" + String("GRB|") + String(static_cast<uint8_t>(PixelColor::GRB)) + "\n";
    values += "pixel_color|opt|" + String("BRG|") + String(static_cast<uint8_t>(PixelColor::BRG)) + "\n";
    values += "pixel_color|opt|" + String("RBG|") + String(static_cast<uint8_t>(PixelColor::RBG)) + "\n";
    values += "pixel_color|input|" + String(static_cast<uint8_t>(config.pixel_color)) + "\n";
    values += "gamma|chk|" + String(config.gamma ? "checked" : "") + "\n";
    values += "title|div|" + config.id + " - Pixel Config\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_CONFIG_PIXEL_H_ */
