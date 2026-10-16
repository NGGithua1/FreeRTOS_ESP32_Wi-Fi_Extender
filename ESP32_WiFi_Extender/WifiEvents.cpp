// Event queue, reconnection policy and NAT lifecycle.#include "WifiEvents.h"


#include <Arduino.h>
#include <WiFi.h>
#include <Network.h>
#include <NetworkEvents.h>

#include "WifiEvents.h"
#include "Config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_heap_caps.h>

namespace {

struct QueuedEvent {
    arduino_event_id_t id;
    uint32_t reason;
};

QueueHandle_t eventQueue = nullptr;

WifiEvents::Snapshot snapshot = {
    WifiEvents::UpstreamState::Starting,
    false,
    0,
    0,
    0
};

uint32_t nextRetryMs = 0;
uint32_t retryDelayMs = Config::RETRY_INITIAL_MS;
bool stationStarted = false;
bool apStarted = false;

void onWiFiEvent(
    arduino_event_id_t event,
    arduino_event_info_t info
) {
    if (eventQueue == nullptr) {
        return;
    }

    QueuedEvent item{};
    item.id = event;

    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        item.reason = info.wifi_sta_disconnected.reason;
    }

    // Never block the Wi-Fi event task.
    if (xQueueSend(eventQueue, &item, 0) != pdPASS) {
        // Queue overflow is a diagnostic condition.
        // The next periodic status report can expose
        // current state even if an event was dropped.
    }
}

void scheduleRetry() {
    nextRetryMs = millis() + retryDelayMs;

    retryDelayMs = min(
        retryDelayMs * 2UL,
        Config::RETRY_MAX_MS
    );
}

void disableNAT() {
    if (!snapshot.natEnabled || !apStarted) {
        return;
    }

    if (WiFi.AP.enableNAPT(false)) {
        snapshot.natEnabled = false;
        Serial.println("[NAT] Disabled.");
    } else {
        Serial.println("[NAT] Disable failed; will reconcile.");
    }
}

void enableNAT() {
    if (!apStarted || !stationStarted ||
        snapshot.state != WifiEvents::UpstreamState::HasIP) {
        return;
    }

    if (snapshot.natEnabled) {
        return;
    }

    if (WiFi.AP.enableNAPT(true)) {
        snapshot.natEnabled = true;
        Serial.println("[NAT] Enabled.");
    } else {
        Serial.println("[NAT] Enable failed; will retry.");
    }
}

void handleEvent(const QueuedEvent &item) {
    switch (item.id) {

        case ARDUINO_EVENT_WIFI_STA_START:
            stationStarted = true;
            Serial.println("[STA] Started.");
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            snapshot.state = WifiEvents::UpstreamState::Associated;
            Serial.println("[STA] Associated; waiting for IP.");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            snapshot.state = WifiEvents::UpstreamState::HasIP;
            snapshot.connectedSinceMs = millis();
            snapshot.reconnectAttempts = 0;
            retryDelayMs = Config::RETRY_INITIAL_MS;
            nextRetryMs = 0;

            Serial.print("[STA] IP assigned: ");
            Serial.println(WiFi.localIP());

            enableNAT();
            break;

        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            snapshot.state = WifiEvents::UpstreamState::Disconnected;
            Serial.println("[STA] IP lost.");
            disableNAT();
            scheduleRetry();
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            snapshot.lastDisconnectReason = item.reason;
            snapshot.state = WifiEvents::UpstreamState::Disconnected;

            Serial.printf(
                "[STA] Disconnected. Reason code: %lu\n",
                static_cast<unsigned long>(item.reason)
            );

            disableNAT();
            scheduleRetry();
            break;

        case ARDUINO_EVENT_WIFI_STA_STOP:
            stationStarted = false;
            snapshot.state = WifiEvents::UpstreamState::Disconnected;
            disableNAT();
            Serial.println("[STA] Stopped.");
            break;

        case ARDUINO_EVENT_WIFI_AP_START:
            apStarted = true;
            Serial.print("[AP] Started at ");
            Serial.println(WiFi.AP.localIP());
            break;

        case ARDUINO_EVENT_WIFI_AP_STOP:
            apStarted = false;
            snapshot.natEnabled = false;
            Serial.println("[AP] Stopped.");
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.printf(
                "[AP] Client connected. Count: %u\n",
                WiFi.AP.stationCount()
            );
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.printf(
                "[AP] Client disconnected. Count: %u\n",
                WiFi.AP.stationCount()
            );
            break;

        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Serial.print("[AP] Client received IP: ");
            Serial.println(
                IPAddress(item.reason)
            );
            break;

        default:
            break;
    }
}

}

