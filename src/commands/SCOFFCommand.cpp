#include "SCOFFCommand.h"
#include "./serialCom/SerialCom.h"

bool handleSCOFFCommand(const String &command) {
    if (!command.equalsIgnoreCase("scoff")) {
        return false;
    }

    SlimeVR::SerialCom::getInstance().enabled = false;
    Serial.println("[CMD] SerialCom disabled");
    return true;
}
