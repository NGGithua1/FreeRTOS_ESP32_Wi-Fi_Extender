# ESP32 Wi-Fi Extender

A Wi-Fi extender built using an ESP32 DevKit, C++, and the Arduino-ESP32 framework. The ESP32 connects to an existing Wi-Fi router as a station (STA), creates a separate wireless access point (AP), and uses Network Address and Port Translation (NAPT) to share the upstream internet connection with connected devices.

## Prerequisites 

- **Hardware:** ESP32 DevKit
- **Development environment:** Arduino IDE
- **Framework:** Arduino-ESP32
- **ESP32 Arduino core:** 3.3.12
- **Programming language:** C++
- **Network mode:** Wi-Fi Station + SoftAP (AP+STA)
- **Network translation:** NAPT

## 1. How It Works

The ESP32 operates two Wi-Fi interfaces simultaneously:

- **Station (STA):** Connects to the existing Wi-Fi router.
- **Access Point (AP):** Creates a separate Wi-Fi network for phones and laptops.
- **NAPT:** Translates client traffic so connected devices can share the upstream internet connection.

### Network Architecture

```text
             INTERNET
                 |
          Existing Router
                 |
            Wi-Fi (STA)
                 |
          +--------------+
          | ESP32 DevKit |
          |              |
          |     NAPT     |
          |              |
          | Wi-Fi AP     |
          +--------------+
                 |
          ESP32-WIFI-EXTENDER
                 |
          Phone / Laptop
```

The ESP32 does not create a new internet connection. It forwards client traffic through the router it connects to.

## 2. Network Configuration

The prototype uses the following configuration:

| Setting | Value |
|---|---|
| AP SSID | `ESP32-WIFI-EXTENDER` |
| AP IP address | `192.168.4.1` |
| Subnet mask | `255.255.255.0` |
| DHCP lease start | `192.168.4.2` |
| DNS resolver | `8.8.4.4` |
| Serial baud rate | `115200` |

The AP subnet must not conflict with the existing router's subnet.

## 3. Wi-Fi Configuration

Configure the upstream router and extender credentials in the source code:

```cpp
#define STA_SSID "YOUR_ROUTER_SSID"
#define STA_PASS "YOUR_ROUTER_PASSWORD"

#define AP_SSID "ESP32-WIFI-EXTENDER"
#define AP_PASS "YOUR_SECURE_PASSWORD"
```

Replace the placeholders with your actual credentials before compiling.

**Security:** Avoid committing real Wi-Fi passwords to a public repository. For a version-controlled project, use a separate local configuration file excluded through `.gitignore`, and commit a placeholder configuration example instead.

## 4. Network Event Handling

The firmware registers a callback using:

```cpp
Network.onEvent(onEvent);
```

The callback monitors changes in the station and access-point interfaces.

Key events include:

- `ARDUINO_EVENT_WIFI_STA_CONNECTED`: The ESP32 associates with the router.
- `ARDUINO_EVENT_WIFI_STA_GOT_IP`: The ESP32 receives an IP address and enables NAPT.
- `ARDUINO_EVENT_WIFI_STA_DISCONNECTED`: The upstream connection is lost and NAPT is disabled.
- `ARDUINO_EVENT_WIFI_AP_START`: The extender access point starts.
- `ARDUINO_EVENT_WIFI_AP_STACONNECTED`: A client connects to the extender.
- `ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED`: A client receives an IP address through DHCP.

Enabling NAPT after the station receives an IP address ensures that the upstream interface is ready before internet sharing begins.

## 5. Installation and Usage

1. Install **esp32 by Espressif Systems** through Arduino IDE's Boards Manager.
2. Select the appropriate ESP32 board and USB serial port.
3. Enter the correct router and extender credentials.
4. Compile the firmware using Arduino IDE.
5. Connect the ESP32 using a USB data cable and upload the firmware.
6. Open Serial Monitor at `115200` baud.
7. Wait for the station connection and IP address confirmation.
8. Connect a phone or laptop to `ESP32-WIFI-EXTENDER`.
9. Test internet access using a browser or speed-test application.

## 6. FreeRTOS and Scheduling

The Arduino-ESP32 framework runs on FreeRTOS. The Wi-Fi driver, network event processing, and TCP/IP stack operate through framework-managed tasks.

The current prototype uses an event-driven design for network state changes. Its `loop()` function only calls `delay(20000)`; this does not stop the network stack from processing traffic.

As the project grows, application-level monitoring and diagnostics can be organized into dedicated FreeRTOS tasks without manually scheduling internal Wi-Fi or TCP/IP tasks.

## 7. Limitations

- The ESP32's station and access-point interfaces share one Wi-Fi radio.
- Throughput and client capacity are limited compared with a dedicated commercial repeater.
- The AP and station channel behavior depends on the upstream Wi-Fi connection.
- Performance varies with signal strength, interference, and client load.

## 8. References

- [Arduino-ESP32 Wi-Fi API documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
- [Official Espressif WiFiExtender example](https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiExtender/WiFiExtender.ino)
- [ESP-IDF networking documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif.html)
