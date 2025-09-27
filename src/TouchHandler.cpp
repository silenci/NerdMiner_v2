
#include "drivers/devices/device.h"
#ifdef TOUCH_ENABLE
#include "TouchHandler.h"

// Configurable touch debounce timing (can be overridden in build_flags)
#ifndef TOUCH_DEBOUNCE_MS
  #define TOUCH_DEBOUNCE_MS 2000 // Default debounce time in milliseconds
#endif

TouchHandler::~TouchHandler() {
}

TouchHandler::TouchHandler(TFT_eSPI& tft, uint8_t csPin, uint8_t irqPin, SPIClass& spi)
  : tft(tft), csPin(csPin), irqPin(irqPin), spi(spi), lastTouchTime(0),
  screenSwitchCallback(nullptr), screenSwitchAltCallback(nullptr)
#ifndef ESP32_2432S024R
  , touch(spi, csPin, irqPin)
#endif
{

}

void TouchHandler::begin(uint16_t xres, uint16_t yres) {
#ifdef ESP32_2432S024R
    // For ESP32-2432S024R, touch is initialized in display driver
    // Nothing to do here
    Serial.println("[TouchHandler] Using ESP32-2432S024R external touch function");
#else
    spi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI);
    touch.begin(xres, yres);
#endif
}

void TouchHandler::setScreenSwitchCallback(void (*callback)()) {
  screenSwitchCallback = callback;
}

void TouchHandler::setScreenSwitchAltCallback(void (*callback)()) {
  screenSwitchAltCallback = callback;
}


uint16_t TouchHandler::isTouched() {
  // XXX - move touch_x, touch_y to private and min_x, min_y,max_x, max_y
  uint16_t touch_x, touch_y, code = 0;
  bool touched = false;

#ifdef ESP32_2432S024R
  // Use external touch function for ESP32-2432S024R
  touched = esp32_2432S024R_getTouch(&touch_x, &touch_y);
#else
  // Use original method for other devices
  if (touch.pressed()) {
    touched = true;
    touch_x = touch.RawX();
    touch_y = touch.RawY();
  }
#endif

  if (touched) {
#ifdef DISABLE_TOUCH_ZONES
    // When touch zones are disabled, any touch triggers the same action
#ifdef ESP32_2432S024R
    // For ESP32-2432S024R, validate coordinates
    if (touch_x >= 0 && touch_x <= 240 && touch_y >= 0 && touch_y <= 320) {
#endif
      Serial.printf("Touch at: x=%d, y=%d - switching display state\n", touch_x, touch_y);
      code = 1;
      if (debounce() && screenSwitchCallback) {
        screenSwitchCallback();  // This should be alternateScreenState
      }
#ifdef ESP32_2432S024R
    }
#endif
#else
    // Zone-based touch behavior - divide screen in zones
    if (touch_x < 200 + (1700 - 200) / 4) {
      // bottom
      code = 1;
      if (debounce() && screenSwitchAltCallback) {
        screenSwitchAltCallback();
      }
    } else {
      // top
      code = 2;
      if (debounce() && screenSwitchCallback) {
        screenSwitchCallback();
      }
    }

    if (code) {
      if (code == 1)
        Serial.print("Touch bottom\n");
      else
        Serial.print("Touch top\n");
    }
#endif
  }
  return code;
}

bool TouchHandler::debounce() {
  unsigned long currentTime = millis();
  if (currentTime - lastTouchTime >= TOUCH_DEBOUNCE_MS) {
    lastTouchTime = currentTime;
    return true;
  }
  return false;
}
#endif