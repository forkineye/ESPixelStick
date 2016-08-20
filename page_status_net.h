#ifndef PAGE_STATUS_NET_H_
#define PAGE_STATUS_NET_H_

void send_status_net_vals(AsyncWebServerRequest *request) {
    int quality = 0;
    long rssi = WiFi.RSSI();

    if (rssi <= -100)
        quality = 0;
    else if (rssi >= -50)
        quality = 100;
    else
        quality = 2 * (rssi + 100);

    String values = "";
    values += "x_ssid|div|" + (String)WiFi.SSID() + "\n";
    values += "x_hostname|div|" + (String)WiFi.hostname() + "\n";
    values += "x_ip|div|" + (String)WiFi.localIP()[0] + "." + (String)WiFi.localIP()[1] + "." + (String)WiFi.localIP()[2] + "." + (String)WiFi.localIP()[3] + "\n";
    values += "x_gateway|div|" + (String)WiFi.gatewayIP()[0] + "." + (String)WiFi.gatewayIP()[1] + "." + (String)WiFi.gatewayIP()[2] + "." + (String)WiFi.gatewayIP()[3] + "\n";
    values += "x_netmask|div|" + (String)WiFi.subnetMask()[0] + "." + (String)WiFi.subnetMask()[1] + "." + (String) WiFi.subnetMask()[2] + "." + (String)WiFi.subnetMask()[3] + "\n";
    values += "x_mac|div|" + GetMacAddress() + "\n";
    values += "x_rssi|div|" + (String)rssi + "\n";
    values += "x_quality|div|" + (String)quality + "\n";
    values += "title|div|" + config.id + " - Net Status\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_STATUS_NET_H_ */
