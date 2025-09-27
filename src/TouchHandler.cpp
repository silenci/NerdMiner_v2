
#include "drivers/devices/device.h"
#ifdef TOUCH_ENABLE
#include "TouchHandler.h"
#include "TouchCalibration.h"
#include "drivers/storage/nvMemory.h"

// Configurable touch debounce timing (can be overridden in build_flags)
#ifndef TOUCH_DEBOUNCE_MS
  #define TOUCH_DEBOUNCE_MS 2000 // Default debounce time in milliseconds
#endif



TouchHandler::~TouchHandler() {
}

TouchHandler::TouchHandler(TFT_eSPI& tft, uint8_t csPin, uint8_t irqPin, SPIClass& spi)
  : tft(tft), csPin(csPin), irqPin(irqPin), spi(spi), lastTouchTime(0),
  screenSwitchCallback(nullptr), screenSwitchAltCallback(nullptr), lastTouchState_(false),
  resetTouchStateRequested_(false)
#ifndef ESP32_2432S024R
  , touch(spi, csPin, irqPin)
#endif
{

}

void TouchHandler::begin(uint16_t xres, uint16_t yres) {
#ifdef ESP32_2432S024R
    // For ESP32-2432S024R, touch is initialized in display driver
    // Nothing to do here
  // ...existing code...
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
  
  // Static variables to store last valid touch coordinates
  static uint16_t lastValidTouchX = 0;
  static uint16_t lastValidTouchY = 0;

#ifdef ESP32_2432S024R
  // Use external touch function for ESP32-2432S024R
  touched = esp32_2432S024R_getTouch(&touch_x, &touch_y);
  
  // Store last valid coordinates when touch is detected
  if (touched) {
    lastValidTouchX = touch_x;
    lastValidTouchY = touch_y;
  } else {
    // Use last valid coordinates when touch is released
    touch_x = lastValidTouchX;
    touch_y = lastValidTouchY;
  }
#else
  // Use original method for other devices
  if (touch.pressed()) {
    touched = true;
    touch_x = touch.RawX();
    touch_y = touch.RawY();
    lastValidTouchX = touch_x;
    lastValidTouchY = touch_y;
  } else {
    touch_x = lastValidTouchX;
    touch_y = lastValidTouchY;
  }
#endif
  
  // Update touch state for checkCalibrationTrigger()
  lastTouchState_ = touched;
  
  // UNIFIED TOUCH LOGIC: Handle both screen switching AND calibration trigger
  static bool wasTouched = false;
  static unsigned long touchStartTime = 0;
  
  // Reset state if requested via member flag  
  if (resetTouchStateRequested_) {
    wasTouched = false;
    touchStartTime = 0;
    lastValidTouchX = 0;
    lastValidTouchY = 0;
    resetTouchStateRequested_ = false;
  // ...existing code...
  }
  
  // 1. First check for calibration trigger (10 second hold)
  if (TouchCalibrator::isCalibrationTrigger(touched)) {
  // ...existing code...
    return 999;  // Special code for calibration trigger
  }
  
  // 2. Handle touch-to-screen-switch logic: only switch on touch release for short touches
  
  if (touched && !wasTouched) {
    // Touch just started
    touchStartTime = millis();
    wasTouched = true;
  } else if (!touched && wasTouched) {
    // Touch just released
    unsigned long touchDuration = millis() - touchStartTime;
    wasTouched = false;
    
  // ...existing code...
    
    // Only switch screen for short touches (less than 2 seconds)
    // This prevents screen switching during calibration trigger (10s hold)
    if (touchDuration < 2000) {
#ifdef DISABLE_TOUCH_ZONES
      // When touch zones are disabled, any short touch release triggers screen switch
#ifdef ESP32_2432S024R
      // For ESP32-2432S024R, validate coordinates from last touch using actual screen dimensions
      uint16_t screenWidth = tft.width();
      uint16_t screenHeight = tft.height();
  // ...existing code...
      if (touch_x >= 0 && touch_x <= screenWidth && touch_y >= 0 && touch_y <= screenHeight) {
#endif
        bool debounceOK = debounce();
  // ...existing code...
        code = 1;
        if (debounceOK && screenSwitchCallback) {
          // ...existing code...
          screenSwitchCallback();  // This should be alternateScreenState
        } else {
          // ...existing code...
        }
#ifdef ESP32_2432S024R
      } else {
  // ...existing code...
      }
#endif
#else
      // Zone-based touch behavior - divide screen in zones using calibration
      uint16_t threshold;
      if (touchCalibration_.calibrated) {
        // Use calibrated center point
        threshold = touchCalibration_.getCenterX();
      } else {
        // Fallback to old hardcoded values (for compatibility)
        threshold = 200 + (1700 - 200) / 4;
      }
      
      if (touch_x < threshold) {
        // bottom/left zone
        code = 1;
        if (debounce() && screenSwitchAltCallback) {
          screenSwitchAltCallback();
        }
  // ...existing code...
      } else {
        // top/right zone
        code = 2;
        if (debounce() && screenSwitchCallback) {
          screenSwitchCallback();
        }
  // ...existing code...
      }
#endif
    } // End of touchDuration < 2000 check
  } // End of touch release (!touched && wasTouched)
  
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

void TouchHandler::setTouchCalibration(const struct TouchCalibration& calibration) {
  touchCalibration_ = calibration;
  // ...existing code...
}

// checkCalibrationTrigger() is now integrated into isTouched() for unified touch handling

bool TouchHandler::performCalibration() {
  // ...existing code...
  
  // Create calibrator instance
  TouchCalibrator calibrator(tft);
  
  // Define touch reading function for the calibrator
  auto getTouchFunc = [](uint16_t* x, uint16_t* y) -> bool {
#ifdef ESP32_2432S024R
    return esp32_2432S024R_getTouch(x, y);
#else
    // For other devices using XPT2046 - this would need to be implemented
    return false;
#endif
  };
  
  // Perform calibration
  struct TouchCalibration newCalib;
  if (calibrator.calibrate(&newCalib, getTouchFunc)) {
    // Save calibration to local instance
    touchCalibration_ = newCalib;
    
    // Load current settings first to preserve existing configuration
    extern TSettings Settings;
    extern nvMemory nvMem;
    
    // Create a copy of current settings to avoid corrupting global state
    TSettings currentSettings;
    if (nvMem.loadConfig(&currentSettings)) {
      // Only update the touch calibration part
      currentSettings.touchCalibration = newCalib;
      
      // Save back with all data preserved
      if (nvMem.saveConfig(&currentSettings)) {
  // ...existing code...
        // Update global settings with new calibration
        Settings.touchCalibration = newCalib;
        return true;
      } else {
  // ...existing code...
        return false;
      }
    } else {
  // ...existing code...
      return false;
    }
  }
  
  // ...existing code...
  return false;
}

void TouchHandler::resetTouchState() {
  // ...existing code...
  resetTouchStateRequested_ = true;
}

#endif