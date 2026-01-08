#include "ConsoleCommandHandler.h"
#include "HID.h"
#include "button.h"
#include "configuration.h"
#include "error_codes.h"
#include "espnow/espnow.h"
#include "packetHandling.h"
#include "logging/Logger.h"
#include "GlobalVars.h"
#include "Serial.h"

#include "USB.h"

HIDDevice hidDevice;
Button &button = Button::getInstance();
ESPNowCommunication &espnow = ESPNowCommunication::getInstance();
SlimeVR::Status::StatusManager statusManager;
SlimeVR::LEDManager ledManager;
SlimeVR::Logging::Logger logger("Main");
ConsoleCommandHandler consoleCommandHandler;

void fail(ErrorCodes errorCode) {
    Serial.printf("Fatal error occurred: %d\n", static_cast<uint8_t>(errorCode));
    abort();
}

void setup() { 
    hidDevice.begin();
    Serial.println("Starting up " USB_PRODUCT "...");

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
        Serial.println("Trackers reset");
        statusManager.setStatus(SlimeVR::Status::RESETTING, true);
        Configuration::getInstance().clearAllPairedTrackers();
        espnow.sendUnpairToAllTrackers();
        espnow.disconnectAllTrackers();
        Configuration::getInstance().resetSecurityCode();

        // Blink LED twice to indicate reset action
        ledManager.pattern(500, 300, 2);
        
        // Clear resetting status after completion
        statusManager.setStatus(SlimeVR::Status::RESETTING, false);
        return;
    });
    
    button.onMultiPress([](size_t pressCount) {
        if (pressCount == 1) {
            if (!espnow.isInPairingMode()) {
                Serial.println("Pairing mode enabled");
                espnow.enterPairingMode();
            } else {
                Serial.println("Pairing mode disabled");
                espnow.exitPairingMode();
            }
        }
    });

    ErrorCodes result = espnow.begin();
    if (result != ErrorCodes::NO_ERROR) {
        fail(result);
    }

    espnow.onTrackerPaired([&]() { 
        espnow.exitPairingMode();
        statusManager.setStatus(SlimeVR::Status::PAIRING_MODE, false);
    });

    espnow.onTrackerConnected(
        [&](const uint8_t *trackerMacAddress) {
            Serial.println("New tracker connected");
            uint8_t packet[16];
            packet[0] = 0xff;
            memcpy(&packet[2], trackerMacAddress, sizeof(uint8_t) * 6);
            memset(&packet[8], 0, sizeof(uint8_t) * 8);
            PacketHandling::getInstance().insert(packet, 16);
    });

    espnow.onTrackerDisconnected(
        [&](uint8_t trackerId) {
            Serial.printf("Tracker %d disconnected, sending status packet\n", trackerId);
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
