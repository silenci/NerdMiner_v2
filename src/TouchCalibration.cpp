#include "TouchCalibration.h"

#ifdef TOUCH_ENABLE

#include <Arduino.h>

TouchCalibrator::TouchCalibrator(TFT_eSPI& tft) : tft_(tft) {
    setupCalibrationPoints();
}

void TouchCalibrator::setupCalibrationPoints() {
    // Get screen dimensions
    uint16_t w = tft_.width();
    uint16_t h = tft_.height();
    
    // Define calibration points: 4 corners + center
    const uint16_t margin = 30;
    
    // Top-left
    points_[0] = {margin, margin, 0, 0};
    // Top-right  
    points_[1] = {w - margin, margin, 0, 0};
    // Bottom-right
    points_[2] = {w - margin, h - margin, 0, 0};
    // Bottom-left
    points_[3] = {margin, h - margin, 0, 0};
    // Center
    points_[4] = {w / 2, h / 2, 0, 0};
}

bool TouchCalibrator::calibrate(struct TouchCalibration* calibData, 
                                 bool (*getTouchFunc)(uint16_t*, uint16_t*)) {
    
    Serial.println("[TouchCalibration] Starting calibration process...");
    
    // Clear screen
    tft_.fillScreen(TFT_BLACK);
    showInstructions();
    delay(3000);
    
    // Collect calibration points
    for (int i = 0; i < NUM_POINTS; i++) {
        bool pointCaptured = false;
        unsigned long lastTouchTime = 0;
        const unsigned long DEBOUNCE_TIME = 500;
        
        // Draw calibration screen ONCE per point
        drawCalibrationScreen(i + 1, points_[i].screen_x, points_[i].screen_y);
        
        bool touching = false;
        uint16_t current_x = 0, current_y = 0;
        
        while (!pointCaptured) {
            uint16_t touch_x, touch_y;
            bool touchDetected = getTouchFunc(&touch_x, &touch_y);
            
            if (touchDetected && !touching) {
                // Touch started - store coordinates but don't capture yet
                touching = true;
                current_x = touch_x;
                current_y = touch_y;
                
                // Visual feedback - show touch detected (BRIGHT YELLOW)
                drawTarget(points_[i].screen_x, points_[i].screen_y, 0xFFE0);
                
            } else if (!touchDetected && touching) {
                // Touch released - now capture the point
                unsigned long currentTime = millis();
                if (currentTime - lastTouchTime > DEBOUNCE_TIME) {
                    points_[i].touch_x = current_x;
                    points_[i].touch_y = current_y;
                    
                    Serial.printf("[TouchCalibration] Point %d: Screen(%d,%d) -> Touch(%d,%d)\n",
                                i + 1, points_[i].screen_x, points_[i].screen_y,
                                current_x, current_y);
                    
                    // Visual feedback - show point captured (BRIGHT GREEN)
                    drawTarget(points_[i].screen_x, points_[i].screen_y, 0x07E0);
                    delay(800);
                    
                    pointCaptured = true;
                    lastTouchTime = currentTime;
                }
                touching = false;
            } else if (touchDetected && touching) {
                // Touch continues - update coordinates for better accuracy
                current_x = touch_x;
                current_y = touch_y;
            }
            
            delay(30); // Reduced delay for better responsiveness
        }
    }
    
    // Calculate calibration data
    uint16_t min_x = points_[0].touch_x;
    uint16_t max_x = points_[0].touch_x;
    uint16_t min_y = points_[0].touch_y;
    uint16_t max_y = points_[0].touch_y;
    
    for (int i = 1; i < NUM_POINTS; i++) {
        if (points_[i].touch_x < min_x) min_x = points_[i].touch_x;
        if (points_[i].touch_x > max_x) max_x = points_[i].touch_x;
        if (points_[i].touch_y < min_y) min_y = points_[i].touch_y;
        if (points_[i].touch_y > max_y) max_y = points_[i].touch_y;
    }
    
    // Store calibration results
    calibData->min_x = min_x;
    calibData->max_x = max_x;
    calibData->min_y = min_y;
    calibData->max_y = max_y;
    calibData->calibrated = true;
    
    Serial.printf("[TouchCalibration] Results: X(%d-%d) Y(%d-%d)\n", 
                  min_x, max_x, min_y, max_y);
    
    // Show results
    showResults(calibData);
    delay(3000);
    
    // Force screen redraw when calibration ends
    tft_.fillScreen(0x0000); // Pure black
    
    // Set flag to force background redraw on next screen update
    extern bool hasChangedScreen;
    hasChangedScreen = true;
    
    return true;
}

