#pragma once

#include <LittleFS.h>
#include <cstddef>
#include <cstdint>
#include "Serial.h"

class Configuration {
public:
    // WiFi channel override
    void setWifiChannel(uint8_t channel);
    uint8_t getWifiChannel();
    static constexpr char wifiChannelPath[] = "/wifiChannel.bin";
    // Helper: iterate all paired trackers, calling a callback with mac and trackerId
    void forEachPairedTracker(std::function<void(const uint8_t mac[6], uint8_t trackerId)> cb);

    // Helper: get all paired tracker MACs and IDs as vectors
    std::vector<std::array<uint8_t, 6>> getAllPairedTrackerMacs();
    std::vector<uint8_t> getAllPairedTrackerIds();
    size_t getSavedTrackerCount();
    bool isPairedTrackerCapacityReached();
    static constexpr size_t maxPairedTrackers = 256;
    static Configuration &getInstance();
    void setup();
    void getSecurityCode(uint8_t securityCode[8]);
    void resetSecurityCode();
    
    // Tracker management
    bool isPairedTracker(const uint8_t mac[6]);
    void addPairedTracker(const uint8_t mac[6]);
    void removePairedTracker(const uint8_t mac[6]);
    void clearAllPairedTrackers();

    // Returns true if trackerId is present in persistent storage
    bool isTrackerIdInUse(uint8_t trackerId);
    
    // Tracker ID management (persistent)
    bool getTrackerIdForMac(const uint8_t mac[6], uint8_t &trackerId);  // Returns existing or allocates new ID
    bool allocateTrackerIdForMac(const uint8_t mac[6], uint8_t &trackerId);  // Internal: finds first available ID
    bool wifiChannelFileExists();

private:
    Configuration() = default;

    static Configuration instance;
    static constexpr char securityCodePath[] = "/securityCode.bin";
    static constexpr char pairedTrackersPath[] = "/pairedTrackers.bin";
    static constexpr char trackerIdsPath[] = "/trackerIds.bin";
};
