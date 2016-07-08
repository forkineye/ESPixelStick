#ifndef PAGE_STATUS_E131_H_
#define PAGE_STATUS_E131_H_

void send_status_e131_vals(AsyncWebServerRequest *request) {
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
    values += "title|div|" + config.id + " - E1.31 Status\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_STATUS_E131_H_ */
