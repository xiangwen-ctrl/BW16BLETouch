#include "BW16BLETouch.h"
#include <atomic>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <profile_client.h>
#include <string.h>

namespace {
const uint8_t touchReportId = 1;
uint8_t touchMap[] = {
    0x05, 0x0D,        // Digitizers
    0x09, 0x04,        // Touch Screen
    0xA1, 0x01,        // Application collection
    0x85, touchReportId,
    0x09, 0x22,        // Finger
    0xA1, 0x02,        // Logical collection
    0x09, 0x42,        // Tip Switch
    0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x01, 0x81, 0x02,
    0x75, 0x07, 0x95, 0x01, 0x81, 0x03,  // Padding
    0x09, 0x51,        // Contact Identifier
    0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02,
    0x05, 0x01,        // Generic Desktop
    0x09, 0x30, 0x09, 0x31,               // X, Y
    0x15, 0x00, 0x26, 0xFF, 0x7F,         // Logical range 0..32767
    0x75, 0x10, 0x95, 0x02, 0x81, 0x02,   // Two absolute 16-bit values
    0xC0,              // End Finger
    0x05, 0x0D, 0x09, 0x54,               // Contact Count
    0x15, 0x00, 0x25, 0x01,
    0x75, 0x08, 0x95, 0x01, 0x81, 0x02,
    0x91, 0x03,        // One reserved output byte for the SDK's output report
    0xC0               // End Touch Screen
};

struct Point {
    uint16_t x, y;
};

const uint16_t hidMax = 32767;
bool hidInitialized = false;
bool running = false;
bool touching = false;
// The stack reuses slot 0, including when the same bonded peer reconnects.
// The callback runs on the BLE task. Gesture state belongs to the sketch task.
std::atomic<uint32_t> linkEpoch{0};
uint32_t contactEpoch = 0;

void onDisconnect(uint8_t connId) {
    if (connId == 0) ++linkEpoch;
}

bool registerDisconnectObserver() {
    static T_FUN_CLIENT_CBS callbacks{};
    callbacks.disconnect_cb = onDisconnect;
    T_CLIENT_ID id;
    client_init(1);
    return client_register_spec_client_cb(&id, &callbacks);
}
uint16_t width = 0, height = 0, rotation = 0;
Point last = {0, 0};

uint16_t pixelCenter(int32_t coordinate, uint16_t extent) {
    return uint16_t(((2UL * coordinate + 1) * 16384UL) / extent);
}

bool mapPoint(int32_t x, int32_t y, BW16BLETouch::Units units, Point& out) {
    if (x < 0 || y < 0) return false;
    if (units == BW16BLETouch::Units::Pixels) {
        if (x >= width || y >= height) return false;
        x = pixelCenter(x, width);
        y = pixelCenter(y, height);
    } else if (units != BW16BLETouch::Units::Hid || x > hidMax || y > hidMax) {
        return false;
    }
    // Convert the clockwise-rotated screenshot back into the native HID frame.
    uint16_t u = uint16_t(x), v = uint16_t(y);
    switch (rotation) {
        case 90:  out = {v, uint16_t(hidMax - u)}; break;
        case 180: out = {uint16_t(hidMax - u), uint16_t(hidMax - v)}; break;
        case 270: out = {uint16_t(hidMax - v), u}; break;
        default:  out = {u, v};
    }
    return true;
}

bool sendPoint(Point point, bool down, uint32_t epoch) {
    if (!BW16BLETouch::isConnected() || linkEpoch.load() != epoch) return false;
    uint8_t report[] = {uint8_t(down), 0, uint8_t(point.x), uint8_t(point.x >> 8),
                        uint8_t(point.y), uint8_t(point.y >> 8), 1};
    BLEHIDDev.inputReport(touchReportId, report, sizeof(report));
    last = point;
    contactEpoch = epoch;
    touching = down;
    return linkEpoch.load() == epoch;
}

bool waitConnected(unsigned long start, uint16_t dueMs, uint32_t epoch) {
    while (millis() - start < dueMs) {
        if (!BW16BLETouch::isConnected() || linkEpoch.load() != epoch) return false;
        unsigned long elapsed = millis() - start;
        if (elapsed >= dueMs) break;
        unsigned long remaining = dueMs - elapsed;
        delay(remaining < 10 ? remaining : 10);
    }
    return BW16BLETouch::isConnected() && linkEpoch.load() == epoch;
}

bool tapPoint(Point point, uint16_t holdMs, uint32_t epoch) {
    return !touching && sendPoint(point, true, epoch) &&
           waitConnected(millis(), holdMs, epoch) && sendPoint(point, false, epoch);
}

bool moveLine(Point start, Point finish, uint16_t durationMs, uint16_t steps, uint32_t epoch) {
    const unsigned long started = millis();
    const int32_t dx = int32_t(finish.x) - start.x;
    const int32_t dy = int32_t(finish.y) - start.y;
    for (int32_t i = 1; i <= steps; ++i) {
        const uint16_t due = uint16_t((uint32_t(durationMs) * i) / steps);
        if (!waitConnected(started, due, epoch)) return false;
        Point next = {uint16_t(start.x + dx * i / steps), uint16_t(start.y + dy * i / steps)};
        if (!sendPoint(next, true, epoch)) return false;
    }
    return true;
}
} // namespace

