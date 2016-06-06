#ifndef PAGE_ADMIN_H
#define PAGE_ADMIN_H

const char PAGE_ADMIN[] PROGMEM = R"=====(
<a href="/" class="btn btn--s">&lt;</a>&nbsp;&nbsp;<strong>Administration</strong> <hr> <form action=""> <table border="0" cellspacing="0" cellpadding="3"> <tr><td align="right">FW Version :</td><td><span id="version"></span></td></tr> <tr><td align="right">ESPixelStick Mode :</td><td><select id="mode" name="mode"></select></td></tr> <tr><td colspan="2" align="center"><input type="submit" style="width:150px" class="btn btn--m btn--blue" value="Save"></td></tr> <tr><td colspan="2" align="center"><a href="/reboot" class="btn btn--m btn--red">Reboot</a></td></tr> </table> </form> <script>setValues("/adminvals");</script>
)=====";

const char PAGE_ADMIN_REBOOT[] PROGMEM = R"=====(
<meta http-equiv="refresh" content="2; url=/">
<strong>Please Wait, Rebooting....</strong>
)=====";

void send_admin_html() {
    
    if (web.args()) {  //Save Settings
        for (uint8_t i = 0; i < web.args(); i++) {
					if (web.argName(i) == "mode") config.mode = (ESP_mode_t)web.arg(i).toInt();
        }
        saveConfig();
				
				//Reboot to reload mode specific settings/pages. 
				sendPage(PAGE_ADMIN_REBOOT, sizeof(PAGE_ADMIN_REBOOT), PTYPE_HTML); 
				ESP.restart();
    }
    
    sendPage(PAGE_ADMIN, sizeof(PAGE_ADMIN), PTYPE_HTML);
}

void send_admin_vals() {
    String values = "";
    values += "version|div|" + (String)VERSION + "\n";
    values += "title|div|" + (String)config.name + " - Admin\n";
		values += "mode|opt|" + String("Pixel|") + (String)MODE_PIXEL + "\n";
    values += "mode|opt|" + String("Serial|") + (String)MODE_SERIAL + "\n";
    values += "mode|input|" + (String)config.mode + "\n";
    web.send(200, PTYPE_PLAIN, values);
}

#endif
