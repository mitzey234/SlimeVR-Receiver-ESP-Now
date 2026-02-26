#include "UnknownCommand.h"

#include "Serial.h"

void handleUnknownCommand() {
    Serial.println("[CMD] Unknown command. Available: factoryreset, setsecurity <16hex>, setchannel <num>, getchannel, pair, unpair <MAC>, unpairall, scanenv, startotaupdate <auth> <port> <ip> <ssid>\\t<pass>, reboot, temperature, scoff");
}