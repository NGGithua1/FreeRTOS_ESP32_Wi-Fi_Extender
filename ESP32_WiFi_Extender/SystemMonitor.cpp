// Periodic RSSI, heap, client and stack diagnostics.

#include "SystemMonitor.h"
#include "WifiEvents.h"
#include "Config.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

void monitorTask(void *parameter) {
    (void)parameter;

    for (;;) {
        WifiEvents::printDiagnostics();

        const UBaseType_t stackRemaining =
            uxTaskGetStackHighWaterMark(nullptr);

        Serial.print("[MONITOR] Stack high-water mark: ");
        Serial.print(stackRemaining);
        Serial.println(" words");

        vTaskDelay(
            pdMS_TO_TICKS(Config::MONITOR_INTERVAL_MS)
        );
    }
}

} 

namespace SystemMonitor {

void begin() {
    BaseType_t result = xTaskCreatePinnedToCore(
        monitorTask,
        "WiFiMonitor",
        4096,
        nullptr,
        Config::MONITOR_PRIORITY,
        nullptr,
        1
    );

    if (result != pdPASS) {
        Serial.println(
            "[ERROR] Could not create monitoring task."
        );
    } else {
        Serial.println("[MONITOR] Task started.");
    }
}

}