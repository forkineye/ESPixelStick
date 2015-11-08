#ifndef PAGE_CONFIG_SERIAL_H
#define PAGE_CONFIG_SERIAL_H

const char PAGE_CONFIG_SERIAL[] PROGMEM = R"=====(<a href="/" class="btn btn--s">&lt;</a>&nbsp;&nbsp;<strong>Serial Configuration</strong> <hr> <form action=""> <table border="0" cellspacing="0" cellpadding="3"> <tr><td align="right">Device ID :</td><td><input id="devid" name="devid" value=""></td></tr> <tr><td align="right">Universe :</td><td><input id="universe" name="universe" value=""></td></tr> <tr><td align="right">Start Channel :</td><td><input id="channel_start" name="channel_start" value=""></td></tr> <tr><td align="right">Channel Count :</td><td><input id="channel_count" name="channel_count" value=""></td></tr> <tr><td align="right">Serial Type :</td><td><select id="serial_type" name="serial_type"></select></td></tr> <tr><td align="right">Serial Baud :</td><td><select id="serial_baud" name="serial_baud"></select></td></tr> <tr><td colspan="2" align="center"><input type="submit" style="width:150px" class="btn btn--m btn--blue" value="Save"></td></tr> </table> </form> <script>setValues("/config/serialvals");</script>)=====";

void send_config_serial_html() {
    if (web.args()) {  // Save Settings
        for (uint8_t i = 0; i < web.args(); i++) {
            if (web.argName(i) == "devid") urldecode(web.arg(i)).toCharArray(config.name, sizeof(config.name));
            if (web.argName(i) == "universe") config.universe = web.arg(i).toInt(); 
            if (web.argName(i) == "channel_start") config.channel_start = web.arg(i).toInt();
            if (web.argName(i) == "channel_count") config.channel_count = web.arg(i).toInt();
						if (web.argName(i) == "serial_type") config.serial_type = (serial_t)web.arg(i).toInt();
            if (web.argName(i) == "serial_baud") config.serial_baud = web.arg(i).toInt();
        }
        updateSerialConfig();
        saveConfig();
    }
    sendPage(PAGE_CONFIG_SERIAL, sizeof(PAGE_CONFIG_SERIAL), PTYPE_HTML);
}

void send_config_serial_vals() {
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
    web.send(200, PTYPE_PLAIN, values);
}

#endif
