#include <BW16BLETouch.h>

BW16BLETouch touch;
bool demonstrated = false;

void setup() {
    Serial.begin(115200);
    touch.begin("BW16 Touch", "BW16BLETouch");
    Serial.println("Open a drawing or touch-test screen, then pair BW16 Touch.");
}

void loop() {
    if (!touch.isConnected()) {
        demonstrated = false;
    } else if (!demonstrated) {
        demonstrated = true;
        delay(2000); // Give the host time to discover HID and subscribe.
        bool ok = touch.tap(16384, 16384);
        delay(500);
        ok = ok && touch.longPress(8192, 8192, 800);
        delay(500);
        ok = ok && touch.swipe(4096, 24576, 28672, 24576, 600, 30);
        Serial.println(ok ? "Demo submitted" : "Demo interrupted");
    }
    delay(20);
}
