#include "BW16BLETouch.h"
#include "BLEHIDDevice.h"
#include "profile_client.h"
#include <cstdio>

FakeBLE BLE;
FakeHid BLEHIDDev;
const T_FUN_CLIENT_CBS* registeredClient = nullptr;
bool linkUp = false;
bool stackReady = false;
unsigned long nowMs = 0;
unsigned long reconnectAt = 0;
std::vector<std::array<uint8_t,7>> reports;

// The same peer returns on the same peripheral slot. No sketch API runs
// while the link is down, even when the transition occurs during delay().
void reconnect() {
    linkUp = false;
    if (registeredClient) registeredClient->disconnect_cb(0);
    linkUp = true;
}

void delay(unsigned long ms) {
    nowMs += ms;
    if (reconnectAt && nowMs >= reconnectAt) {
        reconnectAt = 0;
        reconnect();
    }
}

int main() {
    BW16BLETouch touch;
    assert(touch.begin());
    linkUp = true;
    assert(touch.press(100, 200));
    assert(!touch.press(300, 400)); // Still the same contact.
    reconnect();
    assert(!touch.moveTo(500, 600));
    assert(!touch.release());
    assert(reports.size() == 1);
    assert(touch.tap(300, 400));
    assert(reports.size() == 3 && reports[1][0] == 1 && reports[2][0] == 0);

    assert(touch.press(10, 20));
    reconnect();
    assert(!touch.release()); // Release is the first API this time.
    assert(touch.press(10, 20));
    reconnect();
    assert(touch.press(30, 40)); // New press is the first API after reconnection.
    assert(touch.moveTo(50, 60) && touch.release());

    reports.clear();
    reconnectAt = nowMs + 10;
    assert(!touch.swipe(10, 20, 50, 60, 100, 5));
    assert(reports.size() == 1); // No continuation reports on the new link.
    reconnectAt = nowMs + 70; // Between the two taps.
    assert(!touch.doubleTap(10, 20));
    assert(reports.size() == 3);
    assert(touch.tap(10, 20));

    touch.end();
    assert(touch.begin());
    linkUp = true;
    assert(touch.press(10, 20));
    reconnect();
    assert(!touch.release() && touch.tap(30, 40));
    std::puts("Reconnect regression passed");
}
