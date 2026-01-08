#include "configuration.h"
#include "espnow.h"

#include "configuration.h"
#include "espnow/messages.h"
#include "packetHandling.h"
#include <esp_wifi.h>
#include <string>
#include "../GlobalVars.h"

// Ensure StatusManager type is defined before extern declaration
// Use the global StatusManager instance defined in main.cpp
extern SlimeVR::Status::StatusManager statusManager;


#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2ARGS(mac) mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]


// Static member definition
unsigned int ESPNowCommunication::channel = 6;

ESPNowCommunication ESPNowCommunication::instance;

// Gets the singleton instance of ESPNowCommunication
ESPNowCommunication &ESPNowCommunication::getInstance() {
    return instance;
}

// Adds a callback method for when a tracker is paired
void ESPNowCommunication::onTrackerPaired(std::function<void()> callback) {
    trackerPairedCallbacks.push_back(callback);
}

// Adds a callback method for when a tracker is connected
void ESPNowCommunication::onTrackerConnected(std::function<void(const uint8_t *)> callback) {
    trackerConnectedCallbacks.push_back(std::move(callback));
}

// Adds a callback method for when a tracker is disconnected
void ESPNowCommunication::onTrackerDisconnected(std::function<void(uint8_t)> callback) {
    trackerDisconnectedCallbacks.push_back(std::move(callback));
}

// Invokes all registered tracker paired event callbacks
void ESPNowCommunication::invokeTrackerPairedEvent() {
    for (auto &callback : trackerPairedCallbacks) callback();
}

// Invokes all registered tracker connected event callbacks
void ESPNowCommunication::invokeTrackerConnectedEvent(const uint8_t *trackerMacAddress) {
    for (auto &callback : trackerConnectedCallbacks) callback(trackerMacAddress);
}

// Invokes all registered tracker disconnected event callbacks
void ESPNowCommunication::invokeTrackerDisconnectedEvent(uint8_t trackerId) {
    for (auto &callback : trackerDisconnectedCallbacks) callback(trackerId);
}

// Gets the number of currently connected trackers
size_t ESPNowCommunication::getConnectedTrackerCount() const {
    return connectedTrackers.size();
}

// Gets the MAC address of a connected tracker by its index
bool ESPNowCommunication::getTrackerMacByIndex(size_t index, uint8_t mac[6]) const {
    if (index >= connectedTrackers.size()) return false;
    memcpy(mac, connectedTrackers[index].mac.data(), 6);
    return true;
}

// Gets the MAC address of a connected tracker by its index
uint8_t* ESPNowCommunication::getTrackerIdByIndex(size_t index) {
    return &connectedTrackers[index].trackerId;
}

// Gets the tracker structure for a given MAC address
ESPNowCommunication::Tracker *ESPNowCommunication::getTracker(const uint8_t peerMac[6]) {
    // Fast MAC comparison using integer comparisons instead of memcmp
    for (auto &tracker : connectedTrackers) {
        if (*reinterpret_cast<const uint32_t *>(tracker.mac.data()) == *reinterpret_cast<const uint32_t *>(peerMac) && *reinterpret_cast<const uint16_t *>(tracker.mac.data() + 4) == *reinterpret_cast<const uint16_t *>(peerMac + 4)) {
            return &tracker;
        }
    }
    return nullptr;
}

// Checks if a tracker with the given MAC address is currently connected
bool ESPNowCommunication::isTrackerConnected(const uint8_t peerMac[6]) {
    // Fast MAC comparison using integer comparisons instead of memcmp
    for (const auto &tracker : connectedTrackers) {
        if (*reinterpret_cast<const uint32_t *>(tracker.mac.data()) == *reinterpret_cast<const uint32_t *>(peerMac) && *reinterpret_cast<const uint16_t *>(tracker.mac.data() + 4) == *reinterpret_cast<const uint16_t *>(peerMac + 4) && esp_now_is_peer_exist(peerMac)) return true;
    }
    return false;
}

// Checks if a tracker ID is currently connected
bool ESPNowCommunication::isTrackerIdConnected(uint8_t trackerId) const {
    for (const auto &tracker : connectedTrackers) if (tracker.trackerId == trackerId) return true;
    return false;
}

// Enters pairing mode
void ESPNowCommunication::enterPairingMode() {
    pairing = true;
    statusManager.setStatus(SlimeVR::Status::PAIRING_MODE, true);
}

