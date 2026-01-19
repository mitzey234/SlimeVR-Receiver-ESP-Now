#include "button.h"

#include "pins_arduino.h"

void IRAM_ATTR button_isr() {
    Button &button = Button::getInstance();
    detachInterrupt(USER_BUTTON);
    button.initDebouncing(true);
    button.polling = true;
}

Button &Button::getInstance() {
    return instance;
}

void Button::begin() {
    pinMode(USER_BUTTON,
            USER_BUTTON_ACTIVE_LEVEL ? INPUT_PULLDOWN : INPUT_PULLUP);
    attach();
}

void Button::update() {
    if (!polling) {
        return;
    }

    auto buttonState = isButtonPressed();
    auto elapsedMillis = millis() - lastButtonChangeMillis;

    // Require minimum debounce time (50ms) before recognizing state changes
    const unsigned long minDebounceTime = 50;

    // Detect button release (pressed -> not pressed)
    if (!buttonState && lastButtonState) {
        if (elapsedMillis >= minDebounceTime) {
            pressCount++;
            lastButtonChangeMillis = millis();
        }
    }

    // Detect button idle (not pressed for a while)
    if (!buttonState && !lastButtonState) {
        if (elapsedMillis < multiPressMaxDelaySeconds * 1e3) {
            lastButtonState = buttonState;
            return;
        }
        if (pressCount >= 1) {
            invokeMultiPressCallbacks(pressCount);
            pressCount = 0;
        }
        polling = false;
        attach();
        lastButtonState = buttonState;
        return;
    }

    // Detect long press (held down)
    if (buttonState && lastButtonState) {
        if (elapsedMillis < longPressSeconds * 1e3 || pressCount > 0) {
            lastButtonState = buttonState;
            return;
        }
        invokeLongPressCallbacks();
        pressCount = 0;
        polling = false;
        attach();
        lastButtonState = buttonState;
        return;
    }

    lastButtonState = buttonState;
}

void Button::initDebouncing(bool state) {
    circularBuffer = state ? 0xffff : 0x0000;
    lastButtonState = state;
    lastButtonChangeMillis = millis();
    pressCount = 0;
}

bool Button::isButtonPressed() {
    circularBuffer = (circularBuffer << 1)
                     | (digitalRead(USER_BUTTON) == USER_BUTTON_ACTIVE_LEVEL);

    uint8_t popCount = __builtin_popcount(circularBuffer);

    return popCount > 12;  // Require at least 13 out of 16 samples to be high
}

void Button::onLongPress(std::function<void()> callback) {
    longPressCallbacks.push_back(callback);
}

void Button::onMultiPress(std::function<void(size_t)> callback) {
    multiPressCallbacks.push_back(callback);
}

void Button::attach() {
    attachInterrupt(digitalPinToInterrupt(USER_BUTTON),
                    button_isr,
                    USER_BUTTON_ACTIVE_LEVEL ? RISING : FALLING);
}

void Button::invokeLongPressCallbacks() {
    for (auto &callback : longPressCallbacks) {
        callback();
    }
}

void Button::invokeMultiPressCallbacks(size_t pressCount) {
    for (auto &callback : multiPressCallbacks) {
        callback(pressCount);
    }
}

Button Button::instance;
