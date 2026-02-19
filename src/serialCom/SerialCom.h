#pragma once

#include <cstdint>
#include <string>

enum class SerialComMessageTypes : uint8_t 
{
        IDENT,    // Used mostly for when the dongle manager is asking the dongle to ident itself
        TRACKER_CONNECTED,
        TRACKER_DISCONNECTED,
        PAIRING_MODE,
        ENVIRONMENT_SCAN_MODE,
        ENVIRONMENT_SCAN_PROGRESS,
        ENVIRONMENT_SCAN_RESULT,
        PAIRED_TRACKERS_LIST,
        TRACKER_PAIRED,
        TRACKER_UNPAIRED
};

namespace SlimeVR
{
  class SerialCom
  {
    public:
        static SerialCom &getInstance() { return Singleton; }
    
        static std::string getPrefix () { return "[SC]"; }
        static bool comEnabled() { return Singleton.enabled; }
    
    private:
        static SerialCom Singleton;
        SerialCom() = default;
        bool enabled = false;
  };
}