#include "MessageBuilder.h"
#include "./SerialCom.h"
#include "../GlobalVars.h"

namespace SlimeVR
{
    namespace SerialComMessages
    {
        class Ident {
            public:
                static bool print() {
                    // Constructor can be used for initialization if needed
                    MessageBuilder builder;
                    builder.writeStringRaw(SlimeVR::SerialCom::getPrefix());
                    builder.writeString(USB_PRODUCT);
                    builder.writeString(FIRMWARE_VERSION);
                    builder.writeBool(espnow.isInPairingMode());
                    builder.writeBool(espnow.isScanningEnvironment());
                    builder.writeUInt8(espnow.getConnectedTrackerCount());
                    for (size_t i = 0; i < espnow.getConnectedTrackerCount(); i++) {
                        builder.writeTracker(*espnow.getTrackerByIndex(i)); // Write tracker info to message
                    }
                    builder.print();
                    builder.Destroy(); // Clean up buffer after use
                    return true;
                }
        };
    }
}