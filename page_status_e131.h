#ifndef PAGE_STATUS_DMX_H
#define PAGE_STATUS_DMX_H

const char PAGE_STATUS_E131[] PROGMEM = R"=====(
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<link rel="stylesheet" href="/style.css" type="text/css"/>
<script src="/microajax.js"></script> 
<a href="/" class="btn btn--s"><</a>&nbsp;&nbsp;<strong>E1.31 Status</strong>
<hr>
<table border="0" cellspacing="0" cellpadding="3" style="width:310px">
<tr><td align="right">Last Universe :</td><td><span id="universe"></span></td></tr>
<tr><td align="right">Total Packets :</td><td><span id="num_packets"></span></td></tr>
<tr><td align="right">Sequence Errors :</td><td><span id="sequence_errors"></span></td></tr>
<tr><td align="right">Packet Errors :</td><td><span id="packet_errors"></span></td></tr>
<tr><td colspan="2" align="center"><a href="javascript:GetState()" class="btn btn--m btn--blue">Refresh</a></td></tr>
</table>
<script>
	setTimeout(GetState,1000);
	function GetState() {
		setValues("/status/e131vals");
	}
</script>
)=====" ;

//
// FILL WITH INFOMATION
// 

void send_status_e131_vals_html() {
    String values = "";
    values += "universe|div|" + (String)e131.universe + "\n";
    values += "num_packets|div|" + (String)e131.stats.num_packets + "\n";
    values += "sequence_errors|div|" + (String)e131.stats.sequence_errors + "\n";
    values += "packet_errors|div|" + (String)e131.stats.packet_errors + "\n";
    web.send (200, "text/plain", values);
}

#endif
