#include <BW16BLETouch.h>

BW16BLETouch touch;
using Point = BW16BLETouch::PathPoint;

// Screenshot coordinates for a generic 1080 x 2400 touch-test surface.
// Each row is X, Y, travel time from the preceding point, hold after arrival.
const Point path[] = {
    {180, 1500,   0,   0},
    {420,  900, 900,   0},
    {690, 1500, 900,   0},
    {900,  950, 900,   0}
};
const Point pausedPath[] = {
    {180, 1500,   0, 850},
    {820, 1450, 900, 600},
    {350,  900, 900,   0},
    {900,  850, 900,   0}
};

void setup() {
    Serial.begin(115200);
    touch.setScreenResolution(1080, 2400);
    touch.begin();
    Serial.println("Pair and open a touch-test app. d = hold then drag; p = path; h = path with holds");
}

void loop() {
    if (Serial.available()) {
        bool ok;
        switch (Serial.read()) {
            case 'd':
                ok = touch.longPressDragPixel(540, 900, 540, 1500, 850, 900, 45);
                break;
            case 'p':
                ok = touch.swipePathPixel(path, sizeof(path) / sizeof(path[0]), 45);
                break;
            case 'h':
                ok = touch.swipePathPixel(pausedPath, sizeof(pausedPath) / sizeof(pausedPath[0]), 45);
                break;
            default:
                return;
        }
        Serial.println(ok ? "Gesture submitted" : "Gesture failed");
    }
    delay(1);
}
