// Network state definitions and event interface.

#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace WifiEvents {

enum class UpstreamState : uint8_t {
    Starting,
    Disconnected,
    Associated,
    HasIP
};

struct Snapshot {
    UpstreamState state;
    bool natEnabled;
    uint32_t connectedSinceMs;
    uint32_t lastDisconnectReason;
    uint32_t reconnectAttempts;
};

void begin();
void processEvents();
void printDiagnostics();
Snapshot getSnapshot();

} 