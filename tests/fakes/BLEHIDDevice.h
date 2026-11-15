#pragma once

#include "BLEDevice.h"
#include <array>
#include <cassert>
#include <cstring>
#include <vector>

using Report = std::array<uint8_t, 7>;
extern std::vector<Report> reports;

struct FakeHid {
    void (*observe)(const Report&) = nullptr;
    void init() {}
    void setNumInputReport(int n) { assert(n == 1); }
    void setNumOutputReport(int n) { assert(n == 1); }
    void setReportMap(uint8_t*, size_t) {}
    void setModelString(const char*) {}
    void setManufacturerString(const char*) {}
    int hidService() { return 0; }
    int battService() { return 1; }
    int devInfoService() { return 2; }
    void inputReport(int id, uint8_t* bytes, size_t len) {
        Report report{};
        assert(id == 1 && len == report.size());
        std::memcpy(report.data(), bytes, len);
        reports.push_back(report);
        if (observe) observe(report);
    }
};
extern FakeHid BLEHIDDev;
