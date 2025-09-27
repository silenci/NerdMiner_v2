
#include <Wire.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_task_wdt.h>
#include <OneButton.h>

#include "mbedtls/md.h"
#include "wManager.h"
#include "mining.h"
#include "monitor.h"
#include "drivers/displays/display.h"
#include "drivers/storage/SDCard.h"
#include "ShaTests/nerdSHA_HWTest.h"
#include "timeconst.h"

#ifdef TOUCH_ENABLE
#include "TouchHandler.h"
#include "TouchCalibration.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"
#endif



#include <soc/soc_caps.h>
//#define HW_SHA256_TEST

//3 seconds WDT
#define WDT_TIMEOUT 3
//15 minutes WDT for miner task
#define WDT_MINER_TIMEOUT 900

#ifdef PIN_BUTTON_1
  OneButton button1(PIN_BUTTON_1);
#endif

#ifdef PIN_BUTTON_2
  OneButton button2(PIN_BUTTON_2);
#endif

#ifdef TOUCH_ENABLE
extern TouchHandler touchHandler;
#ifdef ESP32_2432S024R
extern bool esp32_2432S024R_getTouch(uint16_t *x, uint16_t *y);
#endif
#endif

extern monitor_data mMonitor;

#ifdef TOUCH_ENABLE
extern TSettings Settings;
extern nvMemory nvMem;
extern void alternateScreenState();
#endif

#ifdef SD_ID
  SDCard SDCrd = SDCard(SD_ID);
#else  
  SDCard SDCrd = SDCard();
#endif

/**********************⚡ GLOBAL Vars *******************************/

unsigned long start = millis();
const char* ntpServer = "pool.ntp.org";

//void runMonitor(void *name);


/********* INIT *****/
#include "monitor.h"
#include "utils.h"
#include "version.h"

