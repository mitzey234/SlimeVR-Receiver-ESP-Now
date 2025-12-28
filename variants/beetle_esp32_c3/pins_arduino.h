#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Default USB Settings
#define USB_VID          0x303A
#define USB_PID          0x1001
#define USB_MANUFACTURER "DFRobot"
#define USB_PRODUCT      "Beetle ESP32-C3"
#define USB_SERIAL       ""

// DFRobot Beetle ESP32-C3 Pin Definitions
// Built-in RGB LED is on GPIO10 (WS2812)
static const uint8_t LED_BUILTIN = 10;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN
static const bool LED_ACTIVE_LEVEL = true;

// Boot button (GPIO9)
static const uint8_t USER_BUTTON = 9;
static const bool USER_BUTTON_ACTIVE_LEVEL = false;

// Boot mode pin - when pulled low, prevents USB/HID initialization
// D1 on Beetle C3 is typically GPIO5
static const uint8_t BOOT_MODE_PIN = 5;
static const bool BOOT_MODE_ACTIVE_LEVEL = false; // Active LOW (pulled down)

// Beetle ESP32-C3 Pin Mapping
// GPIO0  = A0 (ADC1_CH0)
// GPIO1  = A1 (ADC1_CH1)
// GPIO2  = A2 (ADC1_CH2)
// GPIO3  = A3 (ADC1_CH3)
// GPIO4  = A4 (ADC1_CH4, SDA)
// GPIO5  = A5 (ADC2_CH0, SCL)
// GPIO6  = TX
// GPIO7  = RX
// GPIO8  = SCK
// GPIO9  = Boot Button
// GPIO10 = RGB LED (WS2812)
// GPIO20 = UART_RX
// GPIO21 = UART_TX

static const uint8_t TX = 6;
static const uint8_t RX = 7;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS   = 7;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 2;
static const uint8_t SCK  = 8;

#endif /* Pins_Arduino_h */
