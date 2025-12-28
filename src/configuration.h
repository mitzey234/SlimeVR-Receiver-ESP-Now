#pragma once

#include <LittleFS.h>
#include <cstdint>

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
    static Configuration &getInstance();
    void setup();
    uint8_t getSavedTrackerCount();
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
    uint8_t getTrackerIdForMac(const uint8_t mac[6]);  // Returns existing or allocates new ID
    uint8_t allocateTrackerIdForMac(const uint8_t mac[6]);  // Internal: finds first available ID

private:
    Configuration() = default;

    static Configuration instance;

    static constexpr char savedTrackerCountPath[] = "/savedTrackerCount.bin";
    static constexpr char securityCodePath[] = "/securityCode.bin";
    static constexpr char pairedTrackersPath[] = "/pairedTrackers.bin";
    static constexpr char trackerIdsPath[] = "/trackerIds.bin";
};