void TouchCalibrator::drawCalibrationScreen(int step, uint16_t x, uint16_t y) {
    // Ensure completely black background
    tft_.fillScreen(0x0000); // Pure black (RGB565)
    
    // Title with high contrast
    tft_.setTextColor(0xFFFF, 0x0000); // White text on black
    tft_.setTextSize(2);
    tft_.setCursor(10, 10);
    tft_.printf("Touch Calibration");
    
    // Step info with high contrast
    tft_.setTextColor(0x07FF, 0x0000); // Cyan text on black
    tft_.setTextSize(1);
    tft_.setCursor(10, 40);
    tft_.printf("Step %d of %d", step, NUM_POINTS);
    
    // Instructions with high contrast
    tft_.setTextColor(0xFFE0, 0x0000); // Yellow text on black
    tft_.setCursor(10, 60);
    tft_.printf("Touch and release the target");
    
    // Draw target with bright red
    drawTarget(x, y, 0xF800); // Bright red (RGB565)
}

void TouchCalibrator::drawTarget(uint16_t x, uint16_t y, uint16_t color) {
    const uint16_t size = 20; // Increased size for better visibility
    
    // Draw thick crosshair with multiple lines for better visibility
    for (int i = -1; i <= 1; i++) {
        tft_.drawLine(x - size, y + i, x + size, y + i, color);
        tft_.drawLine(x + i, y - size, x + i, y + size, color);
    }
    
    // Draw multiple circles for better visibility
    tft_.drawCircle(x, y, size/2, color);
    tft_.drawCircle(x, y, size/2 + 1, color);
    tft_.drawCircle(x, y, size/3, color);
    tft_.drawCircle(x, y, size/3 + 1, color);
    
    // Add a center dot
    tft_.fillCircle(x, y, 2, color);
}

void TouchCalibrator::showInstructions() {
    tft_.fillScreen(TFT_BLACK);
    tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft_.setTextSize(2);
    tft_.setCursor(10, 50);
    tft_.printf("Touch Calibration");
    
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.setTextSize(1);
    tft_.setCursor(10, 90);
    tft_.printf("You will touch 5 targets");
    tft_.setCursor(10, 110);
    tft_.printf("to calibrate the touch screen.");
    
    tft_.setCursor(10, 140);
    tft_.printf("Touch each target accurately");
    tft_.setCursor(10, 160);
    tft_.printf("when it appears.");
    
    tft_.setCursor(10, 190);
    tft_.printf("Starting in 3 seconds...");
}

void TouchCalibrator::showResults(struct TouchCalibration* calibData) {
    tft_.fillScreen(TFT_BLACK);
    tft_.setTextColor(TFT_GREEN, TFT_BLACK);
    tft_.setTextSize(2);
    tft_.setCursor(10, 30);
    tft_.printf("Calibration Complete!");
    
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.setTextSize(1);
    tft_.setCursor(10, 70);
    tft_.printf("Touch ranges:");
    
    tft_.setCursor(10, 90);
    tft_.printf("X: %d - %d", calibData->min_x, calibData->max_x);
    
    tft_.setCursor(10, 110);
    tft_.printf("Y: %d - %d", calibData->min_y, calibData->max_y);
    
    tft_.setCursor(10, 140);
    tft_.printf("Center: %d, %d", calibData->getCenterX(), calibData->getCenterY());
    
    tft_.setCursor(10, 170);
    tft_.printf("Calibration saved!");
    
    tft_.setCursor(10, 200);
    tft_.printf("Returning to normal mode...");
}

#endif // TOUCH_ENABLE