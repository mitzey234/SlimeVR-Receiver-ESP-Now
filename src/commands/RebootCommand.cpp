#include "RebootCommand.h"

bool handleRebootCommand(const String &command) {
    if (!command.equalsIgnoreCase("reboot") && !command.equalsIgnoreCase("restart")) {
        return false;
    }

    Serial.println("[CMD] Rebooting device...");
    delay(100);
    ESP.restart();
    return true;
}