// Exits pairing mode
void ESPNowCommunication::exitPairingMode() {
    pairing = false;
    statusManager.setStatus(SlimeVR::Status::PAIRING_MODE, false);
}

// Disconnect a single tracker by MAC
bool ESPNowCommunication::disconnectSingleTracker(const uint8_t mac[6]) {
    for (auto it = connectedTrackers.begin(); it != connectedTrackers.end(); ++it) {
        if (memcmp(it->mac.data(), mac, 6) == 0) {
            uint8_t trackerId = it->trackerId;
            deletePeer(mac);
            connectedTrackers.erase(it);
            Serial.printf("[ESPNOW] Disconnected tracker %02x:%02x:%02x:%02x:%02x:%02x (ID: %d)\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], trackerId);
            invokeTrackerDisconnectedEvent(trackerId);
            sendRateUpdateNextTick = true;
            return true;
        }
    }
    return false;
}

// Disconnect all trackers
void ESPNowCommunication::disconnectAllTrackers() {
    for (const auto &tracker : connectedTrackers) {
        deletePeer(tracker.mac.data());
        invokeTrackerDisconnectedEvent(tracker.trackerId);
    }

    connectedTrackers.clear();
    Serial.println("All trackers disconnected");
}

// Queue a message for sending with rate limiting
void ESPNowCommunication::queueMessageMutex(const uint8_t peerMac[6], const uint8_t *data, size_t dataLen, Tracker* tracker, bool ephemeral) {
    // Validate message data
    // Serial.printf("Queueing message to " MACSTR " of size %zu\n", MAC2ARGS(peerMac), dataLen);
    if (dataLen == 0 || dataLen > ESP_NOW_MAX_DATA_LEN) {
        Serial.printf("Invalid message size %zu for " MACSTR ", skipping\n", dataLen, MAC2ARGS(peerMac));
        return;
    }

    // Check if queue is full
    size_t nextTail = (queueTail + 1) % maxQueueSize;
    if (nextTail == queueHead) {
        // Calculate queue depth for diagnostic output
        size_t queueDepth = (queueTail >= queueHead) ? (queueTail - queueHead) : (maxQueueSize - queueHead + queueTail);
        Serial.printf("Send queue full! Dropping message to " MACSTR " (queue: %zu/%zu, depth: %zu)\n", MAC2ARGS(peerMac), maxQueueSize, maxQueueSize, queueDepth);
        return;
    }
    
    // Add message to queue
    PendingMessage &msg = sendQueue[queueTail];
    memcpy(msg.peerMac, peerMac, 6);
    memcpy(msg.data, data, dataLen);
    msg.dataLen = dataLen;
    msg.tracker = tracker;
    msg.ephemeral = ephemeral;
    msg.skip = false;
    queueTail = nextTail;
}

// Queue a message for sending with rate limiting
void ESPNowCommunication::queueMessage(const uint8_t peerMac[6], const uint8_t *data, size_t dataLen, Tracker* tracker, bool ephemeral) {
    queueMessageMutex(peerMac, data, dataLen, tracker, ephemeral);
    processSendQueue();
}

void ESPNowCommunication::queueMessage(const uint8_t peerMac[6], const uint8_t *data, size_t dataLen, Tracker* tracker) {
    queueMessage(peerMac, data, dataLen, tracker, false);
}

// Overloaded method to queue a message without tracker pointer
void ESPNowCommunication::queueMessage(const uint8_t peerMac[6], const uint8_t *data, size_t dataLen) {
    queueMessage(peerMac, data, dataLen, nullptr, false);
}

// Process queued messages with rate limiting
void ESPNowCommunication::processSendQueue() {
    MutexLock lock(queueMutex);
    if (queueHead == queueTail) return;

    // Serial.printf("Queue in processSendQueue: head=%zu, tail=%zu\n", queueHead, queueTail);

    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= sendRateLimit) {
        PendingMessage &msg = sendQueue[queueHead];

        if (msg.skip) {
            queueHead = (queueHead + 1) % maxQueueSize;
            return;
        }

        // Validate message data
        if (msg.dataLen == 0 || msg.dataLen > ESP_NOW_MAX_DATA_LEN) {
            Serial.printf("Invalid message size %zu for " MACSTR ", dropping\n", msg.dataLen, MAC2ARGS(msg.peerMac));
            queueHead = (queueHead + 1) % maxQueueSize;
            lastSendTime = currentTime;
            return;
        }

        // Ensure peer is added before sending
        if (!esp_now_is_peer_exist(msg.peerMac)) {
            Serial.printf("Peer " MACSTR " not found, adding before sending queued message\n", MAC2ARGS(msg.peerMac));
            auto addResult = addPeer(msg.peerMac);
            if (addResult != ESP_OK) {
                Serial.printf("Failed to add peer " MACSTR " for queued message, error: %s (%d)\n", MAC2ARGS(msg.peerMac), espNowErrorToString(addResult).c_str(), addResult);
                queueHead = (queueHead + 1) % maxQueueSize;
                lastSendTime = currentTime;
                return;
            }
        }
        
        //Serial.printf("Sending message to " MACSTR ", size %zu\n", MAC2ARGS(msg.peerMac), msg.dataLen);
        auto result = esp_now_send(msg.peerMac, msg.data, msg.dataLen);
        
        if (msg.ephemeral) {
            // Remove peer if message was ephemeral
            deletePeer(msg.peerMac);
        }

        if (msg.tracker != nullptr) {
            // Update ping info if this message is associated with a tracker
            msg.tracker->lastPingSent = currentTime;
            msg.tracker->pingStartTime = currentTime;
        }

        if (result == ESP_OK) {
            // Message sent successfully, remove from queue
            queueHead = (queueHead + 1) % maxQueueSize;
            lastSendTime = currentTime;
        } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
            // ESP-NOW internal buffer is full - retry this message later without advancing queue
            // Don't update lastSendTime to allow immediate retry on next processSendQueue call
            Serial.printf("ESP-NOW buffer full, retrying message to " MACSTR ", error: %s (%d)\n", MAC2ARGS(msg.peerMac), espNowErrorToString(result).c_str(), result);
        } else {
            // Other errors - log and drop the message
            Serial.printf("Failed to send queued message to " MACSTR ", error: %s (%d)\n", MAC2ARGS(msg.peerMac), espNowErrorToString(result).c_str(), result);
            queueHead = (queueHead + 1) % maxQueueSize;
            lastSendTime = currentTime;
        }
    }
}

