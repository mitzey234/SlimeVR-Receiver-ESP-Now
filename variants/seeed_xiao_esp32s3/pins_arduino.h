#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#ifdef __cplusplus
// C++ has bool built-in
#else
#include <stdbool.h>
#endif

static const uint8_t USER_BUTTON = 0;
static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

static const uint8_t SS = 44;
static const uint8_t MOSI = 9;
static const uint8_t MISO = 8;
static const uint8_t SCK = 7;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A8 = 7;
static const uint8_t A9 = 8;
static const uint8_t A10 = 9;

static const uint8_t D0 = 1;
static const uint8_t D1 = 2;
static const uint8_t D2 = 3;
static const uint8_t D3 = 4;
static const uint8_t D4 = 5;
static const uint8_t D5 = 6;
static const uint8_t D6 = 43;
static const uint8_t D7 = 44;
static const uint8_t D8 = 7;
static const uint8_t D9 = 8;
static const uint8_t D10 = 9;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;

#define USB_VID          0x1209
#define USB_PID          0x7690
#define USB_MANUFACTURER "SlimeVR"
#define USB_PRODUCT      "SlimeVR ESPNow Dongle"
#define USB_SERIAL       "SVRDGA1B2C3D4E5F6"

// Default USB FirmwareMSC Settings
#define USB_FW_MSC_VENDOR_ID        "ESP32-S2"     // max 8 chars
#define USB_FW_MSC_PRODUCT_ID       "Firmware MSC" // max 16 chars
#define USB_FW_MSC_PRODUCT_REVISION "1.23"         // max 4 chars
#define USB_FW_MSC_VOLUME_NAME      "S2-Firmware"  // max 11 chars
#define USB_FW_MSC_SERIAL_NUMBER    0x00000000

static const bool USER_BUTTON_ACTIVE_LEVEL = false;

static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
static const bool LED_ACTIVE_LEVEL = 0;

#endif /* Pins_Arduino_h */