namespace WifiEvents {

void begin() {
    eventQueue = xQueueCreate(
        Config::EVENT_QUEUE_LENGTH,
        sizeof(QueuedEvent)
    );

    if (eventQueue == nullptr) {
        Serial.println("[ERROR] Event queue allocation failed.");
        return;
    }

    // Register before starting either Wi-Fi interface.
    Network.onEvent(onWiFiEvent);

    WiFi.setSleep(false);

    WiFi.AP.begin();

    IPAddress apIP(192, 168, 4, 1);
    IPAddress apMask(255, 255, 255, 0);
    IPAddress leaseStart(192, 168, 4, 2);
    IPAddress apDNS(8, 8, 8, 8);

    if (!WiFi.AP.config(
            apIP, apIP, apMask, leaseStart, apDNS)) {
        Serial.println("[ERROR] AP configuration failed.");
        return;
    }

    if (!WiFi.AP.create(
            Config::AP_SSID,
            Config::AP_PASSWORD,
            Config::AP_CHANNEL,
            false,
            Config::AP_MAX_CLIENTS)) {
        Serial.println("[ERROR] AP creation failed.");
        return;
    }

    if (!WiFi.AP.waitStatusBits(
            ESP_NETIF_STARTED_BIT,
            Config::AP_START_TIMEOUT_MS)) {
        Serial.println("[ERROR] AP start timeout.");
        return;
    }

    apStarted = true;

    Serial.print("[AP] Address: ");
    Serial.println(WiFi.AP.localIP());

    // Start the upstream connection after AP startup.
    WiFi.begin(Config::STA_SSID, Config::STA_PASSWORD);
}

void processEvents() {
    if (eventQueue == nullptr) {
        return;
    }

    QueuedEvent item{};

    // Drain all queued events before handling retry timing.
    while (xQueueReceive(eventQueue, &item, 0) == pdPASS) {
        handleEvent(item);
    }

    const uint32_t now = millis();

    // Retry only while disconnected and after the deadline.
    if (snapshot.state == UpstreamState::Disconnected &&
        nextRetryMs != 0 &&
        static_cast<int32_t>(now - nextRetryMs) >= 0) {

        nextRetryMs = 0;
        snapshot.reconnectAttempts++;

        Serial.printf(
            "[STA] Reconnect attempt %lu\n",
            static_cast<unsigned long>(
                snapshot.reconnectAttempts
            )
        );

        if (!WiFi.reconnect()) {
            scheduleRetry();
        }
    }

    // Retry NAPT if the previous operation failed.
    if (snapshot.state == UpstreamState::HasIP &&
        !snapshot.natEnabled) {
        enableNAT();
    }
}

void printDiagnostics() {
    Serial.println("\n--- Extender diagnostics ---");

    Serial.print("Upstream state: ");
    switch (snapshot.state) {
        case UpstreamState::Starting:
            Serial.println("STARTING");
            break;
        case UpstreamState::Disconnected:
            Serial.println("DISCONNECTED");
            break;
        case UpstreamState::Associated:
            Serial.println("ASSOCIATED; NO IP CONFIRMED");
            break;
        case UpstreamState::HasIP:
            Serial.println("IP ASSIGNED");
            break;
    }

    if (snapshot.state == UpstreamState::HasIP) {
        Serial.print("Upstream IP: ");
        Serial.println(WiFi.localIP());

        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");

        Serial.print("Connected duration: ");
        Serial.print(
            (millis() - snapshot.connectedSinceMs) / 1000
        );
        Serial.println(" s");
    }

    Serial.print("AP clients: ");
    Serial.println(WiFi.AP.stationCount());

    Serial.print("NAPT enabled: ");
    Serial.println(snapshot.natEnabled ? "yes" : "no");

    Serial.print("Last disconnect reason: ");
    Serial.println(snapshot.lastDisconnectReason);

    Serial.print("Reconnect attempts: ");
    Serial.println(snapshot.reconnectAttempts);

    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());

    Serial.print("Minimum free heap: ");
    Serial.println(ESP.getMinFreeHeap());
}

Snapshot getSnapshot() {
    return snapshot;
}

} 