// Sends an unpair message to a specific tracker
void ESPNowCommunication::sendUnpairToTracker(const uint8_t mac[6]) {
    ESPNowUnpairMessage unpairMsg;
    memcpy(unpairMsg.securityBytes, securityCode, 8);
	queueMessage(mac, reinterpret_cast<const uint8_t *>(&unpairMsg), sizeof(ESPNowUnpairMessage));
	queueMessage(mac, reinterpret_cast<const uint8_t *>(&unpairMsg), sizeof(ESPNowUnpairMessage));
	queueMessage(mac, reinterpret_cast<const uint8_t *>(&unpairMsg), sizeof(ESPNowUnpairMessage), nullptr, true);
	// Serial.printf("Queued unpair to tracker " MACSTR "\n", MAC2ARGS(mac));
}

// Sends unpair messages to all connected trackers
void ESPNowCommunication::sendUnpairToAllTrackers() {
    ESPNowUnpairMessage unpairMsg;
    memcpy(unpairMsg.securityBytes, securityCode, 8);

    queueMessage(broadcastAddress, reinterpret_cast<const uint8_t *>(&unpairMsg), sizeof(ESPNowUnpairMessage));
    queueMessage(broadcastAddress, reinterpret_cast<const uint8_t *>(&unpairMsg), sizeof(ESPNowUnpairMessage));
    queueMessage(broadcastAddress, reinterpret_cast<const uint8_t *>(&unpairMsg), sizeof(ESPNowUnpairMessage));

    // Serial.println("Unpair messages queued to all trackers");
}

// Sends rate update messages to all connected trackers
void ESPNowCommunication::sendRateUpdateToAllTrackers() {
    size_t trackerCount = connectedTrackers.size();
    if (trackerCount == 0) return; // No trackers to update

    // Calculate polling rate per tracker in Hz: divide maxPPS by number of trackers
    uint32_t pollRateHz = maxPPS / trackerCount;

    Serial.printf("Updating tracker rate: %u trackers, %u Hz per tracker\n", trackerCount, pollRateHz);

    // Queue rate update to all connected trackers
    ESPNowTrackerRateMessage rateMsg;
    rateMsg.pollRateHz = pollRateHz;

    // Send rate update as a broadcast instead of per-tracker
    queueMessage(broadcastAddress, reinterpret_cast<const uint8_t *>(&rateMsg), sizeof(ESPNowTrackerRateMessage));
}

