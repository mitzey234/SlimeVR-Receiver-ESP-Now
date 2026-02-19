#include "SetChannelCommand.h"

#include "../configuration.h"

bool handleSetChannelCommand(const String &command) {
    if (!command.startsWith("setchannel ")) {
        return false;
    }

    String chStr = command.substring(10);
    chStr.trim();
    int ch = chStr.toInt();

    if (ch >= 1 && ch <= 14) {
        Configuration::getInstance().setWifiChannel((uint8_t)ch);
        Serial.printf("[CMD] WiFi channel set to %d and saved.\n", ch);
    } else {
        Serial.println("[CMD] Invalid channel. Use 1-14.");
    }

    return true;
}