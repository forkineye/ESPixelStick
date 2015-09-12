#ifndef PAGE_ADMIN_H
#define PAGE_ADMIN_H

const char PAGE_ADMIN[] PROGMEM = R"=====(
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<link rel="stylesheet" href="/style.css" type="text/css"/>
<script src="/microajax.js"></script>
<a href="/" class="btn btn--s"><</a>&nbsp;&nbsp;<strong>Administration</strong>
<hr>
<form action="" method="get">
<table border="0" cellspacing="0" cellpadding="3">
<tr><td align="right">FW Version :</td><td><span id="version"></span></td></tr>
<tr><td align="right">Device ID :</td><td><input type="text" id="devid" name="devid" value=""></td></tr>
<tr><td colspan="2" align="center"><input type="submit" style="width:150px" class="btn btn--m btn--blue" value="Save"></td></tr>
<tr><td colspan="2" align="center"><a href="/reboot" class="btn btn--m btn--red">Reboot</a></td></tr>
</table>
</form>
<script>
    setValues("/adminvals");
</script>
)=====";

const char PAGE_ADMIN_REBOOT[] PROGMEM = R"=====(
<meta http-equiv="refresh" content="2; url=/">
<strong>Please Wait, Rebooting....</strong>
)=====";

void send_admin_html() {
    if (web.args()) {
        for (uint8_t i = 0; i < web.args(); i++) {
            if (web.argName(i) == "devid") urldecode(web.arg(i)).toCharArray(config.name, sizeof(config.name));
        }
        saveConfig();
    }
    web.send(200, "text/html", PAGE_ADMIN);
}

void send_admin_vals_html() {
    String values = "";
    values += "version|div|" + (String)VERSION + "\n";
    values += "devid|input|" + (String)config.name + "\n";
    web.send(200, "text/plain", values);
}

#endif
