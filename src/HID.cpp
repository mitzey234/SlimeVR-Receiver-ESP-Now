#include "HID.h"

#include <cstring>
#include <Arduino.h>

HIDDevice::HIDDevice() {
    if (initialized) {
        return;
    }

    initialized = true;
    HID.addDevice(this, sizeof(hid_report_desc));
}

void HIDDevice::begin() {
    HID.begin();
}

uint16_t HIDDevice::_onGetDescriptor(uint8_t *buffer) {
    memcpy(buffer, hid_report_desc, sizeof(hid_report_desc));
    return sizeof(hid_report_desc);
}

bool HIDDevice::send(const uint8_t *value, size_t size) {
    // Only attempt send if HID is actually ready to prevent error spam
    if (!HID.ready()) {
        return false;
    }
    // Use shorter timeout to prevent blocking when USB is congested
    return HID.SendReport(0, value, size, 10);
}

bool HIDDevice::ready() {
    return HID.ready();
}

bool HIDDevice::initialized = false;
