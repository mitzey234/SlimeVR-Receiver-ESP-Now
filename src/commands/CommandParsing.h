#pragma once

#include <Arduino.h>
#include <ctype.h>

inline int commandHexCharToNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline bool parseHexString(const String &hexStr, uint8_t *out, int byteCount) {
    if (hexStr.length() != byteCount * 2) {
        return false;
    }

    for (int i = 0; i < byteCount; i++) {
        int hi = commandHexCharToNibble(hexStr[2 * i]);
        int lo = commandHexCharToNibble(hexStr[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (hi << 4) | lo;
    }

    return true;
}

inline bool parseMacAddress(const String &macStr, uint8_t mac[6]) {
    int lastIdx = 0;
    for (int i = 0; i < 6; i++) {
        int nextIdx = macStr.indexOf(':', lastIdx);
        String part = (nextIdx == -1) ? macStr.substring(lastIdx) : macStr.substring(lastIdx, nextIdx);
        if (part.length() != 2) {
            return false;
        }

        int hi = isxdigit(part[0]) ? ((part[0] >= '0' && part[0] <= '9') ? part[0] - '0' : (tolower(part[0]) - 'a' + 10)) : -1;
        int lo = isxdigit(part[1]) ? ((part[1] >= '0' && part[1] <= '9') ? part[1] - '0' : (tolower(part[1]) - 'a' + 10)) : -1;
        if (hi < 0 || lo < 0) {
            return false;
        }

        mac[i] = (hi << 4) | lo;
        lastIdx = nextIdx + 1;

        if (i < 5 && nextIdx == -1) {
            return false;
        }
    }

    return macStr.indexOf(':', lastIdx) == -1;
}

inline bool parseIpAddress(const String &ipStr, uint8_t ip[4]) {
    int lastIdx = 0;
    for (int i = 0; i < 4; i++) {
        int nextDot = ipStr.indexOf('.', lastIdx);
        String octet = (nextDot == -1 && i == 3) ? ipStr.substring(lastIdx) : ipStr.substring(lastIdx, nextDot);
        octet.trim();

        long octetVal = octet.toInt();
        if (octetVal < 0 || octetVal > 255) {
            return false;
        }

        ip[i] = (uint8_t)octetVal;
        lastIdx = nextDot + 1;

        if (i < 3 && nextDot == -1) {
            return false;
        }
    }

    return ipStr.indexOf('.', lastIdx) == -1;
}