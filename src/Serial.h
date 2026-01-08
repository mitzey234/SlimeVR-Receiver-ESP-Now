#include <Arduino.h>

#undef Serial  // Remove the core's Serial definition


#ifndef SERIAL_H
#define SERIAL_H
extern USBCDC USBSerial;

class HybridSerial : public Stream {
private:
    HardwareSerial* uart;
    USBCDC* usb;
    
public:
    HybridSerial() : uart(&Serial0), usb(&USBSerial) {}
    
    void begin(unsigned long baud = 115200) {
        uart->begin(baud);
        usb->begin();
    }
    
    void beginUSB() {
        usb->begin();
    }
    
    // Write to both USB and UART
    size_t write(uint8_t c) override {
        size_t n = 0;
        n += uart->write(c);
        // Only write to USB if connected
        if (usb && usb->availableForWrite()) {
            n += usb->write(c);
        }
        return n;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        size_t n = 0;
        n += uart->write(buffer, size);
        // Only write to USB if connected
        if (usb && usb->availableForWrite()) {
            n += usb->write(buffer, size);
        }
        return n;
    }
    
    // Read from both (USB has priority, then UART)
    int available() override {
        int n = usb->available();
        if (n > 0) return n;
        return uart->available();
    }
    
    int read() override {
        if (usb->available()) {
            return usb->read();
        }
        return uart->read();
    }
    
    int peek() override {
        if (usb->available()) {
            return usb->peek();
        }
        return uart->peek();
    }
    
    void flush() override {
        uart->flush();
        usb->flush();
    }
    
    // Expose operator bool for connection checking
    operator bool() const {
        return *usb || *uart;
    }
};

#endif

extern HybridSerial Serial;