// Initializes ESPNOW communication
ErrorCodes ESPNowCommunication::begin() {
    // Initialize mutex for queue protection
    if (!queueMutex) {
        queueMutex = xSemaphoreCreateMutex();
        if (!queueMutex) {
            Serial.println("[ESPNOW] Failed to create queue mutex!");
            return ErrorCodes::ESP_NOW_INIT_FAILED;
        }
    }
    channel = Configuration::getInstance().getWifiChannel();

    // Pre-allocate vectors to avoid reallocations during operation
    connectedTrackers.reserve(16); // Reserve space for up to 16 trackers
    trackerPairedCallbacks.reserve(4);
    trackerConnectedCallbacks.reserve(4);
    trackerDisconnectedCallbacks.reserve(4);

    // Load or generate security code
    Configuration::getInstance().getSecurityCode(securityCode);

    WiFi.mode(WIFI_STA);
    WiFi.setChannel(channel);
    WiFi.setTxPower(WIFI_POWER_17dBm);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11G);
    esp_wifi_set_ps(WIFI_PS_NONE);

    rate_config.phymode = WIFI_PHY_MODE_HT20;
    rate_config.rate = WIFI_PHY_RATE_MCS7_SGI;
    rate_config.ersu = false;

    auto result = esp_now_init();
    if (result != ESP_OK) {
        Serial.printf("Couldn't initialize ESPNOW! - %s\n", espNowErrorToString(result).c_str());
        return ErrorCodes::ESP_NOW_INIT_FAILED;
    }

    result = addPeer(broadcastAddress, true);
    if (result != ESP_OK)
    {
        Serial.printf("Couldn't add broadcast peer! - %s\n", espNowErrorToString(result).c_str());
        return ErrorCodes::ESP_NOW_ADDING_BROADCAST_FAILED;
    }

    result = esp_now_register_recv_cb(onReceive);
    if (result != ESP_OK)
    {
        Serial.printf("Couldn't register message callback! - %s\n", espNowErrorToString(result).c_str());
        return ErrorCodes::ESP_RECV_CALLACK_REGISTERING_FAILED;
    }

    uint8_t macaddr[6];
    WiFi.macAddress(macaddr);

    Serial.printf("[ESPNOW] address: %02x:%02x:%02x:%02x:%02x:%02x Channel: %d\n", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5], WiFi.channel());
    return ErrorCodes::NO_ERROR;
}

// ESPNOW receive callback
void ESPNowCommunication::onReceive(const esp_now_recv_info_t *senderInfo, const uint8_t *data, int dataLen) {
    ESPNowCommunication::getInstance().handleMessage(senderInfo, data, dataLen);
}

