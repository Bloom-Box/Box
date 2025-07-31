#include <WiFi.h>
#include <stdio.h>
#include <vector>
#include <DHT.h>
#include <esp_system.h>

// === CONFIG ===
namespace wlan {
    const String ssid = "NorthernLight";
    const String password = "BloomBox";
}

namespace backend {
    const char* address = "192.168.4.3";
    const int port = 28878;
}

#define PUMP_PIN 0
#define LIGHT_PIN 17
#define DHT_PIN 5
#define MOISTURE_SENSOR_PIN 34

DHT dht(DHT_PIN, DHT22);

// === TOOLS ===

std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

const char* get_reset_reason(int reason) {
    switch (reason) {
        case 1 : return "POWERON_RESET";
        case 3 : return "SW_RESET";
        case 4 : return "OWDT_RESET";
        case 5 : return "DEEPSLEEP_RESET";
        case 6 : return "SDIO_RESET";
        case 7 : return "TG0WDT_SYS_RESET";
        case 8 : return "TG1WDT_SYS_RESET";
        case 9 : return "RTCWDT_SYS_RESET";
        case 10: return "INTRUSION_RESET";
        case 11: return "TGWDT_CPU_RESET";
        case 12: return "SW_CPU_RESET";
        case 13: return "RTCWDT_CPU_RESET";
        case 14: return "EXT_CPU_RESET";
        case 15: return "RTCWDT_BROWN_OUT_RESET";
        case 16: return "RTCWDT_RTC_RESET";
        default: return "NO_MEAN";
    }
}

// === NETWORK ===

WiFiClient server;

unsigned long lastWiFiAttempt = 0;
unsigned long lastServerAttempt = 0;
const unsigned long reconnectInterval = 5000;

void connectToWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.printf("Connecting to '%s'", wlan::ssid.c_str());
    WiFi.begin(wlan::ssid.c_str(), wlan::password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" - Connected!");
    } else {
        Serial.println(" - Failed to connect to WiFi.");
    }
}

void connectToServer() {
    if (server.connected()) return;

    Serial.printf("Connecting to %s:%d. ", backend::address, backend::port);

    server.stop();  // Clean up before reconnect
    if (server.connect(backend::address, backend::port)) {
        server.println("MAC|" + WiFi.macAddress());
        Serial.println(" - Connected!");
    } else {
        Serial.println(" - Failed!");
    }
}

// === LOGGING ===

void log(String level, String message) {
    Serial.println("[" + level + "] " + message);
    if (server.connected()) {
        server.println("LOG|" + level + "|" + message);
    }
}

// === MAIN LOOP ===

void loop() {
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED) {
        if (now - lastWiFiAttempt >= reconnectInterval) {
            lastWiFiAttempt = now;
            connectToWiFi();
        }
        return;
    }

    if (!server.connected()) {
        if (now - lastServerAttempt >= reconnectInterval) {
            lastServerAttempt = now;
            connectToServer();
        }
        return;
    }

    while (server.available()) {
        String message = server.readStringUntil('\n');
        message.trim();
        Serial.println("[Server] " + message);
        std::vector<std::string> args = split(message.c_str(), " ");

        if (args.empty()) {
            log("WARN", "Received empty message");
            continue;
        }

        else if (args[0] == "DATA") {
            float humidity = dht.readHumidity();
            float temperature = dht.readTemperature();

            if (isnan(humidity) || isnan(temperature)) {
                log("ERROR", "Failed to read from DHT sensor");
                continue;
            }

            float moisture = 1 - analogRead(MOISTURE_SENSOR_PIN) / 4095.0;
            server.printf("DATA|%.1f|%.1f|%.3f\n", temperature, humidity, moisture);
            Serial.printf("temperature: %.1f, humidity: %.1f, moisture: %.3f\n", temperature, humidity, moisture);
        }

        else if (args[0] == "LIGHT" && args.size() >= 2) {
            int previousState = digitalRead(LIGHT_PIN);
            int state = args[1] == "ON" ? HIGH : LOW;

            digitalWrite(LIGHT_PIN, state);

            if (previousState && !state) {
                log("INFO", "Restarting");
                delay(500);
                ESP.restart();
            }
        }

        else {
            log("WARN", "Unknown command: " + message);
        }
    }

    server.println("HEARTBEAT");
    delay(1000);
}

// === SETUP ===

void setup() {
    Serial.begin(115200);

    pinMode(PUMP_PIN, OUTPUT);
    pinMode(LIGHT_PIN, OUTPUT);
    pinMode(DHT_PIN, INPUT);
    pinMode(MOISTURE_SENSOR_PIN, INPUT);

    dht.begin();

    connectToWiFi();
    connectToServer();

    esp_reset_reason_t reason = esp_reset_reason();
    log("INFO", "Booting... Last reset reason: " + String(get_reset_reason(reason)));
}
