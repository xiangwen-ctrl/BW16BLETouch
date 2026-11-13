#pragma once

#include <stdint.h>

extern unsigned long nowMs;
extern bool linkUp;
inline unsigned long millis() { return nowMs; }
void delay(unsigned long ms);

#define HID_SERVICE_UUID "1812"
const int GAP_GATT_APPEARANCE_HUMAN_INTERFACE_DEVICE = 0;
const int GAP_AUTHEN_BIT_BONDING_FLAG = 1;

struct BLEUUID {
    explicit BLEUUID(const char*) {}
};

struct BLEAdvertData {
    void addFlags() {}
    void addCompleteName(const char*) {}
    void addAppearance(int) {}
    void addCompleteServices(BLEUUID) {}
};

struct FakeBLE {
    FakeBLE* configConnection() { return this; }
    FakeBLE* configAdvert() { return this; }
    FakeBLE* configSecurity() { return this; }
    bool disconnect() {
        linkUp = false;
        return true;
    }
    bool connected() { return linkUp; }
    void init() {}
    void deinit() {}
    void setPairable(bool) {}
    void setAuthFlags(int) {}
    void configServer(int) {}
    void addService(int) {}
    void setDeviceAppearance(int) {}
    void setDeviceName(const char*) {}
    void setAdvData(BLEAdvertData) {}
    void beginPeripheral() {}
};
extern FakeBLE BLE;
