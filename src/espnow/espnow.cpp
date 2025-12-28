#include "configuration.h"
#include "espnow.h"

#include "configuration.h"
#include "espnow/messages.h"
#include "packetHandling.h"
#include <esp_wifi.h>


#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2ARGS(mac) mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]

// Static member definition
unsigned int ESPNowCommunication::channel = 6;

size_t ESPNowCommunication::getConnectedTrackerCount() const {
    return connectedTrackers.size();
}

bool ESPNowCommunication::getTrackerMacByIndex(size_t index, uint8_t mac[6]) const {
    if (index >= connectedTrackers.size()) {
        return false;
    }
    memcpy(mac, connectedTrackers[index].mac.data(), 6);
    return true;
}

void onReceive(const esp_now_recv_info_t *senderInfo,
               const uint8_t *data,
               int dataLen)
{
    ESPNowCommunication::getInstance().handleMessage(senderInfo, data, dataLen);
}

ESPNowCommunication &ESPNowCommunication::getInstance()
{
    return instance;
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
            return true;
        }
    }
    return false;
}

ErrorCodes ESPNowCommunication::begin()
{
    channel = Configuration::getInstance().getWifiChannel();
    // Pre-allocate vectors to avoid reallocations during operation
    connectedTrackers.reserve(16); // Reserve space for up to 16 trackers
    trackerConnectedCallbacks.reserve(4);
    trackerPairedCallbacks.reserve(4);

    // Load or generate security code
    Configuration::getInstance().getSecurityCode(securityCode);

    WiFi.mode(WIFI_STA);
    WiFi.setChannel(channel);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11N);
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Couldn't initialize ESPNOW!");
        return ErrorCodes::ESP_NOW_INIT_FAILED;
    }

    if (addPeer(broadcastAddress) != ESP_OK)
    {
        Serial.println("Couldn't add broadcast as ESPNOW peer!");
        return ErrorCodes::ESP_NOW_ADDING_BROADCAST_FAILED;
    }

    if (esp_now_register_recv_cb(onReceive) != ESP_OK)
    {
        Serial.println("Couldn't register message callback!");
        return ErrorCodes::ESP_RECV_CALLACK_REGISTERING_FAILED;
    }

    uint8_t macaddr[6];
    WiFi.macAddress(macaddr);

    Serial.printf(
        "[ESPNOW] address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        macaddr[0],
        macaddr[1],
        macaddr[2],
        macaddr[3],
        macaddr[4],
        macaddr[5]);

    return ErrorCodes::NO_ERROR;
}

bool ESPNowCommunication::isTrackerIdConnected(uint8_t trackerId) const {
    for (const auto &tracker : connectedTrackers) {
        if (tracker.trackerId == trackerId) {
            return true;
        }
    }
    return false;
}

void ESPNowCommunication::enterPairingMode()
{
    pairing = true;
}

void ESPNowCommunication::exitPairingMode()
{
    pairing = false;
}

bool ESPNowCommunication::isInPairingMode()
{
    return pairing;
}

void ESPNowCommunication::disconnectAllTrackers()
{
    Serial.printf("Disconnecting all %d trackers...\n", connectedTrackers.size());

    for (const auto &tracker : connectedTrackers)
    {
        deletePeer(tracker.mac.data());
        Serial.printf("Disconnected tracker " MACSTR "\n", MAC2ARGS(tracker.mac.data()));
    }

    connectedTrackers.clear();
    Serial.println("All trackers disconnected");
}

