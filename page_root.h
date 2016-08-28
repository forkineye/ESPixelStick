#ifndef PAGE_ROOT_H_
#define PAGE_ROOT_H_

void send_root_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "name|div|" + config.id + "\n";
    values += "title|div|" + String("ESPS - ") + config.id + "\n";
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", values);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

#endif /* PAGE_ROOT_H_ */
