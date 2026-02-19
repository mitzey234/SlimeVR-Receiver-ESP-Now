#include "ConsoleCommandHandler.h"
#include "HID.h"
#include "button.h"
#include "configuration.h"
#include "error_codes.h"
#include "espnow/espnow.h"
#include "packetHandling.h"
#include "GlobalVars.h"
#include "Serial.h"
#include "./serialCom/Ident.h"

#include "USB.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown"
#endif

HIDDevice hidDevice;
Button &button = Button::getInstance();
ESPNowCommunication &espnow = ESPNowCommunication::getInstance();
SlimeVR::Status::StatusManager statusManager;
SlimeVR::LEDManager ledManager;
ConsoleCommandHandler consoleCommandHandler;

void fail(ErrorCodes errorCode) {
    Serial.printf("Fatal error occurred: %d\n", static_cast<uint8_t>(errorCode));
    abort();
}

void setup() { 
    hidDevice.begin();
    Serial.printf("Starting up " USB_PRODUCT  "  - " FIRMWARE_VERSION "\n");

    statusManager.setStatus(SlimeVR::Status::LOADING, true);
    ledManager.setup();
    Configuration::getInstance().setup();

    // Print all paired trackers and their tracker IDs
    Serial.println("Paired trackers:");
    bool found = false;
    Configuration::getInstance().forEachPairedTracker([&found](const uint8_t mac[6], uint8_t trackerId) {
        Serial.printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x, TrackerID: %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], trackerId);
        found = true;
    });

    // If no paired trackers, print message
    // (forEachPairedTracker does nothing if none found)
    if (!found) Serial.println("No paired trackers found.");

    button.begin();

    button.onLongPress([]() {
        if (espnow.isScanningEnvironment()) {
            espnow.exitEnvironmentScanningMode();
            return;
        }
        espnow.UnpairAllTrackers();
    });
    
    button.onMultiPress([](size_t pressCount) {
        if (pressCount == 1) {
            if (!espnow.isInPairingMode()) {
                espnow.enterPairingMode();
            } else {
                espnow.exitPairingMode();
            }
        } else if (pressCount >= 2) {
            if (espnow.isScanningEnvironment()) return;
            Serial.println("Starting environment scanning");
            ESPNowCommunication::getInstance().enterEnvironmentScanningMode();
        }
    });

    ErrorCodes result = espnow.begin();
    if (result != ErrorCodes::NO_ERROR) {
        fail(result);
    }

    espnow.onTrackerPaired([&]() { 
        //espnow.exitPairingMode();
    });

    espnow.onTrackerConnected(
        [&](const uint8_t *trackerMacAddress) {
            uint8_t packet[16];
            packet[0] = 0xff;
            memcpy(&packet[2], trackerMacAddress, sizeof(uint8_t) * 6);
            memset(&packet[8], 0, sizeof(uint8_t) * 8);
            PacketHandling::getInstance().insert(packet, 16);
    });

    espnow.onTrackerDisconnected(
        [&](uint8_t trackerId) {
            //Serial.printf("Tracker %d disconnected, sending status packet\n", trackerId);
            PacketHandling::getInstance().sendDisconnectionStatus(trackerId);
    });

    Serial.println("Boot complete");
    statusManager.setStatus(SlimeVR::Status::LOADING, false);
    statusManager.setStatus(SlimeVR::Status::READY, true);
}

void loop() {
    button.update();
    ledManager.update();
    espnow.update();

    // Non-blocking serial command handler
    consoleCommandHandler.update();

    PacketHandling::getInstance().tick(hidDevice);
}
