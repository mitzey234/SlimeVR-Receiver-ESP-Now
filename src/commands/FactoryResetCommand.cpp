#include "FactoryResetCommand.h"

#include <LittleFS.h>

bool handleFactoryResetCommand(const String &command) {
    if (!command.equalsIgnoreCase("factoryreset")) {
        return false;
    }

    Serial.println("[CMD] Factory reset: deleting pairedTrackers.bin, securityCode.bin, trackerIds.bin, wifiChannel.bin");
    LittleFS.remove("/pairedTrackers.bin");
    LittleFS.remove("/securityCode.bin");
    LittleFS.remove("/trackerIds.bin");
    LittleFS.remove("/wifiChannel.bin");
    Serial.println("[CMD] Factory reset complete");
    ESP.restart();
    return true;
}