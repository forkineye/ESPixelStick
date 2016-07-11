#ifndef PAGE_CONFIG_NET_H_
#define PAGE_CONFIG_NET_H_

void send_config_net_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        config.dhcp = false;
        config.multicast = false;
        for ( uint8_t i = 0; i < request->params(); i++ ) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "ssid") config.ssid = p->value();
            if (p->name() == "password") config.passphrase = p->value();
            if (p->name() == "ip_0") if (checkRange(p->value())) config.ip[0] = p->value().toInt();
            if (p->name() == "ip_1") if (checkRange(p->value())) config.ip[1] = p->value().toInt();
            if (p->name() == "ip_2") if (checkRange(p->value())) config.ip[2] = p->value().toInt();
            if (p->name() == "ip_3") if (checkRange(p->value())) config.ip[3] = p->value().toInt();
            if (p->name() == "nm_0") if (checkRange(p->value())) config.netmask[0] = p->value().toInt();
            if (p->name() == "nm_1") if (checkRange(p->value())) config.netmask[1] = p->value().toInt();
            if (p->name() == "nm_2") if (checkRange(p->value())) config.netmask[2] = p->value().toInt();
            if (p->name() == "nm_3") if (checkRange(p->value())) config.netmask[3] = p->value().toInt();
            if (p->name() == "gw_0") if (checkRange(p->value())) config.gateway[0] = p->value().toInt();
            if (p->name() == "gw_1") if (checkRange(p->value())) config.gateway[1] = p->value().toInt();
            if (p->name() == "gw_2") if (checkRange(p->value())) config.gateway[2] = p->value().toInt();
            if (p->name() == "gw_3") if (checkRange(p->value())) config.gateway[3] = p->value().toInt();
            if (p->name() == "dhcp") config.dhcp = true;
            if (p->name() == "multicast") config.multicast = true;
        }
        saveConfig();

        request->send(200, "text/html",
                R"=====(<meta http-equiv="refresh" content="2; url=/"><strong>Rebooting...</strong>)=====");
        reboot = true;
    } else {
        request->send(400);
    }
}

void send_config_net_vals(AsyncWebServerRequest *request) {
    String values ="";
    values += "ssid|input|" + config.ssid + "\n";
    values += "password|input|" + config.passphrase + "\n";
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
    values += "title|div|" + config.id + " - Net Config\n";
    request->send(200, "text/plain", values);
}

void send_survey_vals(AsyncWebServerRequest *request) {
    String networks = "";
    int scanStatus = WiFi.scanComplete();

    if (scanStatus == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true);
        networks = "<font color='#FF0000'>Scan in progress...</font>";
    } else if (scanStatus == 0) {
        networks = "<font color='#FF0000'>No networks found!</font>";
    } else if (scanStatus) {
        networks = "Found " + String(scanStatus) + " Networks<br>";
        networks += "<table border='0' cellspacing='0' cellpadding='3'>";
        networks += "<tr bgcolor='#DDDDDD' ><td><strong>Name</strong></td><td><strong>Quality</strong></td><td><strong>Enc</strong></td><tr>";
        for (int i = 0; i < scanStatus; ++i) {
            int quality = 0;
            if (WiFi.RSSI(i) <= -100) {
                quality = 0;
            } else if (WiFi.RSSI(i) >= -50) {
                quality = 100;
            } else {
                quality = 2 * (WiFi.RSSI(i) + 100);
            }
            networks += "<tr><td><a href='javascript:selssid(\""  +  String(WiFi.SSID(i))  + "\")'>"  +  String(WiFi.SSID(i))  + "</a></td><td>" +  String(quality) + "%</td><td>" +  String((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*")  + "</td></tr>";
        }
        networks += "</table>";

        WiFi.scanDelete();
    }
    String values = "";
    values += "networks|div|" + networks + "\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_CONFIG_NET_H_ */
