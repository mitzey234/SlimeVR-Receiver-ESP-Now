#include "Serial.h"

#if !(defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT)
USBCDC USBSerial;
#endif
HybridSerial Serial;
