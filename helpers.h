#ifndef HELPERS_H_
#define HELPERS_H_

/* Check if value is between 0 and 255 */
bool checkRange(String Value) {
    if (Value.toInt() < 0 || Value.toInt() > 255)
        return false;
    else
        return true;
}

/* Plain text friendly MAC */
String GetMacAddress() {
    uint8_t mac[6];
    char macStr[18] = {0};
    WiFi.macAddress(mac);
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],  mac[1], mac[2], mac[3], mac[4], mac[5]);
    return  String(macStr);
}

/* Convert a single hex digit character to its integer value - from https://code.google.com/p/avr-netino */
unsigned char h2int(char c) {
    if (c >= '0' && c <='9')
        return((unsigned char)c - '0');
    if (c >= 'a' && c <='f')
        return((unsigned char)c - 'a' + 10);
    if (c >= 'A' && c <='F')
        return((unsigned char)c - 'A' + 10);
    return(0);
}

/* Based on https://code.google.com/p/avr-netino */
String urldecode(String input) {
    char c;
    String ret = "";

    for (byte t = 0; t < input.length(); t++) {
        c = input[t];
        if (c == '+') c = ' ';
        if (c == '%') {
            t++;
            c = input[t];
            t++;
            c = (h2int(c) << 4) | h2int(input[t]);
        }
        ret.concat(c);
    }
    return ret;
}

#endif /* HELPERS_H_ */
