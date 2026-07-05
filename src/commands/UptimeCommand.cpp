#include "UptimeCommand.h"

#include "GlobalVars.h"
#include "Serial.h"

bool handleUptimeCommand(const String &command) {
    if (!command.equalsIgnoreCase("uptime")) {
        return false;
    }

    const uint32_t uptimeSeconds = millis() / 1000UL;

    const uint32_t days = uptimeSeconds / 86400UL;
    const uint32_t hours = (uptimeSeconds % 86400UL) / 3600UL;
    const uint32_t minutes = (uptimeSeconds % 3600UL) / 60UL;
    const uint32_t seconds = uptimeSeconds % 60UL;

    Serial.printf("[CMD] Uptime: %02lu:%02lu:%02lu:%02lu\n", days, hours, minutes, seconds);
    return true;
}