bool BW16BLETouch::begin(const char* name, const char* manufacturer) {
    // 31-byte advertisement minus flags, appearance, HID UUID, and name header.
    // AmebaBLE's Device Information strings have 20-byte buffers.
    if (running || !name || !*name || strlen(name) > 18 ||
        !manufacturer || !*manufacturer || strlen(manufacturer) > 20) return false;
    if (!hidInitialized) {
        // AmebaD 3.1.7 retains these HID service objects across BLE.deinit().
        // Allocate them once, then register them below on every BLE start.
        BLEHIDDev.setNumInputReport(1);
        BLEHIDDev.setNumOutputReport(1); // SDK 3.1.7 allocates even with count zero.
        BLEHIDDev.init();
        BLEHIDDev.setReportMap(touchMap, sizeof(touchMap));
        hidInitialized = true;
    }
    BLEHIDDev.setModelString(name);
    BLEHIDDev.setManufacturerString(manufacturer);
    BLEAdvertData adv;
    adv.addFlags();
    adv.addCompleteName(name);
    adv.addAppearance(GAP_GATT_APPEARANCE_HUMAN_INTERFACE_DEVICE);
    adv.addCompleteServices(BLEUUID(HID_SERVICE_UUID));
    BLE.init();
    BLE.configAdvert()->setAdvData(adv);
    BLE.setDeviceName(name);
    BLE.setDeviceAppearance(GAP_GATT_APPEARANCE_HUMAN_INTERFACE_DEVICE);
    BLE.configSecurity()->setPairable(true);
    BLE.configSecurity()->setAuthFlags(GAP_AUTHEN_BIT_BONDING_FLAG);
    BLE.configServer(3);
    BLE.addService(BLEHIDDev.hidService());
    BLE.addService(BLEHIDDev.battService());
    BLE.addService(BLEHIDDev.devInfoService());
    BLE.beginPeripheral();
    // Realtek client_init() needs the running stack and its configured link count.
    if (!registerDisconnectObserver()) {
        BLE.deinit();
        return false;
    }
    running = true;
    return true;
}

bool BW16BLETouch::isConnected() {
    if (contactEpoch != linkEpoch.load()) touching = false;
    if (running && BLE.connected()) return true;
    touching = false;
    return false;
}

void BW16BLETouch::end() {
    if (!running) return;
    if (isConnected()) {
        if (touching) {
            release();
            delay(60);
        }
        if (BLE.configConnection()->disconnect()) {
            const unsigned long start = millis();
            while (BLE.connected() && millis() - start < 1000) delay(10);
        }
    }
    BLE.deinit(); // Includes BLE.end().
    touching = running = false;
}

