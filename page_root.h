#ifndef PAGE_ROOT_H_
#define PAGE_ROOT_H_

void send_root_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "name|div|" + (String)config.name + "\n";
    values += "title|div|" + String("ESPS - ") + (String)config.name + "\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_ROOT_H_ */
