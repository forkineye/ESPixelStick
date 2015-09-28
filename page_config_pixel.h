#ifndef PAGE_CONFIG_PIXEL_H
#define PAGE_CONFIG_PIXEL_H

const char PAGE_CONFIG_PIXEL[] PROGMEM = R"=====(
<a href="/" class="btn btn--s">&lt;</a>&nbsp;&nbsp;<strong>Pixel Configuration</strong> <hr> <form action=""> <table border="0" cellspacing="0" cellpadding="3"> <tr><td align="right">Device ID :</td><td><input id="devid" name="devid" value=""></td></tr> <tr><td align="right">Universe :</td><td><input id="universe" name="universe" value=""></td></tr> <tr><td align="right">Start Channel :</td><td><input id="channel_start" name="channel_start" value=""></td></tr> <tr><td align="right">Pixel Count :</td><td><input id="pixel_count" name="pixel_count" value=""></td></tr> <tr><td align="right">Pixel Type :</td><td><select id="pixel_type" name="pixel_type"></select></td></tr> <tr><td align="right">Color Order :</td><td><select id="pixel_color" name="pixel_color"></select></td></tr> <tr><td align="right">PPU :</td><td><input id="ppu" name="ppu" value=""></td></tr> <tr><td align="right">Gamma Map :</td><td><input type="checkbox" id="gamma" name="gamma"></td></tr> <tr><td colspan="2" align="center"><input type="submit" style="width:150px" class="btn btn--m btn--blue" value="Save"></td></tr> </table> </form> <script>setValues("/config/pixelvals");</script>
)=====";

void send_config_pixel_html() {
    if (web.args()) {  // Save Settings
        config.gamma = 0;
        for (uint8_t i = 0; i < web.args(); i++) {
            if (web.argName(i) == "devid") urldecode(web.arg(i)).toCharArray(config.name, sizeof(config.name));
            if (web.argName(i) == "universe") config.universe = web.arg(i).toInt(); 
            if (web.argName(i) == "channel_start") config.channel_start = web.arg(i).toInt();
            if (web.argName(i) == "pixel_count") config.pixel_count = web.arg(i).toInt();
            if (web.argName(i) == "pixel_type") config.pixel_type = (pixel_t)web.arg(i).toInt();
            if (web.argName(i) == "pixel_color") config.pixel_color = (color_t)web.arg(i).toInt();
            if (web.argName(i) == "ppu") config.ppu = web.arg(i).toInt();
            //if (web.argName(i) == "gamma") config.gamma = web.arg(i).toFloat();
            if (web.argName(i) == "gamma") config.gamma = 1.0;
        }
        updatePixelConfig();
        saveConfig();
    }
    sendPage(PAGE_CONFIG_PIXEL, sizeof(PAGE_CONFIG_PIXEL), PTYPE_HTML);
}

void send_config_pixel_vals() {
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
    web.send(200, PTYPE_PLAIN, values);
}

#endif
