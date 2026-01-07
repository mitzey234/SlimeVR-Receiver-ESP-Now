#include "ConsoleCommandHandler.h"
#include "HID.h"
#include "button.h"
#include "configuration.h"
#include "error_codes.h"
#include "espnow/espnow.h"
#include "packetHandling.h"
#include "logging/Logger.h"
#include "GlobalVars.h"
#include <WiFi.h>

#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>


/*
* Todo:
* [X] Connect to the PC through HID
* [X] Enter pairing mode when long pressing button
* - [ ] In pairing mode listen for pair requests and send out mac address
* - [ ] Count the number of paired devices
* [ ] Listen for incoming data packets and store them in a circular buffer
* [ ] Send out data periodically to the PC
*/

HIDDevice hidDevice;
Button &button = Button::getInstance();
ESPNowCommunication &espnow = ESPNowCommunication::getInstance();
SlimeVR::Status::StatusManager statusManager;
SlimeVR::LEDManager ledManager;
SlimeVR::Logging::Logger logger("Main");
ConsoleCommandHandler consoleCommandHandler;

bool started = false;

void fail(ErrorCodes errorCode) {
    
}

bool isBootModeActive() {
    return digitalRead(BOOT_MODE_PIN) == BOOT_MODE_ACTIVE_LEVEL;
}

void debugPacket(const uint8_t packet[ESPNowCommunication::packetSizeBytes]) {
    Serial.print("New packet: ");
    for(int i = 0; i < ESPNowCommunication::packetSizeBytes; ++i) {
        Serial.printf("%02x ", packet[i]);
    }
    Serial.println();
}

void start () {
    if (!started) {
        started = true;
        hidDevice.begin();
        uint8_t mac[6];
            if (WiFi.getMode() == WIFI_MODE_NULL) {
                WiFi.mode(WIFI_STA);
                delay(100);
            }
            WiFi.macAddress(mac);

            // Format for USB_SERIAL: SVRDG + last 6 hex digits (e.g., SVRDGA1B2C3D4E5F6)
            char usbSerial[20] = "SVRDG";
            // Append full MAC address (12 hex digits) to serial string
            snprintf(usbSerial + 5, sizeof(usbSerial) - 5, "%02X%02X%02X%02X%02X%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.printf("USB Serial Number: %s\n", usbSerial);
        USB.serialNumber(usbSerial);
        USB.begin();
    }
}

void stop () {
    if (started) {
        Serial.println("Boot mode activated - resetting device...");
        delay(100); // Allow serial message to be sent
        ESP.restart();
    }
}

void setup() { 
    Serial.begin(115200);
    Serial.println("Starting up " USB_PRODUCT "...");

    // Initialize boot mode pin with pullup
    pinMode(BOOT_MODE_PIN, INPUT_PULLUP);
    
    // Check if boot mode is active (pin pulled down)
    if (isBootModeActive()) {
        Serial.println("Boot mode active - USB/HID initialization delayed");
    }
    
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
    if (!found) {
        Serial.println("No paired trackers found.");
    }

    // Only start USB/HID if boot mode is not active
    if (!isBootModeActive()) {
        start();
    }

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
    // Check boot mode status and start/stop accordingly
    if (isBootModeActive()) {
        // Boot mode active - stop USB/HID if running
        if (started) {
            stop();
        }
    } else {
        // Boot mode inactive - start USB/HID if not running
        if (!started) {
            Serial.println("Boot mode released - starting USB/HID");
            start();
        }
    }
    button.update();
    ledManager.update();
    espnow.update();


    // Non-blocking serial command handler
    consoleCommandHandler.update();

    PacketHandling::getInstance().tick(hidDevice);
}