// Handles incoming ESPNOW messages
void ESPNowCommunication::handleMessage(const esp_now_recv_info_t *senderInfo, const uint8_t *data, int dataLen) {
    // Fast path: cast message once and read header
    //Serial.printf("[ESPNOW] Received message of length %d from " MACSTR "\n", dataLen, MAC2ARGS(senderInfo->src_addr));
    const ESPNowMessage *message = reinterpret_cast<const ESPNowMessage *>(data);
    const ESPNowMessageTypes header = message->base.header;

    // Optimize the most common case - TRACKER_DATA (hot path)
    if (header == ESPNowMessageTypes::TRACKER_DATA) {
        // Fast validation: check if tracker is connected (most packets come from connected trackers)
        const uint8_t *mac = senderInfo->src_addr;
        Tracker* tracker = getTracker(mac);
        if (tracker == nullptr) return; // Tracker not connected - ignore packet

        // Tracker found and connected - process packet
        recievedPacketCount++;
        recievedByteCount += message->packet.len;

        // Update RSSI for this tracker
        tracker->rssi = senderInfo->rx_ctrl->rssi;

        // Forward packet to PacketHandling with RSSI
        PacketHandling::getInstance().insert(message->packet.data, message->packet.len, senderInfo->rx_ctrl->rssi);
        return;
    }

    // Handle less frequent message types
    switch (header) {
    case ESPNowMessageTypes::PAIRING_REQUEST: {
        const ESPNowPairingMessage &request = message->pairing;
        if (memcmp(request.securityBytes, securityCode, 8) != 0) return; // Invalid security code

        // Step 1: Check if tracker is already paired
        if (!Configuration::getInstance().isPairedTracker(senderInfo->src_addr)) {
            if (!pairing) return; // Ignore pairing requests if not in pairing mode
            Configuration::getInstance().addPairedTracker(senderInfo->src_addr);
            // Allocate persistent tracker ID for this MAC address
            uint8_t trackerId = Configuration::getInstance().getTrackerIdForMac(senderInfo->src_addr);
            Serial.printf("Paired a new tracker at mac address " MACSTR " with ID %d!\n", MAC2ARGS(senderInfo->src_addr), trackerId);
        } else {
            Serial.printf("Tracker at mac address " MACSTR " is already paired!\n", MAC2ARGS(senderInfo->src_addr));
        }

        // Step 2: Send acknowledgment
        ESPNowPairingAckMessage ackMessage;
        // Serial.printf("Sending pairing acknowledgment to " MACSTR "\n", MAC2ARGS(senderInfo->src_addr));
        queueMessage(senderInfo->src_addr, reinterpret_cast<uint8_t *>(&ackMessage), sizeof(ackMessage), nullptr, true);

        // Step 3: Invoke paired event
        invokeTrackerPairedEvent();
        break;
    }
    case ESPNowMessageTypes::PAIRING_RESPONSE:
        return;
    case ESPNowMessageTypes::HANDSHAKE_REQUEST: {
        const ESPNowConnectionMessage &handshake = message->connection;
        // Validate security code
        if (memcmp(handshake.securityBytes, securityCode, 8) != 0) {
            Serial.printf("Received handshake from " MACSTR " with invalid security code! Sent: ", MAC2ARGS(senderInfo->src_addr));
            for (int i = 0; i < 8; ++i) Serial.printf("%02x", handshake.securityBytes[i]);
            Serial.println();
            return;
        }

        // Check that the tracker MAC is in persistent memory
        if (!Configuration::getInstance().isPairedTracker(senderInfo->src_addr)) {
            Serial.printf("Received handshake from unpaired tracker " MACSTR " - ignoring!\n", MAC2ARGS(senderInfo->src_addr));
            return;
        }

        Tracker* tracker = getTracker(senderInfo->src_addr);
        // Check to make sure the tracker isn't already connected
        if (tracker != nullptr) {
            Serial.printf("Tracker at mac address " MACSTR " is already connected!\n", MAC2ARGS(senderInfo->src_addr));

            ESPNowConnectionAckMessage handshakeResponse;
            handshakeResponse.trackerId = tracker->trackerId;
            handshakeResponse.channel = channel;
            // Serial.printf("Re-sending handshake ack to " MACSTR " for tracker ID %d\n", MAC2ARGS(senderInfo->src_addr), tracker->trackerId);
            queueMessage(senderInfo->src_addr, reinterpret_cast<const uint8_t *>(&handshakeResponse), sizeof(ESPNowConnectionAckMessage));
            return;
        }

        // Step 1: Get persistent tracker ID for this MAC address
        uint8_t trackerId = Configuration::getInstance().getTrackerIdForMac(senderInfo->src_addr);

        // Step 2: Send handshake response with tracker ID and channel
        ESPNowConnectionAckMessage handshakeResponse;
        handshakeResponse.trackerId = trackerId;
        handshakeResponse.channel = channel;
        // Serial.printf("Sending handshake ack to " MACSTR " with tracker ID %d\n", MAC2ARGS(senderInfo->src_addr), trackerId);
        queueMessage(senderInfo->src_addr, reinterpret_cast<const uint8_t *>(&handshakeResponse), sizeof(ESPNowConnectionAckMessage));

        // Step 3: Add tracker to connected list with heartbeat tracking
        Tracker newTracker;
        memcpy(newTracker.mac.data(), senderInfo->src_addr, 6);
        newTracker.trackerId = trackerId;
        newTracker.lastPingSent = 0;
        newTracker.waitingForResponse = false;
        newTracker.missedPings = 0;
        connectedTrackers.push_back(newTracker);

        Serial.printf("Device with mac address " MACSTR " connected with tracker id %d!\n", MAC2ARGS(senderInfo->src_addr), trackerId);

        // Step 4: Send rate update to newly connected trackers
        sendRateUpdateNextTick = true;

        // Step 5: Invoke connected event (also sends rate updates to all other trackers)
        invokeTrackerConnectedEvent(senderInfo->src_addr);
        return;
    }
    case ESPNowMessageTypes::HEARTBEAT_ECHO: {
        // Fast MAC lookup for connected tracker
        const uint8_t *mac = senderInfo->src_addr;
        Tracker *tracker = getTracker(mac);
        if (tracker == nullptr) return;
        tracker->missedPings = 0;

        // Send heartbeat response with the same sequence number
        ESPNowHeartbeatResponseMessage response;
        response.sequenceNumber = message->heartbeatEcho.sequenceNumber;
        // Serial.printf("Sending heartbeat response to tracker " MACSTR " with sequence number %u\n", MAC2ARGS(mac), response.sequenceNumber);
        queueMessage(mac, reinterpret_cast<const uint8_t *>(&response), sizeof(ESPNowHeartbeatResponseMessage));
        return;
    }
    case ESPNowMessageTypes::HEARTBEAT_RESPONSE: {
        // Find the tracker and update heartbeat info
        const uint8_t *mac = senderInfo->src_addr;
        Tracker *tracker = getTracker(mac);
        if (tracker == nullptr) return;
        if (tracker->waitingForResponse) {
            // Validate sequence number matches expected
            if (message->heartbeatResponse.sequenceNumber == tracker->expectedSequenceNumber) {
                unsigned long latency = millis() - tracker->pingStartTime;
                tracker->latency = static_cast<uint8_t>(latency);
                tracker->waitingForResponse = false;
                tracker->missedPings = 0;
                senderInfo->rx_ctrl->rssi;
            }
            // If sequence number doesn't match, ignore the response (likely stale)
        }
        return;
    }
    case ESPNowMessageTypes::ENTER_OTA_ACK:{
        // Find the tracker and mark it as in OTA
        const uint8_t *mac = senderInfo->src_addr;
        Tracker *tracker = getTracker(mac);
        if (tracker == nullptr) return;

        disconnectSingleTracker(mac);
        return;
    }
    default:
        break;
    }
}

