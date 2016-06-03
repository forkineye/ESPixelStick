#ifndef PAGE_CONFIG_NET_H
#define PAGE_CONFIG_NET_H

const char PAGE_CONFIG_NET[] PROGMEM = R"=====(
<a href="/" class="btn btn--s">&lt;</a>&nbsp;&nbsp;<strong>Network Configuration</strong> <hr> Connect to Router with these settings:<br> <form action=""> <table border="0" cellspacing="0" cellpadding="3" style="width:360px"> <tr><td align="right">SSID :</td><td><input id="ssid" name="ssid" value=""></td></tr> <tr><td align="right">Password :</td><td><input id="password" name="password" value=""></td></tr> <tr><td align="right">DHCP :</td><td><input type="checkbox" id="dhcp" name="dhcp"></td></tr> <tr><td align="right">IP :</td><td><input id="ip_0" name="ip_0" size="3">.<input id="ip_1" name="ip_1" size="3">.<input id="ip_2" name="ip_2" size="3">.<input id="ip_3" name="ip_3" value="" size="3"></td></tr> <tr><td align="right">Netmask :</td><td><input id="nm_0" name="nm_0" size="3">.<input id="nm_1" name="nm_1" size="3">.<input id="nm_2" name="nm_2" size="3">.<input id="nm_3" name="nm_3" size="3"></td></tr> <tr><td align="right">Gateway :</td><td><input id="gw_0" name="gw_0" size="3">.<input id="gw_1" name="gw_1" size="3">.<input id="gw_2" name="gw_2" size="3">.<input id="gw_3" name="gw_3" size="3"></td></tr> <tr><td align="right">Multicast :</td><td><input type="checkbox" id="multicast" name="multicast"></td></tr> <tr><td align="right">Stream Mode :</td><td><select id="protocol" name="protocol"></select></td></tr> <tr><td colspan="2" align="center"><input type="submit" style="width:150px" class="btn btn--m btn--blue" value="Save"></td></tr> </table> </form> <hr> <strong>Connection State:</strong><div id="connectionstate">N/A</div> <hr> <strong>Networks:</strong><br> <table border="0" cellspacing="3" style="width:310px"> <tr><td><div id="networks">Scanning...</div></td></tr> <tr><td align="center"><a href="javascript:GetState()" style="width:150px" class="btn btn--m btn--blue">Refresh</a></td></tr> </table> <script>function GetState(){setValues("/config/connectionstate")}function selssid(e){document.getElementById("ssid").value=e}setValues("/config/netvals"),setTimeout(GetState,2e3);</script>
)=====";

/* No source .html for this */
const char PAGE_RELOAD_NET[] PROGMEM = R"=====(
<meta http-equiv="refresh" content="2; url=/config/net.html">
<strong>Please Wait....Configuring and Restarting.</strong>
)=====";

//
//  SEND HTML PAGE OR IF A FORM SUMBITTED VALUES, PROCESS THESE VALUES
// 

void send_config_net_html() {
    if (web.args()) { // Save Settings
        config.dhcp = false;
        config.multicast = false;
        for ( uint8_t i = 0; i < web.args(); i++ ) {
            if (web.argName(i) == "ssid") urldecode(web.arg(i)).toCharArray(config.ssid, sizeof(config.ssid));
            if (web.argName(i) == "password") urldecode(web.arg(i)).toCharArray(config.passphrase, sizeof(config.passphrase));
            if (web.argName(i) == "ip_0") if (checkRange(web.arg(i))) config.ip[0] = web.arg(i).toInt();
            if (web.argName(i) == "ip_1") if (checkRange(web.arg(i))) config.ip[1] = web.arg(i).toInt();
            if (web.argName(i) == "ip_2") if (checkRange(web.arg(i))) config.ip[2] = web.arg(i).toInt();
            if (web.argName(i) == "ip_3") if (checkRange(web.arg(i))) config.ip[3] = web.arg(i).toInt();
            if (web.argName(i) == "nm_0") if (checkRange(web.arg(i))) config.netmask[0] = web.arg(i).toInt();
            if (web.argName(i) == "nm_1") if (checkRange(web.arg(i))) config.netmask[1] = web.arg(i).toInt();
            if (web.argName(i) == "nm_2") if (checkRange(web.arg(i))) config.netmask[2] = web.arg(i).toInt();
            if (web.argName(i) == "nm_3") if (checkRange(web.arg(i))) config.netmask[3] = web.arg(i).toInt();
            if (web.argName(i) == "gw_0") if (checkRange(web.arg(i))) config.gateway[0] = web.arg(i).toInt();
            if (web.argName(i) == "gw_1") if (checkRange(web.arg(i))) config.gateway[1] = web.arg(i).toInt();
            if (web.argName(i) == "gw_2") if (checkRange(web.arg(i))) config.gateway[2] = web.arg(i).toInt();
            if (web.argName(i) == "gw_3") if (checkRange(web.arg(i))) config.gateway[3] = web.arg(i).toInt();
            if (web.argName(i) == "dhcp") config.dhcp = true;
            if (web.argName(i) == "multicast") config.multicast = true;
            if (web.argName(i) == "protocol") config.protocol = (stream_mode_t)web.arg(i).toInt();
        }
        saveConfig();
        delay(500);
        sendPage(PAGE_ADMIN_REBOOT, sizeof(PAGE_ADMIN_REBOOT), PTYPE_HTML);
        ESP.restart(); 

//        sendPage(PAGE_RELOAD_NET, sizeof(PAGE_RELOAD_NET), PTYPE_HTML);
//        ESP.restart();
    } else {
        sendPage(PAGE_CONFIG_NET, sizeof(PAGE_CONFIG_NET), PTYPE_HTML);
    }
}

