#include <BW16BLETouch.h>
#include <stdlib.h>
#include <string.h>

BW16BLETouch touch;
const char help[] = "status | size W H | rotation DEG | pixel X Y | tap X Y | longpress X Y MS | swipe X1 Y1 X2 Y2 MS STEPS";

void execute(char* line) {
    char* cmd = strtok(line, " \t");
    if (!cmd) return;
    uint16_t args[6];
    unsigned count = 0;
    for (char* token = strtok(NULL, " \t"); token; token = strtok(NULL, " \t")) {
        char* end;
        unsigned long value = strtoul(token, &end, 10);
        if (*token < '0' || *token > '9' || *end || value > 65535 || count == 6) {
            Serial.println("ERR invalid argument");
            return;
        }
        args[count++] = uint16_t(value);
    }
    bool ok;
    if (!strcmp(cmd, "status") && !count) {
        Serial.println(touch.isConnected() ? "connected" : "disconnected");
        return;
    } else if (!strcmp(cmd, "size") && count == 2) {
        ok = touch.setScreenResolution(args[0], args[1]);
    } else if (!strcmp(cmd, "rotation") && count == 1) {
        ok = touch.setRotation(args[0]);
    } else if (!strcmp(cmd, "pixel") && count == 2) {
        ok = touch.tapPixel(args[0], args[1]);
    } else if (!strcmp(cmd, "tap") && count == 2) {
        ok = touch.tap(args[0], args[1]);
    } else if (!strcmp(cmd, "longpress") && count == 3) {
        ok = touch.longPress(args[0], args[1], args[2]);
    } else if (!strcmp(cmd, "swipe") && count == 6) {
        ok = touch.swipe(args[0], args[1], args[2], args[3], args[4], args[5]);
    } else {
        Serial.println(help);
        return;
    }
    Serial.println(ok ? "OK" : "ERR check connection, bounds or state");
}

void setup() {
    Serial.begin(115200);
    touch.begin();
    Serial.println(help);
}

void loop() {
    static char line[96];
    static size_t used = 0;
    static bool discard = false;
    while (Serial.available()) {
        int c = Serial.read();
        if (c == '\r' || c == '\n') {
            line[used] = 0;
            if (discard) Serial.println("ERR invalid line");
            else execute(line);
            used = 0;
            discard = false;
        } else if (c <= 0 || used >= sizeof(line) - 1) {
            discard = true;
        } else {
            line[used++] = char(c);
        }
    }
    delay(1);
}