// Main update loop to be called regularly
void ESPNowCommunication::update() {
    const unsigned long currentTime = millis();

    // PRIORITY 1: Handle heartbeat system FIRST - critical for connection stability
    // Process heartbeats before stats/pairing to maintain accurate timing
    if (!connectedTrackers.empty() && (currentTime - lastHeartbeatCheck >= heartbeatInterval+100)) {
        lastHeartbeatCheck = currentTime;

        //For each connected tracker
        for (auto it = connectedTrackers.begin(); it != connectedTrackers.end();) {
            auto &tracker = *it;

            // Check if waiting for response and timeout has occurred
            if (tracker.waitingForResponse && (currentTime - tracker.pingStartTime >= heartbeatTimeout)) {
                tracker.missedPings++;
                tracker.waitingForResponse = false;
                Serial.printf("Missed heartbeat from tracker " MACSTR " (ID: %d), missed count: %d\n", MAC2ARGS(tracker.mac.data()), tracker.trackerId, tracker.missedPings);

                // Send timed out status on second missed heartbeat
                if (tracker.missedPings == 3) {
                    // Send packet type 3 with SVR_STATUS_TIMED_OUT (2)
                    uint8_t statusPacket[16] = {0};
                    statusPacket[0] = 3; // packet type 3 (status)
                    statusPacket[1] = tracker.trackerId;
                    statusPacket[2] = 5;             // SVR_STATUS_TIMED_OUT
                    statusPacket[3] = 0;             // tracker_status (not relevant for timeout)
                    statusPacket[15] = tracker.rssi; // Use last known RSSI before timeout
                    PacketHandling::getInstance().insert(statusPacket, 16, 0);
                }

                // Remove tracker if exceeded max missed pings
                if (tracker.missedPings >= maxMissedPings)
                {
                    Serial.printf("Removing tracker " MACSTR " (ID: %d) due to missed heartbeats\n", MAC2ARGS(tracker.mac.data()), tracker.trackerId);
                    disconnectSingleTracker(tracker.mac.data());
                    continue; // Skip increment since we erased
                }
            }

            // Send heartbeat ping if interval has elapsed and not waiting for response
            if (!tracker.waitingForResponse && (currentTime - tracker.lastPingSent >= heartbeatInterval)) {
                // Generate random 16-bit sequence number using hardware RNG
                tracker.expectedSequenceNumber = static_cast<uint16_t>(esp_random() & 0xFFFF);

                // Create and send heartbeat echo message with sequence number
                ESPNowHeartbeatEchoMessage heartbeatMsg;
                heartbeatMsg.sequenceNumber = tracker.expectedSequenceNumber;

                // Queue heartbeat through the rate-limited queue to prevent ESP_ERR_ESPNOW_NO_MEM
                tracker.lastPingSent = currentTime;
                tracker.pingStartTime = currentTime;
                
                // Serial.printf("Sending heartbeat echo to tracker " MACSTR " with sequence number %u\n", MAC2ARGS(tracker.mac.data()), heartbeatMsg.sequenceNumber);
                queueMessage(tracker.mac.data(), reinterpret_cast<uint8_t *>(&heartbeatMsg), sizeof(ESPNowHeartbeatEchoMessage), &tracker);
                tracker.waitingForResponse = true;
            }

            ++it;
        }
    }

    // Skip lower priority tasks if an OTA update is in progress
    if (ota_in_progress) {
        if (getConnectedTrackerCount() == 0) {
            ota_in_progress = false;
            Serial.println("All trackers entered OTA, resuming normal operation");
            return;
        } else if (currentTime - ota_start_time > ota_timeout) {
            ota_in_progress = false;
            Serial.println("OTA timeout expired, resuming normal operation");
            return;
        } else if (currentTime - ota_last_send_time >= ota_send_interval) {
            ota_last_send_time = currentTime;

            // Send enter OTA command to all connected trackers
            ESPNowEnterOtaModeMessage otaMsg;
            memcpy(otaMsg.securityBytes, securityCode, 8);
            otaMsg.ota_portNum = ota_portNum;
            memcpy(otaMsg.ota_ip, ota_ip, 4);
            memcpy(otaMsg.ota_auth, ota_auth, 16);
            memcpy(otaMsg.ssid, ota_ssid, sizeof(ota_ssid));
            memcpy(otaMsg.password, ota_password, sizeof(ota_password));

            queueMessage(broadcastAddress, reinterpret_cast<uint8_t *>(&otaMsg), sizeof(ESPNowEnterOtaModeMessage));
        }

        // Still report stats during OTA to monitor progress
        if (currentTime - lastStatsReport >= 1000) {
            const int deltaTime = currentTime - lastStatsReport;
            lastStatsReport = currentTime;

            const int pps = (recievedPacketCount * 1000) / deltaTime;
            recievedPacketCount = 0;

            const int bytesPerSecond = (recievedByteCount * 1000) / deltaTime;
            recievedByteCount = 0;

            Serial.printf("OTA in progress - T:%d|PPS:%d|BPS:%d|Q:%d\n", getConnectedTrackerCount(), pps, bytesPerSecond, queueSize());
        }
        return;
    }

    // PRIORITY 2: Handle pairing announcements - only when in pairing mode
    if (pairing && (currentTime - lastPairingBroadcast >= pairingBroadcastInterval)) {
        lastPairingBroadcast = currentTime;

        ESPNowPairingAnnouncementMessage announcement;
        announcement.channel = channel;
        memcpy(announcement.securityBytes, securityCode, 8);

        // Serial.println("Broadcasting pairing announcement");
        queueMessage(broadcastAddress, reinterpret_cast<uint8_t *>(&announcement), sizeof(announcement));
    }

    // PRIORITY 3: Print tracker statistics (lowest priority - can be skipped if timing is tight)
    if (currentTime - lastStatsReport >= 1000) {
        const int deltaTime = currentTime - lastStatsReport;
        lastStatsReport = currentTime;

        const int pps = (recievedPacketCount * 1000) / deltaTime;
        recievedPacketCount = 0;

        const int bytesPerSecond = (recievedByteCount * 1000) / deltaTime;
        recievedByteCount = 0;

        // Calculate latency and RSSI stats in single pass
        uint8_t highestLatency = 0;
        unsigned long totalLatency = 0;
        int8_t maxRssi = -128; // Start with minimum possible RSSI
        long totalRssi = 0;
        const size_t trackerCount = connectedTrackers.size();

        for (const auto &tracker : connectedTrackers) {
            const uint8_t lat = tracker.latency;
            totalLatency += lat;
            if (lat > highestLatency) highestLatency = lat;

            const int8_t rssi = tracker.rssi;
            totalRssi += rssi;
            if (rssi > maxRssi) maxRssi = rssi;
        }

        const uint8_t avgLatency = trackerCount > 0 ? totalLatency / trackerCount : 0;
        const int8_t avgRssi = trackerCount > 0 ? totalRssi / trackerCount : 0;

        // Use shorter format to reduce blocking time
        Serial.printf("T:%d|L:%d/%dms|RSSI:%d/%ddBm|PPS:%d|BPS:%d|Q:%d\n", trackerCount, avgLatency, highestLatency, avgRssi, maxRssi, pps, bytesPerSecond, queueSize());
    }

    // PRIORITY 4: Process send queue - rate limiting to prevent ESP_ERR_ESPNOW_NO_MEM
    processSendQueue();

    // PRIORITY 5: Send rate update if flagged
    if (sendRateUpdateNextTick && (currentTime - lastRateUpdateTime >= 1000)) {
        sendRateUpdateToAllTrackers();
        sendRateUpdateNextTick = false;
        lastRateUpdateTime = currentTime;
    }
}

