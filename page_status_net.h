#ifndef PAGE_STATUS_NET_H
#define PAGE_STATUS_NET_H

const char PAGE_STATUS_NET[] PROGMEM = R"=====(
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<link rel="stylesheet" href="/style.css" type="text/css"/>
<script src="/microajax.js"></script> 
<a href="/" class="btn btn--s"><</a>&nbsp;&nbsp;<strong>Network Information</strong>
<hr>
<table border="0" cellspacing="0" cellpadding="3" style="width:310px">
<tr><td align="right">SSID :</td><td><span id="x_ssid"></span></td></tr>
<tr><td align="right">IP :</td><td><span id="x_ip"></span></td></tr>
<tr><td align="right">Netmask :</td><td><span id="x_netmask"></span></td></tr>
<tr><td align="right">Gateway :</td><td><span id="x_gateway"></span></td></tr>
<tr><td align="right">Mac :</td><td><span id="x_mac"></span></td></tr>
<tr><td colspan="2" align="center"><a href="javascript:GetState()" class="btn btn--m btn--blue">Refresh</a></td></tr>
</table>
<script>
	setTimeout(GetState,1000);
	function GetState()	{
		setValues("/status/netvals");
	}
</script>
)=====" ;

//
// FILL WITH INFOMATION
// 

void send_status_net_vals_html() {
    String values = "";
    values += "x_ssid|div|" + (String)WiFi.SSID() + "\n";
    values += "x_ip|div|" + (String)WiFi.localIP()[0] + "." + (String)WiFi.localIP()[1] + "." + (String)WiFi.localIP()[2] + "." + (String)WiFi.localIP()[3] + "\n";
    values += "x_gateway|div|" + (String)WiFi.gatewayIP()[0] + "." + (String)WiFi.gatewayIP()[1] + "." + (String)WiFi.gatewayIP()[2] + "." + (String)WiFi.gatewayIP()[3] + "\n";
    values += "x_netmask|div|" + (String)WiFi.subnetMask()[0] + "." + (String)WiFi.subnetMask()[1] + "." + (String) WiFi.subnetMask()[2] + "." + (String)WiFi.subnetMask()[3] + "\n";
    values += "x_mac|div|" + GetMacAddress() + "\n";
    web.send(200, "text/plain", values);
}

#endif