void ESPNowCommunication::sendUnpairToAllTrackers()
{
    Serial.printf("Sending unpair message to all %d trackers...\n", connectedTrackers.size());
    
    ESPNowUnpairMessage unpairMsg;
    memcpy(unpairMsg.securityBytes, securityCode, 8);
    
    for (const auto &tracker : connectedTrackers)
    {
        // Temporarily add peer to send unpair message
        if (addPeer(tracker.mac.data()) == ESP_OK)
        {
            esp_now_send(tracker.mac.data(),
                        reinterpret_cast<const uint8_t *>(&unpairMsg),
                        sizeof(ESPNowUnpairMessage));
            
            // Send twice to ensure delivery
            esp_now_send(tracker.mac.data(),
                        reinterpret_cast<const uint8_t *>(&unpairMsg),
                        sizeof(ESPNowUnpairMessage));
                        
            Serial.printf("Sent unpair to tracker " MACSTR "\n", MAC2ARGS(tracker.mac.data()));
        }
    }
    
    Serial.println("Unpair messages sent to all trackers");
}

void ESPNowCommunication::sendUnpairToTracker(const uint8_t mac[6])
{
    ESPNowUnpairMessage unpairMsg;
    memcpy(unpairMsg.securityBytes, securityCode, 8);
    // Temporarily add peer to send unpair message
    if (addPeer(mac) == ESP_OK)
    {
        esp_now_send(mac,
                    reinterpret_cast<const uint8_t *>(&unpairMsg),
                    sizeof(ESPNowUnpairMessage));
        // Send twice to ensure delivery
        esp_now_send(mac,
                    reinterpret_cast<const uint8_t *>(&unpairMsg),
                    sizeof(ESPNowUnpairMessage));
        Serial.printf("Sent unpair to tracker " MACSTR "\n", MAC2ARGS(mac));
    }
}

void ESPNowCommunication::onTrackerPaired(std::function<void()> callback)
{
    trackerPairedCallbacks.push_back(callback);
}

void ESPNowCommunication::onTrackerConnected(
    std::function<void(const uint8_t *)> callback)
{
    trackerConnectedCallbacks.push_back(std::move(callback));
}

void ESPNowCommunication::onTrackerDisconnected(
    std::function<void(uint8_t)> callback)
{
    trackerDisconnectedCallbacks.push_back(std::move(callback));
}

void ESPNowCommunication::invokeTrackerPairedEvent()
{
    for (auto &callback : trackerPairedCallbacks)
    {
        callback();
    }
}

void ESPNowCommunication::invokeTrackerConnectedEvent(const uint8_t *trackerMacAddress)
{
    for (auto &callback : trackerConnectedCallbacks)
    {
        callback(trackerMacAddress);
    }
    // Update polling rate for all trackers when a new tracker connects
    sendRateUpdateToAllTrackers();
}

void ESPNowCommunication::invokeTrackerDisconnectedEvent(uint8_t trackerId)
{
    for (auto &callback : trackerDisconnectedCallbacks)
    {
        callback(trackerId);
    }
    // Update polling rate for all trackers when a tracker disconnects
    sendRateUpdateToAllTrackers();
}

void ESPNowCommunication::sendRateUpdateToAllTrackers()
{
    size_t trackerCount = connectedTrackers.size();
    if (trackerCount == 0) {
        return;  // No trackers to update
    }

    // Calculate polling rate per tracker in Hz: divide maxPPS by number of trackers
    uint32_t pollRateHz = maxPPS / trackerCount;

    Serial.printf("Updating tracker rate: %u trackers, %u Hz per tracker\\n",
                  trackerCount, pollRateHz);

    // Send rate update to all connected trackers
    ESPNowTrackerRateMessage rateMsg;
    rateMsg.pollRateHz = pollRateHz;

    for (const auto &tracker : connectedTrackers) {
        esp_now_send(tracker.mac.data(), 
                     reinterpret_cast<const uint8_t *>(&rateMsg), 
                     sizeof(ESPNowTrackerRateMessage));
    }
}

