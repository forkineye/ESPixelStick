#ifndef PAGE_STATUS_NET_H
#define PAGE_STATUS_NET_H

const char PAGE_STATUS_NET[] PROGMEM = R"=====(
<a href="/" class="btn btn--s">&lt;</a>&nbsp;&nbsp;<strong>Network Status</strong> <hr> <table border="0" cellspacing="0" cellpadding="3" style="width:310px"> <tr><td align="right">SSID :</td><td><span id="x_ssid"></span></td></tr> <tr><td align="right">IP :</td><td><span id="x_ip"></span></td></tr> <tr><td align="right">Netmask :</td><td><span id="x_netmask"></span></td></tr> <tr><td align="right">Gateway :</td><td><span id="x_gateway"></span></td></tr> <tr><td align="right">Mac :</td><td><span id="x_mac"></span></td></tr> <tr><td align="right">RSSI :</td><td><span id="x_rssi"></span>dBm / <span id="x_quality"></span>%</td></tr> <tr><td colspan="2" align="center"><a href="javascript:GetState()" class="btn btn--m btn--blue">Refresh</a></td></tr> </table> <script>function GetState(){setValues("/status/netvals")}setTimeout(GetState,1e3);</script>
)=====" ;

//
// FILL WITH INFOMATION
// 

void send_status_net_vals() {
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
    values += "x_ip|div|" + (String)WiFi.localIP()[0] + "." + (String)WiFi.localIP()[1] + "." + (String)WiFi.localIP()[2] + "." + (String)WiFi.localIP()[3] + "\n";
    values += "x_gateway|div|" + (String)WiFi.gatewayIP()[0] + "." + (String)WiFi.gatewayIP()[1] + "." + (String)WiFi.gatewayIP()[2] + "." + (String)WiFi.gatewayIP()[3] + "\n";
    values += "x_netmask|div|" + (String)WiFi.subnetMask()[0] + "." + (String)WiFi.subnetMask()[1] + "." + (String) WiFi.subnetMask()[2] + "." + (String)WiFi.subnetMask()[3] + "\n";
    values += "x_mac|div|" + GetMacAddress() + "\n";
    values += "x_rssi|div|" + (String)rssi + "\n";
    values += "x_quality|div|" + (String)quality + "\n";
    values += "title|div|" + String("Net Status - ") + (String)config.name + "\n";
    web.send(200, PTYPE_PLAIN, values);
}

#endif
