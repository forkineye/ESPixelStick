#ifndef PAGE_ADMIN_H
#define PAGE_ADMIN_H

const char PAGE_ADMIN[] PROGMEM = R"=====(
<title>ESPS Admin</title>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<link rel="stylesheet" href="/style.css" type="text/css"/>
<script src="/microajax.js"></script>
<a href="/" class="btn btn--s"><</a>&nbsp;&nbsp;<strong>Administration</strong>
<hr>
<form action="" method="get">
<table border="0" cellspacing="0" cellpadding="3">
<tr><td align="right">FW Version :</td><td><span id="version"></span></td></tr>
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
    /* Placeholder for future config save 
    if (web.args()) {
        for (uint8_t i = 0; i < web.args(); i++) {
        }
        saveConfig();
    }
    */
    sendPage(PAGE_ADMIN, sizeof(PAGE_ADMIN), PTYPE_HTML);
}

void send_admin_vals() {
    String values = "";
    values += "version|div|" + (String)VERSION + "\n";
    web.send(200, PTYPE_PLAIN, values);
}

#endif
