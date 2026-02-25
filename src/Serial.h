#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#undef Serial  // Remove the core's Serial definition


#ifndef SERIAL_H
#define SERIAL_H
extern USBCDC USBSerial;

class HybridSerial : public Stream {
private:
    HardwareSerial* uart;
    USBCDC* usb;
    SemaphoreHandle_t writeMutex;
    StaticSemaphore_t mutexBuffer;
    
    // Internal unlocked write helpers
    size_t writeUnlocked(uint8_t c) {
        size_t n = 0;
        n += uart->write(c);
        if (usb && usb->availableForWrite() > 0) {
            n += usb->write(c);
        }
        return n;
    }
    
    size_t writeUnlocked(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        n += uart->write(buffer, size);
        if (usb && usb->availableForWrite() > 0) {
            n += usb->write(buffer, size);
        }
        return n;
    }
    
public:
    HybridSerial() : uart(&Serial0), usb(&USBSerial) {
        writeMutex = xSemaphoreCreateMutexStatic(&mutexBuffer);
    }
    
    void begin(unsigned long baud = 115200) {
        uart->begin(baud);
        usb->begin();
    }
    
    void beginUSB() {
        usb->begin();
    }
    
    size_t write(uint8_t c) override {
        if (writeMutex && xSemaphoreTake(writeMutex, portMAX_DELAY) == pdTRUE) {
            size_t n = writeUnlocked(c);
            xSemaphoreGive(writeMutex);
            return n;
        }
        return 0;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        if (writeMutex && xSemaphoreTake(writeMutex, portMAX_DELAY) == pdTRUE) {
            size_t n = writeUnlocked(buffer, size);
            xSemaphoreGive(writeMutex);
            return n;
        }
        return 0;
    }
    
    size_t printf(const char *format, ...) {
        if (!writeMutex || xSemaphoreTake(writeMutex, portMAX_DELAY) != pdTRUE) {
            return 0;
        }
        
        va_list args;
        va_start(args, format);
        char buffer[256];
        int len = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        size_t n = 0;
        if (len > 0) {
            n = writeUnlocked((const uint8_t*)buffer, len);
        }
        
        xSemaphoreGive(writeMutex);
        return n;
    }

    size_t writeLine(const uint8_t *buffer, size_t size) {
        if (writeMutex && xSemaphoreTake(writeMutex, portMAX_DELAY) == pdTRUE) {
            size_t n = writeUnlocked(buffer, size);
            n += writeUnlocked((const uint8_t*)"\r\n", 2);
            uart->flush();
            if (usb) usb->flush();
            xSemaphoreGive(writeMutex);
            return n;
        }
        return 0;
    }

    size_t writeLine(const char* s) {
        return writeLine((const uint8_t*)s, strlen(s));
    }
    
    size_t println() {
        return writeLine((const uint8_t*)"", 0);
    }
    
    size_t println(const char* s) {
        return writeLine(s);
    }
    
    size_t println(const String& s) {
        return println(s.c_str());
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
        if (writeMutex && xSemaphoreTake(writeMutex, portMAX_DELAY) == pdTRUE) {
            uart->flush();
            if (usb) usb->flush();
            xSemaphoreGive(writeMutex);
        }
    }
    
    // Expose operator bool for connection checking
    operator bool() const {
        return *usb || *uart;
    }
};

#endif

extern HybridSerial Serial;