// Watchdog task to monitor the monitor task health
void runWatchdog(void *name) {
  Serial.println("[WATCHDOG] started");
  
  const unsigned long WATCHDOG_TIMEOUT = 120000; // 2 minutes
  
  while (1) {
    unsigned long currentTime = millis();
    
    // Check if monitor task has been inactive for too long
    if (currentTime - lastMonitorActivity > WATCHDOG_TIMEOUT) {
      Serial.println("[WATCHDOG] Monitor task appears frozen, restarting ESP32...");
      ESP.restart();
    }
    
    // Check every 30 seconds
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

void setup()
{
      //Init pin 15 to eneble 5V external power (LilyGo bug)
  #ifdef PIN_ENABLE5V
      pinMode(PIN_ENABLE5V, OUTPUT);
      digitalWrite(PIN_ENABLE5V, HIGH);
  #endif

#ifdef MONITOR_SPEED
    Serial.begin(MONITOR_SPEED);
#else
    Serial.begin(115200);
#endif //MONITOR_SPEED

  Serial.setTimeout(0);
  delay(SECOND_MS/10);

  esp_task_wdt_init(WDT_MINER_TIMEOUT, true);
  // Idle task that would reset WDT never runs, because core 0 gets fully utilized
  disableCore0WDT();
  //disableCore1WDT();

#ifdef HW_SHA256_TEST
  while (1) HwShaTest();
#endif

  // Setup the buttons
#ifdef DISABLE_SCREEN_SWITCHING
  // Simple button behavior - only screen backlight toggle
  #ifdef PIN_BUTTON_1
    button1.setPressMs(5*SECOND_MS);
    button1.attachClick(alternateScreenState); // Simple backlight toggle
    button1.attachLongPressStart(reset_configuration);
  #endif
  #ifdef PIN_BUTTON_2
    button2.setPressMs(5*SECOND_MS);
    button2.attachClick(alternateScreenState); // Simple backlight toggle
    button2.attachLongPressStart(reset_configuration);
  #endif
#else
  // Full button functionality
  #if defined(PIN_BUTTON_1) && !defined(PIN_BUTTON_2) //One button device
    button1.setPressMs(5*SECOND_MS);
    button1.attachClick(switchToNextScreen);
    button1.attachDoubleClick(alternateScreenRotation);
    button1.attachLongPressStart(reset_configuration);
    button1.attachMultiClick(alternateScreenState);
  #endif

  #if defined(PIN_BUTTON_1) && defined(PIN_BUTTON_2) //Button 1 of two button device
    button1.setPressMs(5*SECOND_MS);
    button1.attachClick(alternateScreenState);
    button1.attachDoubleClick(alternateScreenRotation);
  #endif

  #if defined(PIN_BUTTON_2) //Button 2 of two button device
    button2.setPressMs(5*SECOND_MS);
    button2.attachClick(switchToNextScreen);
    button2.attachLongPressStart(reset_configuration);
  #endif
#endif

  /******** INIT NERDMINER ************/
  Serial.println("NerdMiner v2 starting......");

  /******** INIT DISPLAY ************/
  initDisplay();
  
  /******** PRINT INIT SCREEN *****/
  drawLoadingScreen();
  delay(2*SECOND_MS);

  /******** SHOW LED INIT STATUS (devices without screen) *****/
  mMonitor.NerdStatus = NM_waitingConfig;
  doLedStuff(0);

#ifdef SDMMC_1BIT_FIX
  SDCrd.initSDcard();
#endif

  /******** INIT WIFI ************/
  init_WifiManager();

  /******** CREATE TASK TO PRINT SCREEN *****/
  //tft.pushImage(0, 0, MinerWidth, MinerHeight, MinerScreen);
  // Higher prio monitor task
  Serial.println("");
  Serial.println("Initiating tasks...");
  
  /******** CREATE WATCHDOG TASK *****/
  BaseType_t resWatchdog = xTaskCreatePinnedToCore(runWatchdog, "Watchdog", 2048, NULL, 1, NULL, 0);
  if (resWatchdog != pdPASS) {
    Serial.println("ERROR creating Watchdog task");
  }
  
  static const char monitor_name[] = "(Monitor)";
  #if defined(CONFIG_IDF_TARGET_ESP32)
  // Increased stack for ESP32 classic due to NVS operations  
  BaseType_t res1 = xTaskCreatePinnedToCore(runMonitor, "Monitor", 9500, (void*)monitor_name, 5, NULL,1);
  #else
  BaseType_t res1 = xTaskCreatePinnedToCore(runMonitor, "Monitor", 10000, (void*)monitor_name, 5, NULL,1);
  #endif

  /******** CREATE STRATUM TASK *****/
  static const char stratum_name[] = "(Stratum)";
 #if defined(CONFIG_IDF_TARGET_ESP32) && !defined(ESP32_2432S028R) && !defined(ESP32_2432S028_2USB) && !defined(ESP32_2432S024R)
  // Reduced stack for ESP32 classic to save memory
  BaseType_t res2 = xTaskCreatePinnedToCore(runStratumWorker, "Stratum", 12000, (void*)stratum_name, 4, NULL,1);
 #elif defined(ESP32_2432S028R) || defined(ESP32_2432S028_2USB) || defined(ESP32_2432S024R)
  // Unified stack size for all 2432 variants to match working configuration
  BaseType_t res2 = xTaskCreatePinnedToCore(runStratumWorker, "Stratum", 13500, (void*)stratum_name, 4, NULL,1);
 #else
  BaseType_t res2 = xTaskCreatePinnedToCore(runStratumWorker, "Stratum", 15000, (void*)stratum_name, 4, NULL,1);
 #endif

  /******** CREATE MINER TASKS *****/
  //for (size_t i = 0; i < THREADS; i++) {
  //  char *name = (char*) malloc(32);
  //  sprintf(name, "(%d)", i);

  // Start mining tasks
  //BaseType_t res = xTaskCreate(runWorker, name, 35000, (void*)name, 1, NULL);
  TaskHandle_t minerTask1, minerTask2 = NULL;
  #ifdef HARDWARE_SHA265
    #if defined(CONFIG_IDF_TARGET_ESP32)
    xTaskCreate(minerWorkerHw, "MinerHw-0", 3584, (void*)0, 3, &minerTask1); // Reduced for ESP32 classic
    //xTaskCreate(minerWorkerSw, "MinerSw-0", 5000, (void*)0, 1, &minerTask1); // Reduced for ESP32 classic
    #else
    xTaskCreate(minerWorkerHw, "MinerHw-0", 4096, (void*)0, 3, &minerTask1);
    #endif
  #else
    #if defined(CONFIG_IDF_TARGET_ESP32)
    xTaskCreate(minerWorkerSw, "MinerSw-0", 5000, (void*)0, 1, &minerTask1); // Reduced for ESP32 classic
    #else
    xTaskCreate(minerWorkerSw, "MinerSw-0", 6000, (void*)0, 1, &minerTask1);
    #endif
  #endif
  esp_task_wdt_add(minerTask1);

#if (SOC_CPU_CORES_NUM >= 2)
  #if defined(CONFIG_IDF_TARGET_ESP32)
  xTaskCreate(minerWorkerSw, "MinerSw-1", 5000, (void*)1, 1, &minerTask2); // Reduced for ESP32 classic
  #else
  xTaskCreate(minerWorkerSw, "MinerSw-1", 6000, (void*)1, 1, &minerTask2);
  #endif
  esp_task_wdt_add(minerTask2);
#endif

  vTaskPrioritySet(NULL, 4);

  /******** MONITOR SETUP *****/
  setup_monitor();
}

void app_error_fault_handler(void *arg) {
  // Get stack errors
  char *stack = (char *)arg;

  // Print the stack errors in the console
  esp_log_write(ESP_LOG_ERROR, "APP_ERROR", "Error Stack Code:\n%s", stack);

  // restart ESP32
  esp_restart();
}

void loop() {
  // keep watching the push buttons:
  #ifdef PIN_BUTTON_1
    button1.tick();
  #endif

  #ifdef PIN_BUTTON_2
    button2.tick();
  #endif

#ifdef TOUCH_ENABLE
  // Unified touch handling: processes both screen switching and calibration trigger
  static bool calibrationTriggered = false;
  
  // Check touch state and handle all touch logic in single function
  uint16_t touchResult = touchHandler.isTouched();
  
  // Check if calibration was triggered (special return value from isTouched)
  if (touchResult == 999 && !calibrationTriggered) {  // 999 = calibration trigger signal
    Serial.println("Calibration trigger detected! (Touch held 10s) Starting calibration...");
    calibrationTriggered = true;
    calibrationInProgress = true;  // Prevent reset during calibration
    displayPaused = true;          // Pause display updates during calibration
    
    if (touchHandler.performCalibration()) {
      Serial.println("Touch calibration completed successfully!");
      
      // Apply new calibration to TouchHandler and save settings
      touchHandler.setTouchCalibration(Settings.touchCalibration);
      nvMem.saveConfig(&Settings);
      
      // Re-enable screen switching callback (gets lost during calibration)
      touchHandler.setScreenSwitchCallback(alternateScreenState);
      
      Serial.println("Touch calibration applied and saved!");
    } else {
      Serial.println("Touch calibration failed!");
    }
    
    displayPaused = false;         // Resume display updates after calibration
    calibrationInProgress = false; // Re-enable reset after calibration
    calibrationTriggered = false;
  }
#endif
  wifiManagerProcess(); // avoid delays() in loop when non-blocking and other long running code

  vTaskDelay(50 / portTICK_PERIOD_MS);
}
