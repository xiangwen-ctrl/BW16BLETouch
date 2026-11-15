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
std::vector<Report> reports;
std::vector<unsigned long> reportTimes;
using Touch = BW16BLETouch;
using Point = Touch::PathPoint;

void delay(unsigned long ms) {
    nowMs += ms;
    if (reconnectAt && nowMs >= reconnectAt) {
        reconnectAt = 0;
        linkUp = false;
        registeredClient->disconnect_cb(0);
        linkUp = true;
    }
}

void observed(const Report&) { reportTimes.push_back(nowMs); }
void clear() { reports.clear(); reportTimes.clear(); }
unsigned axis(const Report& r, size_t i) { return r[i] + (r[i + 1] << 8); }
void assertSingleContact(size_t expectedCount) {
    assert(reports.size() == expectedCount);
    for (size_t i = 0; i < reports.size(); ++i) {
        assert(reports[i][0] == (i + 1 < reports.size() ? 1 : 0));
        assert(reports[i][1] == 0 && reports[i][6] == 1);
    }
}

int main() {
    Touch touch;
    assert(touch.begin());
    linkUp = true;
    BLEHIDDev.observe = observed;
    Point path[] = {{100, 200, 0, 80}, {1000, 2000, 100, 60}, {20, 30, 200, 40}};
    assert(!touch.swipePath(nullptr, 3));
    assert(!touch.swipePath(path, 1));
    assert(!touch.swipePath(path, 3, 0));
    assert(!touch.swipePath(path, 3, 101));
    path[0].travelMs = 1;
    assert(!touch.swipePath(path, 3, 10));
    path[0].travelMs = 0;
    path[2].x = 32768;
    assert(!touch.swipePath(path, 3, 10) && reports.empty());
    path[2].x = 20;
    path[2].y = -1;
    assert(!touch.swipePath(path, 3, 10) && reports.empty());
    path[2].y = 30;
    assert(!touch.swipePathPixel(path, 3, 10));
    assert(!touch.longPressDrag(0, 0, 1, 1, 800, 0, 1));
    assert(!touch.longPressDrag(0, 0, 32768, 1));
    assert(reports.empty());

    unsigned long start = nowMs;
    assert(touch.swipePath(path, 3, 10));
    assertSingleContact(22);
    assert(nowMs - start == 480);
    assert(reportTimes[1] - start == 90);  // Start hold plus first step.
    assert(reportTimes[10] - start == 180);
    assert(reportTimes[11] - start == 260); // Intermediate hold plus next step.
    assert(reportTimes.back() - start == 480);
    assert(axis(reports[10], 2) == 1000 && axis(reports[20], 2) == 20);

    clear();
    start = nowMs;
    assert(touch.longPressDrag(100, 200, 1000, 2000, 80, 100, 10));
    assert(nowMs - start == 180);
    assertSingleContact(12);
    assert(reportTimes[1] - start == 90);
    auto expected = reports;
    clear();
    const Point equivalent[] = {{100, 200, 0, 80}, {1000, 2000, 100, 0}};
    assert(touch.swipePath(equivalent, 2, 10) && reports == expected);

    // The legacy swipe remains identical to a drag with a zero initial hold.
    clear();
    assert(touch.swipe(10, 20, 500, 600, 100, 10));
    expected = reports;
    clear();
    assert(touch.longPressDrag(10, 20, 500, 600, 0, 100, 10) && reports == expected);

    assert(touch.setScreenResolution(1080, 2400));
    assert(touch.setRotation(90));
    clear();
    assert(touch.swipePathPixel(path, 3, 10));
    expected = reports;
    clear();
    assert(touch.swipePath(path, 3, 10, Touch::Units::Pixels) && reports == expected);
    clear();
    assert(touch.longPressDragPixel(100, 200, 300, 400, 80, 100, 10));
    expected = reports;
    clear();
    assert(touch.longPressDrag(100, 200, 300, 400, 80, 100, 10, Touch::Units::Pixels));
    assert(reports == expected);

    // Drop and immediately reconnect during the initial hold, the movement,
    // intermediate hold, and final hold. No report may continue on the new link.
    for (unsigned long at : {10UL, 90UL, 210UL, 300UL, 450UL}) {
        clear();
        reconnectAt = nowMs + at;
        assert(!touch.swipePath(path, 3, 10));
        for (const Report& r : reports) assert(r[0] == 1);
        assert(!touch.release());
        assert(touch.tap(100, 200));
    }
    clear();
    reconnectAt = nowMs + 10;
    assert(!touch.longPressDrag(100, 200, 300, 400, 80, 100, 10));
    assert(reports.size() == 1 && touch.tap(0, 0));
    assert(touch.press(0, 0));
    clear();
    assert(!touch.swipePath(path, 3, 10) && !touch.longPressDrag(0, 0, 100, 200));
    assert(reports.empty() && touch.release());
    clear();
    const Point stationary[] = {{10, 20, 0, 0}, {10, 20, 1, 0}};
    assert(touch.swipePath(stationary, 2, 1));
    assertSingleContact(3);
    assert(reports[0] == reports[1]);
    std::puts("Path, timing, validation, pixel mapping and interruption tests passed");
}