//
//   FILL THE PAGE WITH VALUES
//

void send_config_net_vals() {
    String values ="";
    values += "ssid|input|" + String((char*)config.ssid) + "\n";
    values += "password|input|" + String((char*)config.passphrase) + "\n";
    values += "ip_0|input|" + (String)config.ip[0] + "\n";
    values += "ip_1|input|" + (String)config.ip[1] + "\n";
    values += "ip_2|input|" + (String)config.ip[2] + "\n";
    values += "ip_3|input|" + (String)config.ip[3] + "\n";
    values += "nm_0|input|" + (String)config.netmask[0] + "\n";
    values += "nm_1|input|" + (String)config.netmask[1] + "\n";
    values += "nm_2|input|" + (String)config.netmask[2] + "\n";
    values += "nm_3|input|" + (String)config.netmask[3] + "\n";
    values += "gw_0|input|" + (String)config.gateway[0] + "\n";
    values += "gw_1|input|" + (String)config.gateway[1] + "\n";
    values += "gw_2|input|" + (String)config.gateway[2] + "\n";
    values += "gw_3|input|" + (String)config.gateway[3] + "\n";
    values += "dhcp|chk|" + (String)(config.dhcp ? "checked" : "") + "\n";
    values += "multicast|chk|" + (String)(config.multicast ? "checked" : "") + "\n";
    values += "protocol|opt|" + String("sACN|") + (String)MODE_sACN + "\n";
    values += "protocol|opt|" + String("ArtNet|") + (String)MODE_ARTNET + "\n";
    values += "protocol|input|" + (String)config.protocol + "\n";
    values += "title|div|" + (String)config.name + " - Net Config\n";
    web.send(200, PTYPE_PLAIN, values);
}


//
//   FILL THE PAGE WITH NETWORKSTATE & NETWORKS
//
void send_connection_state_vals() {
    String state = "N/A";
    String Networks = "";
    if (WiFi.status() == 0) state = "Idle";
    else if (WiFi.status() == 1) state = "NO SSID AVAILBLE";
    else if (WiFi.status() == 2) state = "SCAN COMPLETED";
    else if (WiFi.status() == 3) state = "CONNECTED";
    else if (WiFi.status() == 4) state = "CONNECT FAILED";
    else if (WiFi.status() == 5) state = "CONNECTION LOST";
    else if (WiFi.status() == 6) state = "DISCONNECTED";

     int n = WiFi.scanNetworks();

     if (n == 0) {
         Networks = "<font color='#FF0000'>No networks found!</font>";
     } else {
        Networks = "Found " +String(n) + " Networks<br>";
        Networks += "<table border='0' cellspacing='0' cellpadding='3'>";
        Networks += "<tr bgcolor='#DDDDDD' ><td><strong>Name</strong></td><td><strong>Quality</strong></td><td><strong>Enc</strong></td><tr>";
        for (int i = 0; i < n; ++i) {
            int quality=0;
            if(WiFi.RSSI(i) <= -100) {
                    quality = 0;
            } else if(WiFi.RSSI(i) >= -50) {
                    quality = 100;
            } else {
                quality = 2 * (WiFi.RSSI(i) + 100);
            }
            Networks += "<tr><td><a href='javascript:selssid(\""  +  String(WiFi.SSID(i))  + "\")'>"  +  String(WiFi.SSID(i))  + "</a></td><td>" +  String(quality) + "%</td><td>" +  String((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*")  + "</td></tr>";
        }
        Networks += "</table>";
    }
   
    String values = "";
    values += "connectionstate|div|" + state + "\n";
    values += "networks|div|" + Networks + "\n";
    web.send(200, PTYPE_PLAIN, values);
}

#endif
