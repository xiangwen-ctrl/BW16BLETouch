#pragma once

#include <stdint.h>
#include <stddef.h>

// One BLE peripheral per board. Call from one task (normally Arduino loop).
class BW16BLETouch {
public:
    enum class Units { Hid, Pixels };

    // travelMs is measured from the preceding point; the first point uses zero.
    // holdMs keeps the finger down at this point after arrival.
    struct PathPoint {
        int32_t x;
        int32_t y;
        uint16_t travelMs;
        uint16_t holdMs;
    };

    static bool begin(const char* deviceName = "BW16 Touch",
                      const char* manufacturer = "BW16BLETouch");
    static bool isConnected();
    static void end();

    // Dimensions of the screenshot in its current orientation.
    static bool setScreenResolution(uint16_t width, uint16_t height);
    static bool setRotation(uint16_t clockwiseDegrees);

    // Hid uses 0..32767 on each axis. Pixels uses 0..width-1, 0..height-1.
    static bool press(int32_t x, int32_t y, Units units = Units::Hid);
    static bool moveTo(int32_t x, int32_t y, Units units = Units::Hid);
    static bool release();
    static bool tap(int32_t x, int32_t y, uint16_t holdMs = 60, Units units = Units::Hid);
    static bool doubleTap(int32_t x, int32_t y, uint16_t intervalMs = 120, Units units = Units::Hid);
    static bool longPress(int32_t x, int32_t y, uint16_t holdMs = 800, Units units = Units::Hid);
    static bool swipe(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                      uint16_t durationMs = 400, uint16_t steps = 20, Units units = Units::Hid);
    static bool longPressDrag(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                              uint16_t holdMs = 800, uint16_t durationMs = 400,
                              uint16_t steps = 20, Units units = Units::Hid);
    // Validates the complete path before pressing. The array must stay valid
    // and unchanged until the call returns. At least two points are required.
    static bool swipePath(const PathPoint* points, size_t count,
                          uint16_t stepsPerSegment = 20, Units units = Units::Hid);

    static bool pressPixel(int32_t x, int32_t y) { return press(x, y, Units::Pixels); }
    static bool moveToPixel(int32_t x, int32_t y) { return moveTo(x, y, Units::Pixels); }
    static bool tapPixel(int32_t x, int32_t y, uint16_t holdMs = 60) {
        return tap(x, y, holdMs, Units::Pixels);
    }
    static bool doubleTapPixel(int32_t x, int32_t y, uint16_t intervalMs = 120) {
        return doubleTap(x, y, intervalMs, Units::Pixels);
    }
    static bool longPressPixel(int32_t x, int32_t y, uint16_t holdMs = 800) {
        return longPress(x, y, holdMs, Units::Pixels);
    }
    static bool swipePixel(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                           uint16_t durationMs = 400, uint16_t steps = 20) {
        return swipe(x1, y1, x2, y2, durationMs, steps, Units::Pixels);
    }
    static bool longPressDragPixel(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                   uint16_t holdMs = 800, uint16_t durationMs = 400,
                                   uint16_t steps = 20) {
        return longPressDrag(x1, y1, x2, y2, holdMs, durationMs, steps, Units::Pixels);
    }
    static bool swipePathPixel(const PathPoint* points, size_t count,
                               uint16_t stepsPerSegment = 20) {
        return swipePath(points, count, stepsPerSegment, Units::Pixels);
    }
};
