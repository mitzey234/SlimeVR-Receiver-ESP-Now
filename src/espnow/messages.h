#pragma once

#include <cstdint>

enum class ESPNowMessageTypes : uint8_t 
{
        PAIRING_REQUEST = 0,    // When the tracker is trying to pair with a gateway
        PAIRING_RESPONSE = 1,   // When the gateway is responding to a pairing request
        HANDSHAKE_REQUEST = 2,  // When the tracker is trying to handshake with a gateway
        HANDSHAKE_RESPONSE = 3, // When the gateway is responding to a handshake
        HEARTBEAT_ECHO = 4,     // Regular heartbeat message to keep the connection alive
        HEARTBEAT_RESPONSE = 5, // Response to the heartbeat message
        TRACKER_DATA = 6,        // Regular tracker data packet
        PAIRING_ANNOUNCEMENT = 7, // When the gateway is announcing its presence for pairing
        UNPAIR = 8,              // When the gateway is unpairing a tracker
        TRACKER_RATE = 9         // When the gateway is setting the polling rate for trackers
};

struct __attribute__((packed)) ESPNowPairingAnnouncementMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::PAIRING_ANNOUNCEMENT;
    uint8_t channel;
    uint8_t securityBytes[8];
};

struct __attribute__((packed)) ESPNowPairingMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::PAIRING_REQUEST;
    uint8_t securityBytes[8];
};

struct __attribute__((packed)) ESPNowPairingAckMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::PAIRING_RESPONSE;
};

struct __attribute__((packed)) ESPNowConnectionMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::HANDSHAKE_REQUEST;
    uint8_t securityBytes[8];
};

struct __attribute__((packed)) ESPNowConnectionAckMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::HANDSHAKE_RESPONSE;
    uint8_t channel;
    uint8_t trackerId;
};

struct __attribute__((packed)) ESPNowPacketMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::TRACKER_DATA;
    uint8_t len;
    uint8_t data[240]; //Probably correct size
};

struct __attribute__((packed)) ESPNowHeartbeatEchoMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::HEARTBEAT_ECHO;
    uint16_t sequenceNumber;
};

struct __attribute__((packed)) ESPNowHeartbeatResponseMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::HEARTBEAT_RESPONSE;
    uint16_t sequenceNumber;
};

struct __attribute__((packed)) ESPNowUnpairMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::UNPAIR;
    uint8_t securityBytes[8];
};

struct __attribute__((packed)) ESPNowTrackerRateMessage {
    ESPNowMessageTypes header = ESPNowMessageTypes::TRACKER_RATE;
    uint32_t pollRateHz;  // Polling rate in Hz (updates per second)
};

struct __attribute__((packed)) ESPNowMessageBase {
    ESPNowMessageTypes header;
};

union ESPNowMessage {
    ESPNowMessageBase base;
    ESPNowPairingMessage pairing;
    ESPNowPairingAckMessage pairingAck;
    ESPNowConnectionMessage connection;
    ESPNowPacketMessage packet;
    ESPNowPairingAnnouncementMessage pairingAnnouncement;
    ESPNowConnectionAckMessage connectionAck;
    ESPNowHeartbeatEchoMessage heartbeatEcho;
    ESPNowHeartbeatResponseMessage heartbeatResponse;
    ESPNowTrackerRateMessage trackerRate;
};
