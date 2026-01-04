#include "ConsoleCommandHandler.h"
#include <LittleFS.h>
#include "configuration.h"
#include "espnow/espnow.h"

void ConsoleCommandHandler::update() {
    static String serialBuffer;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            serialBuffer.trim();
            if (serialBuffer.length() > 0) {
                if (serialBuffer.equalsIgnoreCase("factoryreset")) {
                    Serial.println("[CMD] Factory reset: deleting pairedTrackers.bin, securityCode.bin, trackerIds.bin");
                    LittleFS.remove("/pairedTrackers.bin");
                    LittleFS.remove("/securityCode.bin");
                    LittleFS.remove("/trackerIds.bin");
                    Serial.println("[CMD] Factory reset complete. Please reboot device.");
                } else if (serialBuffer.equalsIgnoreCase("pair")) {
                    bool pairing = !ESPNowCommunication::getInstance().isInPairingMode();
                    if (pairing) {
                        ESPNowCommunication::getInstance().enterPairingMode();
                        Serial.println("[CMD] Pairing mode enabled.");
                    } else {
                        ESPNowCommunication::getInstance().exitPairingMode();
                        Serial.println("[CMD] Pairing mode disabled.");
                    }
                } else if (serialBuffer.startsWith("setsecurity ")) {
                    String hexStr = serialBuffer.substring(12);
                    hexStr.trim();
                    auto hexCharToNibble = [](char c) -> int {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        return -1;
                    };
                    if (hexStr.length() == 16) {
                        uint8_t code[8];
                        bool valid = true;
                        for (int i = 0; i < 8; i++) {
                            int hi = hexCharToNibble(hexStr[2*i]);
                            int lo = hexCharToNibble(hexStr[2*i+1]);
                            if (hi < 0 || lo < 0) { valid = false; break; }
                            code[i] = (hi << 4) | lo;
                        }
                        if (valid) {
                            auto file = LittleFS.open("/securityCode.bin", "w", true);
                            file.write(code, 8);
                            file.close();
                            Serial.print("[CMD] Security code set to: ");
                            for (int i = 0; i < 8; i++) Serial.printf("%02x", code[i]);
                            Serial.println();
                            Configuration::getInstance().getSecurityCode(ESPNowCommunication::getInstance().securityCode); // Reload into ESPNowCommunication
                        } else {
                            Serial.println("[CMD] Invalid hex string. Use 16 hex digits.");
                        }
                    } else {
                        Serial.println("[CMD] Invalid hex string length. Use 16 hex digits.");
                    }
                } else if (serialBuffer.startsWith("setchannel ")) {
                    String chStr = serialBuffer.substring(10);
                    chStr.trim();
                    int ch = chStr.toInt();
                    if (ch >= 1 && ch <= 14) {
                        Configuration::getInstance().setWifiChannel((uint8_t)ch);
                        Serial.printf("[CMD] WiFi channel set to %d and saved.\n", ch);
                    } else {
                        Serial.println("[CMD] Invalid channel. Use 1-14.");
                    }
                } else if (serialBuffer.startsWith("unpair ")) {
                        String macStr = serialBuffer.substring(7);
                        macStr.trim();
                        uint8_t mac[6];
                        bool valid = true;
                        int macParts = 0;
                        int lastIdx = 0;
                        for (int i = 0; i < 6; i++) {
                            int nextIdx = macStr.indexOf(':', lastIdx);
                            String part = (nextIdx == -1) ? macStr.substring(lastIdx) : macStr.substring(lastIdx, nextIdx);
                            if (part.length() != 2) { valid = false; break; }
                            int hi = isxdigit(part[0]) ? ((part[0] >= '0' && part[0] <= '9') ? part[0] - '0' : (tolower(part[0]) - 'a' + 10)) : -1;
                            int lo = isxdigit(part[1]) ? ((part[1] >= '0' && part[1] <= '9') ? part[1] - '0' : (tolower(part[1]) - 'a' + 10)) : -1;
                            if (hi < 0 || lo < 0) { valid = false; break; }
                            mac[i] = (hi << 4) | lo;
                            lastIdx = nextIdx + 1;
                            macParts++;
                        }
                        if (valid && macParts == 6) {
                            // Remove from paired trackers
                            Configuration::getInstance().removePairedTracker(mac);
                            // Disconnect if connected
                            if (ESPNowCommunication::getInstance().disconnectSingleTracker(mac)) {
                                ESPNowCommunication::getInstance().sendUnpairToTracker(mac);
                                Serial.printf("[CMD] Tracker %02x:%02x:%02x:%02x:%02x:%02x disconnected and unpaired.\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                            } else {
                                Serial.printf("[CMD] Tracker %02x:%02x:%02x:%02x:%02x:%02x unpaired.\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                            }
                        } else {
                            Serial.println("[CMD] Invalid MAC address format. Use XX:XX:XX:XX:XX:XX");
                        }
                } else if (serialBuffer.equalsIgnoreCase("reboot") || serialBuffer.equalsIgnoreCase("restart")) {
                    Serial.println("[CMD] Rebooting device...");
                    delay(100);
                    ESP.restart();
                } else if (serialBuffer.equalsIgnoreCase("getchannel")) {
                    int ch = WiFi.channel();
                    Serial.printf("[CMD] Current WiFi channel: %d\n", ch);
                } else {
                    Serial.println("[CMD] Unknown command. Available: factoryreset, setsecurity <16hex>, setchannel <num>, getchannel, pair, reboot");
                }
            }
            serialBuffer = "";
        } else {
            serialBuffer += c;
        }
    }
}
