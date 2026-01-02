#pragma once

#include "error_codes.h"
#include "espnow/messages.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstdint>
#include <esp_now.h>
#include <functional>
#include <vector>

class ESPNowCommunication {
    public:
        static constexpr size_t packetSizeBytes = 240;

        static unsigned int channel;

        const static unsigned int maxPPS = 1500; // Maximum packets per second total across all trackers

        static ESPNowCommunication &getInstance();

        ErrorCodes begin();

        void enterPairingMode();
        void exitPairingMode();
        bool isInPairingMode() const { return pairing; }
        
        void disconnectAllTrackers();
        bool disconnectSingleTracker(const uint8_t mac[6]);
        void sendUnpairToAllTrackers();
        void sendUnpairToTracker(const uint8_t mac[6]);
        inline bool isTrackerIdConnected(uint8_t trackerId) const;

        void update();

        void onTrackerPaired(std::function<void()> callback);
        void onTrackerConnected(std::function<void(const uint8_t *)> callback);
        void onTrackerDisconnected(std::function<void(uint8_t)> callback);  // Passes tracker ID
        
        size_t getConnectedTrackerCount() const;
        bool getTrackerMacByIndex(size_t index, uint8_t mac[6]) const;
        uint8_t securityCode[8];

    private:
        static ESPNowCommunication instance;
        ESPNowCommunication() = default;

        esp_now_rate_config_t rate_config;

        void invokeTrackerPairedEvent();
        void invokeTrackerConnectedEvent(const uint8_t *trackerMacAddress);
        void invokeTrackerDisconnectedEvent(uint8_t trackerId);
        void sendRateUpdateToAllTrackers();

        static void onReceive(const esp_now_recv_info_t *senderInfo, const uint8_t *data, int dataLen);
        void __attribute__((hot)) __attribute__((flatten)) handleMessage(const esp_now_recv_info_t *senderInfo, const uint8_t *data, int dataLen);

        // Heartbeat tracking structure
        struct Tracker {
            std::array<uint8_t, 6> mac;
            uint8_t trackerId;
            unsigned long lastPingSent = 0;
            unsigned long pingStartTime = 0;
            bool waitingForResponse = false;
            uint8_t missedPings = 0;
            uint8_t latency = 0;
            uint16_t expectedSequenceNumber = 0;
            int8_t rssi = 0;  // Signal strength in dBm
        };

        uint8_t addPeer(const uint8_t peerMac[6]);
        bool deletePeer(const uint8_t peerMac[6]);
        inline bool isTrackerConnected(const uint8_t peerMac[6]);
        inline Tracker* getTracker(const uint8_t peerMac[6]);

        bool pairing = false;

        unsigned int recievedPacketCount = 0;
        unsigned int recievedByteCount = 0;
        unsigned long lastStatsReport = 0;
        
        // Store connected tracker MAC addresses with heartbeat tracking
        std::vector<Tracker> connectedTrackers;
        
        static constexpr unsigned long heartbeatInterval = 1000; // 1 second
        static constexpr unsigned long heartbeatTimeout = 1000; // 1 second timeout
        static constexpr uint8_t maxMissedPings = 5;

        std::vector<std::function<void()>> trackerPairedCallbacks;
        std::vector<std::function<void(const uint8_t *)>> trackerConnectedCallbacks;
        std::vector<std::function<void(uint8_t)>> trackerDisconnectedCallbacks;

        static constexpr uint8_t broadcastAddress[6]{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        static constexpr uint8_t espnowWifiChannel = 6;

        unsigned long lastPairingBroadcast = 0;
        unsigned long lastHeartbeatCheck = 0;
        static constexpr unsigned long pairingBroadcastInterval = 500;

        std::string espNowErrorToString(esp_err_t error);
};
