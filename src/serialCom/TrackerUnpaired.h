#pragma once

#include "MessageBuilder.h"
#include "./SerialCom.h"
#include "../GlobalVars.h"

namespace SlimeVR
{
    namespace SerialComMessages
    {
        class TrackerUnpaired {
            public:
                static bool print(uint8_t mac[6], uint8_t trackerId) {
                    // Constructor can be used for initialization if needed
                    MessageBuilder builder;
                    builder.writeStringRaw(SlimeVR::SerialCom::getPrefix());
                    builder.writeUInt8(static_cast<uint8_t>(SerialComMessageTypes::TRACKER_UNPAIRED));
                    builder.writeMacAddress(mac);
                    builder.writeUInt8(trackerId);
                    builder.print();
                    builder.Destroy(); // Clean up buffer after use
                    return true;
                }
        };
    }
}