// Converts ESPNOW error codes to human-readable strings
std::string ESPNowCommunication::espNowErrorToString(esp_err_t error) {
    switch (error) {
    case ESP_OK:
        return "ESP_OK";
    case ESP_ERR_ESPNOW_NOT_INIT:
        return "ESP_ERR_ESPNOW_NOT_INIT";
    case ESP_ERR_ESPNOW_ARG:
        return "ESP_ERR_ESPNOW_ARG";
    case ESP_ERR_ESPNOW_NO_MEM:
        return "ESP_ERR_ESPNOW_NO_MEM";
    case ESP_ERR_ESPNOW_FULL:
        return "ESP_ERR_ESPNOW_FULL";
    case ESP_ERR_ESPNOW_NOT_FOUND:
        return "ESP_ERR_ESPNOW_NOT_FOUND";
    case ESP_ERR_ESPNOW_INTERNAL:
        return "ESP_ERR_ESPNOW_INTERNAL";
    case ESP_ERR_ESPNOW_EXIST:
        return "ESP_ERR_ESPNOW_EXIST";
    default:
        return "UNKNOWN_ERROR - " + std::to_string(error);
    }
}

// Adds a ESP-Now peer with the given MAC address
uint8_t ESPNowCommunication::addPeer(const uint8_t peerMac[6], bool defaultConfig) {
    Serial.printf("Adding peer " MACSTR "\n", MAC2ARGS(peerMac));
    // Check if peer already exists
    if (esp_now_is_peer_exist(peerMac)) {
        Serial.printf("Peer " MACSTR " already exists.\n", MAC2ARGS(peerMac));
        return ESP_OK; // Peer already exists, return success
    }

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    memcpy(peer.peer_addr, peerMac, sizeof(uint8_t[6]));
    peer.channel = 0;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;

    esp_err_t result = esp_now_add_peer(&peer);
    if (result != ESP_OK) {
        Serial.printf("Failed to add peer, error: %s\n", espNowErrorToString(result).c_str());
    } else if (!defaultConfig){
        esp_now_set_peer_rate_config(peer.peer_addr, &rate_config);
    }
    return result;
}

