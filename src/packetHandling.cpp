#include "packetHandling.h"
#include "espnow/espnow.h"
#include "Configuration.h"

PacketHandling &PacketHandling::getInstance() {
    return instance;
}

void PacketHandling::insert(const uint8_t *data, uint8_t len, int8_t rssi) {
    if (len < 2) {
        return; // Need at least packet type and tracker ID
    }

    // Read packet type and tracker ID early for deduplication
    uint8_t packetType = data[0];
    uint8_t trackerId = data[1];

    // FIFO deduplication: Check if this tracker already has data queued
    // Scan existing entries in buffer for same tracker
    for (size_t i = 0; i < buffer.size(); i++) {
        Packet existing = buffer[i];
        if (existing.data[0] == packetType && existing.data[1] == trackerId) {
            // Found existing packet - update in place
            memcpy(existing.data, data, std::min(static_cast<size_t>(len), sizeof(Packet::data)));
            
            // Add RSSI to byte 15 for applicable packet types
            if (packetType != 1 && packetType != 4) {
                existing.data[15] = static_cast<uint8_t>(-rssi);
            }
            
            // Write modified packet back to buffer
            buffer[i] = existing;
            return;
        }
    }

    // No duplicate found - check if buffer has space
    if (buffer.isFull()) {
        droppedReports++;
        Serial.printf("FIFO full! Dropped packet type %d for tracker %d (total dropped: %lu)\n", 
                     packetType, trackerId, droppedReports);
        return;
    }

    // Add new entry
    Packet packet;
    memset(packet.data, 0, sizeof(packet.data));
    memcpy(packet.data, data, std::min(static_cast<size_t>(len), sizeof(packet.data)));

    // Add RSSI to byte 15 for applicable packet types
    if (packetType != 1 && packetType != 4) {
        packet.data[15] = static_cast<uint8_t>(-rssi);
    }

    buffer.push(packet);
}

void PacketHandling::sendDisconnectionStatus(uint8_t trackerId) { 
    // Create packet 3 (status) with SVR_STATUS_DISCONNECTED (0)
    uint8_t packet[16] = {0};
    packet[0] = 3;  // packet type 3 (status)
    packet[1] = trackerId;
    packet[2] = 0;  // SVR_STATUS_DISCONNECTED
    packet[3] = 0;  // tracker_status (not relevant for disconnection)
    packet[15] = 0; // RSSI (will be 0 for disconnected tracker)

    Serial.printf("Sending disconnection status for tracker ID %d\n", trackerId);
    
    insert(packet, 16, 0);
}

void PacketHandling::createRegistrationReport(uint8_t *report, size_t trackerIndex) {
    // Format: [255][tracker_id][6-byte MAC address][8 bytes reserved]
    memset(report, 0, reportSize);
    report[0] = 0xff;
    report[1] = *ESPNowCommunication::getInstance().getTrackerIdByIndex(trackerIndex);
    
    // Get MAC address from connected trackers via ESPNow
    ESPNowCommunication::getInstance().getTrackerMacByIndex(trackerIndex, &report[2]);
    
    // Bytes 8-15 are reserved (already zeroed by memset)
    
    // Debug output
    // Serial.printf("Registration report for tracker #%d\n", trackerIndex);
    // Serial.printf("  Marker: 0x%02x, Tracker ID: %d\n", report[0], report[1]);
    // Serial.printf("  MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", report[2], report[3], report[4], report[5], report[6], report[7]);
}

void PacketHandling::tick(HIDDevice &hidDevice) {
    // PPS print every second (packet types 0-4)
    if (!hidDevice.ready()) return;
    unsigned long now = millis();

    //NOTE: This can be expensive if theres a lot of trackers paired, thats why its commented out for now
    // if (now - lastDiscoSweep > 5000) {
    //     Serial.println("[DISCO] Sending disconnection statuses for unused trackers");
    //     lastDiscoSweep = now;
    //     auto &espnow = ESPNowCommunication::getInstance();
    //     auto pairedIds = Configuration::getInstance().getAllPairedTrackerIds();
    //     for (uint8_t id : pairedIds) {
    //         if (!espnow.isTrackerIdConnected(id)) {
    //             sendDisconnectionStatus(id);
    //         }
    //     }
    // }

    static unsigned long lastDebugPrint = 0;
    unsigned long interval = now - lastSendAttempt;

    // Throttle to prevent overwhelming USB endpoint
    if (interval < minSendIntervalMs) return;

    // Get reference to ESPNow instance once
    auto &espnow = ESPNowCommunication::getInstance();
    size_t trackerCount = espnow.getConnectedTrackerCount();
    size_t availableReports = buffer.size();
    
    // Early exit if nothing to send
    if (availableReports == 0) {
        if (trackerCount == 0 || (now - lastRegistrationSent) < registrationIntervalMs) return;
        lastRegistrationSent = now;
    }

    // Prepare 64-byte transfer buffer (4 reports of 16 bytes each)
    uint8_t transferBuffer[hidTransferSize];
    size_t reportsWritten = 0;

    // Priority 1: Fill slots with tracker data from FIFO (up to 4 reports)
    size_t reportsToSend = std::min(availableReports, reportsPerTransfer);
    for (size_t i = 0; i < reportsToSend; i++) {
        Packet packet = buffer.shift();
        memcpy(&transferBuffer[reportsWritten * reportSize], packet.data, reportSize);
        reportsWritten++;
    }

    // Priority 2: Pad remaining slots with registration packets
    if (trackerCount > 0 && reportsWritten < reportsPerTransfer) {
        size_t registrationsToSend = reportsPerTransfer - reportsWritten;
        
        for (size_t i = 0; i < registrationsToSend; i++) {
            if (nextTrackerIndex >= trackerCount) {
                nextTrackerIndex = 0;
            }
            createRegistrationReport(&transferBuffer[reportsWritten * reportSize], nextTrackerIndex);
            nextTrackerIndex++;
            reportsWritten++;
        }
    }

    // Only send if we have reports to send
    if (reportsWritten > 0) {
        // Zero-fill any remaining bytes if not a full 64-byte transfer
        if (reportsWritten < reportsPerTransfer) {
            memset(&transferBuffer[reportsWritten * reportSize], 0, (reportsPerTransfer - reportsWritten) * reportSize);
        }
        
        lastSendAttempt = now;
        if (!hidDevice.send(transferBuffer, hidTransferSize)) {
            Serial.printf("[USB] Send failed at %lums\n", now);
        }
    }
}

PacketHandling PacketHandling::instance;