void ESPNowCommunication::handleMessage(const esp_now_recv_info_t *senderInfo, const uint8_t *data, int dataLen)
{
    // Fast path: cast message once and read header
    const ESPNowMessage *message = reinterpret_cast<const ESPNowMessage *>(data);
    const ESPNowMessageTypes header = message->base.header;

    // Optimize the most common case - TRACKER_DATA (hot path)
    if (header == ESPNowMessageTypes::TRACKER_DATA)
    {
        // Fast validation: check if tracker is connected (most packets come from connected trackers)
        const uint8_t *mac = senderInfo->src_addr;
        for (auto &tracker : connectedTrackers)
        {
            // Quick MAC comparison - compare as uint32 + uint16 instead of 6 individual bytes
            if (*reinterpret_cast<const uint32_t *>(tracker.mac.data()) == *reinterpret_cast<const uint32_t *>(mac) &&
                *reinterpret_cast<const uint16_t *>(tracker.mac.data() + 4) == *reinterpret_cast<const uint16_t *>(mac + 4))
            {

                // Tracker found and connected - process packet
                recievedPacketCount++;
                recievedByteCount += message->packet.len;
                
                // Update RSSI for this tracker
                tracker.rssi = senderInfo->rx_ctrl->rssi;
                
                // Forward packet to PacketHandling with RSSI
                PacketHandling::getInstance().insert(message->packet.data, message->packet.len, senderInfo->rx_ctrl->rssi);
                return;
            }
        }
        // Tracker not connected - ignore packet
        return;
    }

    // Handle less frequent message types
    switch (header)
    {
    case ESPNowMessageTypes::PAIRING_REQUEST:
    {
        if (!pairing) return; // Ignore pairing requests if not in pairing mode

        const ESPNowPairingMessage &request = message->pairing;
        if (memcmp(request.securityBytes, securityCode, 8) != 0) return; // Invalid security code

        // Step 1: Add peer to allow communication
        auto addingResult = addPeer(senderInfo->src_addr);
        if (addingResult != ESP_OK)
        {
            Serial.printf("Couldn't add tracker at mac address " MACSTR "! Code: %d\n",
                          MAC2ARGS(senderInfo->src_addr), addingResult);
            return;
        }

        // Step 2: Add to paired trackers and allocate tracker ID (if not already paired)
        if (!Configuration::getInstance().isPairedTracker(senderInfo->src_addr))
        {
            Configuration::getInstance().addPairedTracker(senderInfo->src_addr);
            // Allocate persistent tracker ID for this MAC address
            uint8_t trackerId = Configuration::getInstance().getTrackerIdForMac(senderInfo->src_addr);
            Serial.printf("Paired a new tracker at mac address " MACSTR " with ID %d!\n", 
                         MAC2ARGS(senderInfo->src_addr), trackerId);
        }
        else
        {
            Serial.printf("Tracker at mac address " MACSTR " is already paired!\n", 
                         MAC2ARGS(senderInfo->src_addr));
        }

        // Step 3: Send acknowledgment
        ESPNowPairingAckMessage ackMessage;
        esp_now_send(senderInfo->src_addr, reinterpret_cast<uint8_t *>(&ackMessage), sizeof(ackMessage));

        // Step 4: Remove peer after sending acknowledgment
        deletePeer(senderInfo->src_addr);

        // Step 5: Invoke paired event
        invokeTrackerPairedEvent();
        break;
    }
    case ESPNowMessageTypes::PAIRING_RESPONSE:
        return;
    case ESPNowMessageTypes::HANDSHAKE_REQUEST:
    {
        const ESPNowConnectionMessage &handshake = message->connection;
        if (memcmp(handshake.securityBytes, securityCode, 8) != 0)
        {
            Serial.printf("Received handshake from " MACSTR " with invalid security code! Sent: ",
                          MAC2ARGS(senderInfo->src_addr));
            for (int i = 0; i < 8; ++i) {
                Serial.printf("%02x", handshake.securityBytes[i]);
                if (i < 7) Serial.print(":");
            }
            Serial.println();
            return;
        }

        // Check that the tracker MAC is in persistent memory
        if (!Configuration::getInstance().isPairedTracker(senderInfo->src_addr))
        {
            Serial.printf("Received handshake from unpaired tracker " MACSTR " - ignoring!\n",
                          MAC2ARGS(senderInfo->src_addr));
            return;
        }

        // Check to make sure the tracker isn't already connected
        if (isTrackerConnected(senderInfo->src_addr))
        {
            Serial.printf("Tracker at mac address " MACSTR " is already connected!\n",
                          MAC2ARGS(senderInfo->src_addr));
            
            // Get persistent tracker ID from Configuration
            uint8_t trackerId = Configuration::getInstance().getTrackerIdForMac(senderInfo->src_addr);
            
            ESPNowConnectionAckMessage handshakeResponse;
            handshakeResponse.trackerId = trackerId;
            handshakeResponse.channel = channel;
            auto result = esp_now_send(senderInfo->src_addr, reinterpret_cast<const uint8_t *>(&handshakeResponse), sizeof(ESPNowConnectionAckMessage));

            if (result != ESP_OK)
            {
                Serial.printf("Failed to send handshake response to " MACSTR ", error: %d\n",
                              MAC2ARGS(senderInfo->src_addr), result);
                return;
            }

            return;
        }

        // Step 1: Add peer to allow communication
        auto addingResult = addPeer(senderInfo->src_addr);
        if (addingResult != ESP_OK)
        {
            Serial.printf("Couldn't add tracker at mac address " MACSTR "! Code: %d\n",
                          MAC2ARGS(senderInfo->src_addr), addingResult);
            return;
        }

        // Step 2: Get persistent tracker ID for this MAC address
        uint8_t trackerId = Configuration::getInstance().getTrackerIdForMac(senderInfo->src_addr);

        // Step 3: Send handshake response with tracker ID and channel
        ESPNowConnectionAckMessage handshakeResponse;
        handshakeResponse.trackerId = trackerId;
        handshakeResponse.channel = channel;
        auto result = esp_now_send(senderInfo->src_addr, reinterpret_cast<const uint8_t *>(&handshakeResponse), sizeof(ESPNowConnectionAckMessage));

        if (result != ESP_OK)
        {
            Serial.printf("Failed to send handshake response to " MACSTR ", error: %d\n",
                          MAC2ARGS(senderInfo->src_addr), result);
            return;
        }

        // Step 4: Add tracker to connected list with heartbeat tracking
        TrackerHeartbeat newTracker;
        memcpy(newTracker.mac.data(), senderInfo->src_addr, 6);
        newTracker.trackerId = trackerId;
        newTracker.lastPingSent = 0;
        newTracker.waitingForResponse = false;
        newTracker.missedPings = 0;
        connectedTrackers.push_back(newTracker);

        Serial.printf("Device with mac address " MACSTR " connected with tracker id %d!\n",
                      MAC2ARGS(senderInfo->src_addr), trackerId);
        
        // Step 5: Send rate update to newly connected tracker
        size_t trackerCount = connectedTrackers.size();
        uint32_t pollRateHz = maxPPS / trackerCount;
        ESPNowTrackerRateMessage rateMsg;
        rateMsg.pollRateHz = pollRateHz;
        esp_now_send(senderInfo->src_addr, 
                     reinterpret_cast<const uint8_t *>(&rateMsg), 
                     sizeof(ESPNowTrackerRateMessage));
        
        // Step 6: Invoke connected event (also sends rate updates to all other trackers)
        invokeTrackerConnectedEvent(senderInfo->src_addr);
        break;
    }
    case ESPNowMessageTypes::HEARTBEAT_ECHO:
    {
        // Fast MAC lookup for connected tracker
        const uint8_t *mac = senderInfo->src_addr;
        for (const auto &tracker : connectedTrackers)
        {
            if (*reinterpret_cast<const uint32_t *>(tracker.mac.data()) == *reinterpret_cast<const uint32_t *>(mac) &&
                *reinterpret_cast<const uint16_t *>(tracker.mac.data() + 4) == *reinterpret_cast<const uint16_t *>(mac + 4))
            {

                // Send heartbeat response with the same sequence number
                ESPNowHeartbeatResponseMessage response;
                response.sequenceNumber = message->heartbeatEcho.sequenceNumber;
                esp_now_send(mac, reinterpret_cast<const uint8_t *>(&response), sizeof(ESPNowHeartbeatResponseMessage));
                return;
            }
        }
        break;
    }
    case ESPNowMessageTypes::HEARTBEAT_RESPONSE:
    {
        // Find the tracker and update heartbeat info
        const uint8_t *mac = senderInfo->src_addr;
        for (auto &tracker : connectedTrackers)
        {
            if (*reinterpret_cast<const uint32_t *>(tracker.mac.data()) == *reinterpret_cast<const uint32_t *>(mac) &&
                *reinterpret_cast<const uint16_t *>(tracker.mac.data() + 4) == *reinterpret_cast<const uint16_t *>(mac + 4))
            {

                if (tracker.waitingForResponse)
                {
                    // Validate sequence number matches expected
                    if (message->heartbeatResponse.sequenceNumber == tracker.expectedSequenceNumber)
                    {
                        unsigned long latency = millis() - tracker.pingStartTime;
                        tracker.latency = static_cast<uint8_t>(latency);
                        tracker.waitingForResponse = false;
                        tracker.missedPings = 0;
                        senderInfo->rx_ctrl->rssi;
                    }
                    // If sequence number doesn't match, ignore the response (likely stale)
                }
                return;
            }
        }
        break;
    }
    default:
        break;
    }
}