// Adds a ESP-Now peer with the given MAC address (defaultConfig = false)
uint8_t ESPNowCommunication::addPeer(const uint8_t peerMac[6]) {
    return addPeer(peerMac, false);
}

// Deletes a ESP-Now peer with the given MAC address
bool ESPNowCommunication::deletePeer(const uint8_t peerMac[6]) {
    if (!esp_now_is_peer_exist(peerMac)) {
        Serial.printf("Peer " MACSTR " does not exist.\n", MAC2ARGS(peerMac));
        return true; // Peer does not exist, return success
    }

    Serial.printf("Deleting peer " MACSTR "\n", MAC2ARGS(peerMac));
    auto result = esp_now_del_peer(peerMac);
    if (result != ESP_OK || esp_now_is_peer_exist(peerMac)) Serial.printf("Failed to delete peer " MACSTR ", error: %s\n", MAC2ARGS(peerMac), espNowErrorToString(result).c_str());

	//Remove all pending messages to this peer from the send queue by setting the ignore flag
	for (size_t i = 0; i < maxQueueSize; ++i) if (memcmp(sendQueue[i].peerMac, peerMac, 6) == 0) sendQueue[i].skip = true; // Mark message to be skipped

    return result == ESP_OK;
}

void ESPNowCommunication::startOtaUpdate(const uint8_t auth[16], long port, const uint8_t ip[4], const char ssid[33], const char password[65]) {
    memcpy(ota_auth, auth, sizeof(ota_auth));
    ota_portNum = port;
    memcpy(ota_ip, ip, sizeof(ota_ip));
    memcpy(ota_ssid, ssid, sizeof(ota_ssid));
    memcpy(ota_password, password, sizeof(ota_password));
    
    ota_in_progress = true;
    ota_start_time = millis();
}