#include "TemperatureCommand.h"

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

bool handleTemperatureCommand(const String &command) {
    if (!command.equalsIgnoreCase("temperature")) {
        return false;
    }

    float tempC = temperatureRead(); // Convert to Celsius
    
    Serial.printf("[CMD] Chip temperature: %.2f°C\n", tempC);
    return true;
}
