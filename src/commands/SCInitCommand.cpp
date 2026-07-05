#include "SCInitCommand.h"
#include "./serialCom/Ident.h"
#include "./serialCom/SerialCom.h"

bool handleSCInitCommand(const String &command) {
    if (!command.equalsIgnoreCase("SCInit")) {
        return false;
    }
    SlimeVR::SerialComMessages::Ident::print();
    SlimeVR::SerialCom::getInstance().enabled = true; // Enable SerialCom after sending ident message
    return true;
}