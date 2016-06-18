#ifndef PAGE_CONFIG_PIXEL_H_
#define PAGE_CONFIG_PIXEL_H_

void send_config_pixel_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        config.gamma = 0;
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "devid") urldecode(p->value()).toCharArray(config.name, sizeof(config.name));
            if (p->name() == "universe") config.universe = p->value().toInt(); 
            if (p->name() == "channel_start") config.channel_start = p->value().toInt();
            if (p->name() == "pixel_count") config.pixel_count = p->value().toInt();
            if (p->name() == "pixel_type") config.pixel_type = (pixel_t)p->value().toInt();
            if (p->name() == "pixel_color") config.pixel_color = (color_t)p->value().toInt();
            if (p->name() == "ppu") config.ppu = p->value().toInt();
            //if (p->name() == "gamma") config.gamma = p->value().toFloat();
            if (p->name() == "gamma") config.gamma = 1.0;
        }
        updatePixelConfig();
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
    values += "devid|input|" + (String)config.name + "\n";
    values += "universe|input|" + (String)config.universe + "\n";
    values += "channel_start|input|" + (String)config.channel_start + "\n";
    values += "pixel_count|input|" + (String)config.pixel_count + "\n";
    values += "pixel_type|opt|" + String("WS2811 800kHz|") + (String)PIXEL_WS2811 + "\n";
    values += "pixel_type|opt|" + String("GE Color Effects|") + (String)PIXEL_GECE + "\n";
    values += "pixel_type|input|" + (String)config.pixel_type + "\n";
    values += "pixel_color|opt|" + String("RGB|") + (String)COLOR_RGB + "\n";
    values += "pixel_color|opt|" + String("GRB|") + (String)COLOR_GRB + "\n";
    values += "pixel_color|opt|" + String("BRG|") + (String)COLOR_BRG + "\n";
    values += "pixel_color|opt|" + String("RBG|") + (String)COLOR_RBG + "\n";
    values += "pixel_color|input|" + (String)config.pixel_color + "\n";
    values += "ppu|input|" + String(config.ppu) + "\n";
    //values += "gamma|input|" + String(config.gamma) + "\n";
    values += "gamma|chk|" + String(config.gamma ? "checked" : "") + "\n";
    values += "title|div|" + (String)config.name + " - Pixel Config\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_CONFIG_PIXEL_H_ */