bool BW16BLETouch::setScreenResolution(uint16_t w, uint16_t h) {
    if (!w || !h || w > 32768 || h > 32768 || (isConnected() && touching)) return false;
    width = w;
    height = h;
    return true;
}

bool BW16BLETouch::setRotation(uint16_t degrees) {
    if (degrees != 0 && degrees != 90 && degrees != 180 && degrees != 270) return false;
    if (isConnected() && touching) return false;
    rotation = degrees;
    return true;
}

bool BW16BLETouch::press(int32_t x, int32_t y, Units units) {
    const uint32_t epoch = linkEpoch.load();
    Point p;
    return isConnected() && !touching && mapPoint(x, y, units, p) && sendPoint(p, true, epoch);
}

bool BW16BLETouch::moveTo(int32_t x, int32_t y, Units units) {
    Point p;
    return isConnected() && touching && mapPoint(x, y, units, p) && sendPoint(p, true, contactEpoch);
}

bool BW16BLETouch::release() {
    return isConnected() && touching && sendPoint(last, false, contactEpoch);
}

bool BW16BLETouch::tap(int32_t x, int32_t y, uint16_t holdMs, Units units) {
    const uint32_t epoch = linkEpoch.load();
    Point p;
    return isConnected() && mapPoint(x, y, units, p) && tapPoint(p, holdMs, epoch);
}

bool BW16BLETouch::doubleTap(int32_t x, int32_t y, uint16_t intervalMs, Units units) {
    const uint32_t epoch = linkEpoch.load();
    Point p;
    return isConnected() && mapPoint(x, y, units, p) && tapPoint(p, 60, epoch) &&
           waitConnected(millis(), intervalMs, epoch) && tapPoint(p, 60, epoch);
}

bool BW16BLETouch::longPress(int32_t x, int32_t y, uint16_t holdMs, Units units) {
    return tap(x, y, holdMs, units);
}

bool BW16BLETouch::swipe(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                         uint16_t durationMs, uint16_t steps, Units units) {
    return longPressDrag(x1, y1, x2, y2, 0, durationMs, steps, units);
}

bool BW16BLETouch::longPressDrag(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                 uint16_t holdMs, uint16_t durationMs, uint16_t steps, Units units) {
    Point start, finish;
    const uint32_t epoch = linkEpoch.load();
    if (!steps || steps > durationMs || !isConnected() || touching ||
        !mapPoint(x1, y1, units, start) || !mapPoint(x2, y2, units, finish)) return false;
    if (!sendPoint(start, true, epoch)) return false;
    return waitConnected(millis(), holdMs, epoch) &&
           moveLine(start, finish, durationMs, steps, epoch) &&
           sendPoint(finish, false, epoch);
}

bool BW16BLETouch::swipePath(const PathPoint* points, size_t count,
                             uint16_t stepsPerSegment, Units units) {
    const uint32_t epoch = linkEpoch.load();
    if (!points || count < 2 || !stepsPerSegment ||
        !isConnected() || touching || points[0].travelMs != 0) return false;
    Point mapped;
    for (size_t i = 0; i < count; ++i) {
        if (!mapPoint(points[i].x, points[i].y, units, mapped) ||
            (i > 0 && points[i].travelMs < stepsPerSegment)) return false;
    }
    Point previous;
    if (!mapPoint(points[0].x, points[0].y, units, previous) ||
        !sendPoint(previous, true, epoch) ||
        !waitConnected(millis(), points[0].holdMs, epoch)) return false;
    for (size_t i = 1; i < count; ++i) {
        Point next;
        if (!mapPoint(points[i].x, points[i].y, units, next) ||
            !moveLine(previous, next, points[i].travelMs, stepsPerSegment, epoch) ||
            !waitConnected(millis(), points[i].holdMs, epoch)) return false;
        previous = next;
    }
    return sendPoint(previous, false, epoch);
}
