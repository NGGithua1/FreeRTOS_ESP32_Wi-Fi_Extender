//---@neph---
// Initialization and event processing in loop().

#include <Arduino.h>

#include "WifiEvents.h"
#include "SystemMonitor.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================");
    Serial.println(" ESP32 Wi-Fi Extender v2");
    Serial.println(" Arduino-ESP32 + FreeRTOS");
    Serial.println("==============================");

    WifiEvents::begin();
    SystemMonitor::begin();
}

void loop() {
    // Process queued Wi-Fi events and scheduled retries.
    WifiEvents::processEvents();

    // Yield without adding another application task.
    delay(10);
}