uint8_t ESPNowCommunication::addPeer(const uint8_t peerMac[6])
{
    // Check if peer already exists
    if (esp_now_is_peer_exist(peerMac))
    {
        return ESP_OK; // Peer already exists, return success
    }

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    memcpy(peer.peer_addr, peerMac, sizeof(uint8_t[6]));
    peer.channel = 0;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;

    esp_err_t result = esp_now_add_peer(&peer);
    if (result != ESP_OK)
    {
        Serial.printf("Failed to add peer, error: %d\n", result);
    }
    return result;
}

bool ESPNowCommunication::deletePeer(const uint8_t peerMac[6])
{
    return esp_now_del_peer(peerMac) == ESP_OK;
}

inline bool ESPNowCommunication::isTrackerConnected(const uint8_t peerMac[6])
{
    // Fast MAC comparison using integer comparisons instead of memcmp
    for (const auto &tracker : connectedTrackers)
    {
        if (*reinterpret_cast<const uint32_t *>(tracker.mac.data()) == *reinterpret_cast<const uint32_t *>(peerMac) &&
            *reinterpret_cast<const uint16_t *>(tracker.mac.data() + 4) == *reinterpret_cast<const uint16_t *>(peerMac + 4))
        {
            return true;
        }
    }
    return false;
}

