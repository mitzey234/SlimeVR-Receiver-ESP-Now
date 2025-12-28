#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// WEMOS D1 Mini ESP8266 Pin Definitions
// The built-in LED on D1 Mini is connected to GPIO2 (D4)
static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN // allow testing #ifdef LED_BUILTIN
static const bool LED_ACTIVE_LEVEL = false; // LED is active LOW on D1 Mini

// Flash button (GPIO0 / D3)
static const uint8_t USER_BUTTON = 0;
static const bool USER_BUTTON_ACTIVE_LEVEL = false;

// Boot mode pin - when pulled low, prevents USB/HID initialization
// D1 = GPIO5 on D1 Mini
static const uint8_t BOOT_MODE_PIN = 5;
static const bool BOOT_MODE_ACTIVE_LEVEL = false; // Active LOW (pulled down)

// D1 Mini Pin Mapping (for reference)
// D0  = GPIO16
// D1  = GPIO5  (SCL)
// D2  = GPIO4  (SDA)
// D3  = GPIO0  (Flash button)
// D4  = GPIO2  (Built-in LED)
// D5  = GPIO14 (SCK)
// D6  = GPIO12 (MISO)
// D7  = GPIO13 (MOSI)
// D8  = GPIO15 (CS)
// RX  = GPIO3
// TX  = GPIO1

#endif /* Pins_Arduino_h */
