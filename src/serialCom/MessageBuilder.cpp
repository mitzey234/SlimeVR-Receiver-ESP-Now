#include "MessageBuilder.h"
#include <cstring>
#include <array>
#include <algorithm>

namespace SlimeVR
{
    MessageBuilder::MessageBuilder() : buffer(nullptr), bufferSize(0), head(0)
    {
    }

    MessageBuilder::~MessageBuilder()
    {
        delete[] buffer;
        buffer = nullptr;
        bufferSize = 0;
        head = 0;
    }

    bool MessageBuilder::writeStringRaw(const std::string& str)
    {
        // Write string data
        return writeBytes(reinterpret_cast<const uint8_t*>(str.data()), str.length());
    }

    bool MessageBuilder::writeString(const std::string& str)
    {
        // Check if string length exceeds uint8_t max value
        if (str.length() > 255)
        {
            return false;
        }

        // Write string length as uint8_t first
        if (!writeUInt8(str.length()))
        {
            return false;
        }
        
        // Write string data
        return writeBytes(reinterpret_cast<const uint8_t*>(str.data()), str.length());
    }

    bool MessageBuilder::writeBytes(const uint8_t* data, uint16_t length)
    {
        if (!data)
        {
            return false;
        }
        
        // If buffer is nullptr, allocate initial buffer
        if (buffer == nullptr)
        {
            // Allocate initial size (256 bytes or the required size, whichever is larger)
            bufferSize = std::max(static_cast<uint16_t>(256), static_cast<uint16_t>(length + 64));
            buffer = new uint8_t[bufferSize];
            if (buffer == nullptr)
            {
                return false;
            }
            // Zero out the buffer
            std::memset(buffer, 0, bufferSize);
        }
        
        // Check if we need to expand the buffer
        if (head + length > bufferSize)
        {
            // Calculate new buffer size (double it or use what's needed + extra, whichever is larger)
            uint16_t newSize = std::max(
                static_cast<uint16_t>(bufferSize * 2),
                static_cast<uint16_t>(head + length + 64)
            );
            
            uint8_t* newBuffer = new uint8_t[newSize];
            if (newBuffer == nullptr)
            {
                return false;
            }
            
            // Zero out the new buffer
            std::memset(newBuffer, 0, newSize);
            
            // Copy existing data to new buffer
            if (head > 0)
            {
                std::memcpy(newBuffer, buffer, head);
            }
            
            // Free old buffer and update
            delete[] buffer;
            buffer = newBuffer;
            bufferSize = newSize;
        }
        
        // Copy data to buffer at current head position
        std::memcpy(buffer + head, data, length);
        head += length;
        
        return true;
    }

    bool MessageBuilder::writeUInt8(uint8_t value)
    {
        return writeBytes(&value, sizeof(uint8_t));
    }

    bool MessageBuilder::writeUInt16(uint16_t value)
    {
        uint8_t bytes[2];
        bytes[0] = static_cast<uint8_t>(value & 0xFF);
        bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        return writeBytes(bytes, sizeof(uint16_t));
    }

    bool MessageBuilder::writeUInt32(uint32_t value)
    {
        uint8_t bytes[4];
        bytes[0] = static_cast<uint8_t>(value & 0xFF);
        bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        return writeBytes(bytes, sizeof(uint32_t));
    }

    bool MessageBuilder::writeInt32(int32_t value)
    {
        uint8_t bytes[4];
        bytes[0] = static_cast<uint8_t>(value & 0xFF);
        bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        return writeBytes(bytes, sizeof(int32_t));
    }

    bool MessageBuilder::writeMacAddress(const uint8_t mac[6])
    {
        if (!mac)
        {
            return false;
        }
        return writeBytes(mac, 6);
    }

    bool MessageBuilder::writeTracker(ESPNowCommunication::Tracker tracker)
    {
        // Write MAC address (6 bytes)
        if (!writeMacAddress(tracker.mac.data()))
        {
            return false;
        }
        
        // Write tracker ID
        if (!writeUInt8(tracker.trackerId))
        {
            return false;
        }
        
        // Write missed pings
        if (!writeUInt8(tracker.missedPings))
        {
            return false;
        }
        
        // Write latency
        if (!writeUInt8(tracker.latency))
        {
            return false;
        }
        
        // Write RSSI
        if (!writeInt8(tracker.rssi))
        {
            return false;
        }

        // Write bytes per second
        if (!writeUInt16(tracker.bytesPerSecond))
        {
            return false;
        }

        // Write packets per second
        if (!writeUInt16(tracker.packetsPerSecond))
        {
            return false;
        }
        
        return true;
    }

    bool MessageBuilder::writePairedTracker(const uint8_t mac[6], uint8_t trackerId) {
        if (!writeMacAddress(mac)) {
            return false;
        }
        if (!writeUInt8(trackerId)) {
            return false;
        }
        return true;
    }

    bool MessageBuilder::writeInt8(int8_t value)
    {
        return writeBytes(reinterpret_cast<const uint8_t*>(&value), sizeof(int8_t));
    }

    bool MessageBuilder::writeInt16(int16_t value)
    {
        uint8_t bytes[2];
        bytes[0] = static_cast<uint8_t>(value & 0xFF);
        bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        return writeBytes(bytes, sizeof(int16_t));
    }

    bool MessageBuilder::writeBool(bool value)
    {
        uint8_t byte = value ? 1 : 0;
        return writeBytes(&byte, sizeof(uint8_t));
    }

    void MessageBuilder::print()
    {
        Serial.writeLine(buffer, head);
    }
}