ESPNowCommunication ESPNowCommunication::instance;

void ESPNowCommunication::update()
{
    const unsigned long currentTime = millis();

    // PRIORITY 1: Handle heartbeat system FIRST - critical for connection stability
    // Process heartbeats before stats/pairing to maintain accurate timing
    if (!connectedTrackers.empty() && (currentTime - lastHeartbeatCheck >= heartbeatInterval))
    {
        lastHeartbeatCheck = currentTime;

        for (auto it = connectedTrackers.begin(); it != connectedTrackers.end();)
        {
            auto &tracker = *it;

            // Check if waiting for response and timeout has occurred
            if (tracker.waitingForResponse && (currentTime - tracker.pingStartTime >= heartbeatTimeout))
            {
                tracker.missedPings++;
                tracker.waitingForResponse = false;

                // Send timed out status on second missed heartbeat
                if (tracker.missedPings == 2)
                {
                    Serial.printf("Tracker " MACSTR " (ID: %d) timed out (2 missed heartbeats)\n",
                                  MAC2ARGS(tracker.mac.data()), tracker.trackerId);
                    
                    // Send packet type 3 with SVR_STATUS_TIMED_OUT (2)
                    uint8_t statusPacket[16] = {0};
                    statusPacket[0] = 3;  // packet type 3 (status)
                    statusPacket[1] = tracker.trackerId;
                    statusPacket[2] = 5;  // SVR_STATUS_TIMED_OUT
                    statusPacket[3] = 0;  // tracker_status (not relevant for timeout)
                    statusPacket[15] = tracker.rssi; // Use last known RSSI before timeout
                    PacketHandling::getInstance().insert(statusPacket, 16, 0);
                }

                // Remove tracker if exceeded max missed pings
                if (tracker.missedPings >= maxMissedPings)
                {
                    Serial.printf("Removing tracker " MACSTR " (ID: %d) due to missed heartbeats\n",
                                  MAC2ARGS(tracker.mac.data()), tracker.trackerId);
                    uint8_t disconnectedTrackerId = tracker.trackerId;
                    deletePeer(tracker.mac.data());
                    it = connectedTrackers.erase(it);
                    
                    // Notify about disconnection
                    invokeTrackerDisconnectedEvent(disconnectedTrackerId);
                    continue; // Skip increment since we erased
                }
            }

            // Send heartbeat ping if interval has elapsed and not waiting for response
            if (!tracker.waitingForResponse && (currentTime - tracker.lastPingSent >= heartbeatInterval))
            {

                // Generate random 16-bit sequence number using hardware RNG
                tracker.expectedSequenceNumber = static_cast<uint16_t>(esp_random() & 0xFFFF);

                // Create and send heartbeat echo message with sequence number
                ESPNowHeartbeatEchoMessage heartbeatMsg;
                heartbeatMsg.sequenceNumber = tracker.expectedSequenceNumber;

                if (esp_now_send(tracker.mac.data(),
                                 reinterpret_cast<uint8_t *>(&heartbeatMsg),
                                 sizeof(ESPNowHeartbeatEchoMessage)) == ESP_OK)
                {
                    tracker.lastPingSent = currentTime;
                    tracker.pingStartTime = currentTime;
                    tracker.waitingForResponse = true;
                }
            }

            ++it;
        }
    }

    // PRIORITY 2: Handle pairing announcements - only when in pairing mode
    if (pairing && (currentTime - lastPairingBroadcast >= pairingBroadcastInterval))
    {
        lastPairingBroadcast = currentTime;

        ESPNowPairingAnnouncementMessage announcement;
        announcement.channel = channel;
        memcpy(announcement.securityBytes, securityCode, 8);

        esp_now_send(broadcastAddress,
                     reinterpret_cast<uint8_t *>(&announcement),
                     sizeof(announcement));
    }

    // PRIORITY 3: Print tracker statistics (lowest priority - can be skipped if timing is tight)
    if (currentTime - lastStatsReport >= 1000)
    {
        const int deltaTime = currentTime - lastStatsReport;
        lastStatsReport = currentTime;

        const int pps = (recievedPacketCount * 1000) / deltaTime;
        recievedPacketCount = 0;

        const int bytesPerSecond = (recievedByteCount * 1000) / deltaTime;
        recievedByteCount = 0;

        // Calculate latency and RSSI stats in single pass
        uint8_t highestLatency = 0;
        unsigned long totalLatency = 0;
        int8_t maxRssi = -128;  // Start with minimum possible RSSI
        long totalRssi = 0;
        const size_t trackerCount = connectedTrackers.size();

        for (const auto &tracker : connectedTrackers)
        {
            const uint8_t lat = tracker.latency;
            totalLatency += lat;
            if (lat > highestLatency)
            {
                highestLatency = lat;
            }
            
            const int8_t rssi = tracker.rssi;
            totalRssi += rssi;
            if (rssi > maxRssi)
            {
                maxRssi = rssi;
            }
        }

        const uint8_t avgLatency = trackerCount > 0 ? totalLatency / trackerCount : 0;
        const int8_t avgRssi = trackerCount > 0 ? totalRssi / trackerCount : 0;

        // Use shorter format to reduce blocking time
        Serial.printf("T:%d|L:%d/%dms|RSSI:%d/%ddBm|PPS:%d|BPS:%d\n",
                      trackerCount, avgLatency, highestLatency, avgRssi, maxRssi, pps, bytesPerSecond);
    }
}