#ifndef _TOUCHCALIBRATION_H_
#define _TOUCHCALIBRATION_H_

#ifdef TOUCH_ENABLE

#include <TFT_eSPI.h>
#include "drivers/storage/storage.h"

class TouchCalibrator {
public:
    TouchCalibrator(TFT_eSPI& tft);
    
    // Start calibration process and return calibration data
    bool calibrate(struct TouchCalibration* calibData, 
                   bool (*getTouchFunc)(uint16_t*, uint16_t*));
    
    // Draw calibration screen
    void drawCalibrationScreen(int step, uint16_t x, uint16_t y);
    
    // Static method to check if calibration should be triggered (touch only for 10 seconds)
  static void resetCalibrationTriggerState() {
    // Reset the state by calling isCalibrationTrigger with resetState=true
    isCalibrationTrigger(false, true);
  }
  
  static bool isCalibrationTrigger(bool touchPressed, bool resetState = false) {
    static unsigned long triggerStartTime = 0;
    static bool wasTriggering = false;
    static unsigned long lastProgressUpdate = 0;
    
    // Reset state if requested
    if (resetState) {
      triggerStartTime = 0;
      wasTriggering = false;
      lastProgressUpdate = 0;
      // Serial.println("[TouchCalibration] Calibration trigger state reset");
      return false;
    }
    
    if (touchPressed && !wasTriggering) {
      // Just started touching
      triggerStartTime = millis();
      // Serial.println("Hold touch for 10 seconds to start calibration...");
    } else if (!touchPressed && wasTriggering) {
      // Stopped touching
      triggerStartTime = 0;
      // Serial.println("Touch released - calibration cancelled");
    }
    
    // Show progress every second while holding
    if (touchPressed && triggerStartTime > 0) {
      unsigned long elapsed = millis() - triggerStartTime;
      if (elapsed - lastProgressUpdate >= 1000) {
        int secondsLeft = 10 - (elapsed / 1000);
        if (secondsLeft > 0) {
          // Serial.printf("Calibration in %d seconds...\n", secondsLeft);
        }
        lastProgressUpdate = elapsed;
      }
    }
    
    wasTriggering = touchPressed;
    
    // Return true if we've been touching for 10 seconds
    return touchPressed && (millis() - triggerStartTime >= 10000);
  }

private:
    TFT_eSPI& tft_;
    
    // Calibration points (corners + center)
    static const int NUM_POINTS = 5;
    struct CalibrationPoint {
        uint16_t screen_x, screen_y;  // Screen coordinates
        uint16_t touch_x, touch_y;    // Touch coordinates
    };
    
    CalibrationPoint points_[NUM_POINTS];
    
    void setupCalibrationPoints();
    void drawTarget(uint16_t x, uint16_t y, uint16_t color);
    void showInstructions();
    void showResults(struct TouchCalibration* calibData);
};

#endif // TOUCH_ENABLE
#endif // _TOUCHCALIBRATION_H_