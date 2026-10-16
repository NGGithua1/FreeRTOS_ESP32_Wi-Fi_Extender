// Credentials, AP addressing, retry and diagnostic settings.

#pragma once

#include <Arduino.h>

namespace Config {

// Upstream router
constexpr char STA_SSID[] = "YOUR_ROUTER_SSID";
constexpr char STA_PASSWORD[] = "YOUR_ROUTER_PASSWORD";

// ESP32 extender access point
constexpr char AP_SSID[] = "ESP32-Extended";
constexpr char AP_PASSWORD[] = "YOUR_SECURE_PASSWORD";

// Keep this subnet different from the upstream router's subnet.
constexpr uint8_t AP_CHANNEL = 1;
constexpr uint8_t AP_MAX_CLIENTS = 4;

constexpr uint32_t AP_START_TIMEOUT_MS = 2000;

// Retry policy
constexpr uint32_t RETRY_INITIAL_MS = 2000;
constexpr uint32_t RETRY_MAX_MS = 60000;

// Diagnostics
constexpr uint32_t MONITOR_INTERVAL_MS = 5000;

constexpr uint32_t EVENT_QUEUE_LENGTH = 16;
constexpr uint32_t MONITOR_STACK_WORDS = 3072;
constexpr UBaseType_t MONITOR_PRIORITY = 1;

} 