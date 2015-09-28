#ifndef PAGE_STATUS_E131_H
#define PAGE_STATUS_E131_H

const char PAGE_STATUS_E131[] PROGMEM = R"=====(
<a href="/" class="btn btn--s">&lt;</a>&nbsp;&nbsp;<strong>E1.31 Status</strong> <hr> <table border="0" cellspacing="0" cellpadding="3" style="width:310px"> <tr><td align="right">Universe Range :</td><td><span id="uni_first"></span> to <span id="uni_last"></span></td></tr> <tr><td align="right">Last Universe :</td><td><span id="universe"></span></td></tr> <tr><td align="right">Total Packets :</td><td><span id="num_packets"></span></td></tr> <tr><td align="right">Sequence Errors :</td><td><span id="sequence_errors"></span></td></tr> <tr><td align="right">Packet Errors :</td><td><span id="packet_errors"></span></td></tr> <tr><td colspan="2" align="center"><a href="javascript:GetState()" class="btn btn--m btn--blue">Refresh</a></td></tr> </table> <script>function GetState(){setValues("/status/e131vals")}setTimeout(GetState,1e3);</script>
)=====" ;

//
// FILL WITH INFOMATION
// 

void send_status_e131_vals() {
    //TODO: Show sequence errors for each universe
    uint32_t seqErrors = 0;
    for (int i = 0; i < ((uniLast + 1) - config.universe); i++)
        seqErrors =+ seqError[i];
            
    String values = "";
    values += "uni_first|div|" + (String)config.universe + "\n";
    values += "uni_last|div|" + (String)uniLast + "\n";
    values += "universe|div|" + (String)e131.universe + "\n";
    values += "num_packets|div|" + (String)e131.stats.num_packets + "\n";
    values += "sequence_errors|div|" + (String)seqErrors + "\n";
    values += "packet_errors|div|" + (String)e131.stats.packet_errors + "\n";
    values += "title|div|" + (String)config.name + " - E1.31 Status\n";
    web.send (200, PTYPE_PLAIN, values);
}

#endif
