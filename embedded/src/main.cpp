#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "env.h"

// ── Pin definitions ──────────────────────────────────────────────────────────
#define TEMP_PIN    4
#define PIR_PIN     15
#define LIGHT_PIN   22
#define FAN_PIN     23

// ── Sensor setup ─────────────────────────────────────────────────────────────
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

// ── Timing ───────────────────────────────────────────────────────────────────
const unsigned long INTERVAL_MS = 3000;
unsigned long lastUpdate = 0;

// ── WiFi ─────────────────────────────────────────────────────────────────────
void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
}

// ── POST /data ────────────────────────────────────────────────────────────────
void postSensorData(float temperature, bool presence) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ERROR] WiFi not connected - skipping POST /data");
        return;
    }

    HTTPClient http;
    String url = String(SERVER_BASE_URL) + "/data";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<128> doc;
    doc["temperature"] = temperature;
    doc["presence"]    = presence;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    if (code > 0) {
        Serial.printf("[POST /data] %d\n", code);
    } else {
        Serial.printf("[POST /data] Error: %s\n", http.errorToString(code).c_str());
    }
    http.end();
}

// ── GET /state ────────────────────────────────────────────────────────────────
void fetchAndApplyState() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ERROR] WiFi not connected - skipping GET /state");
        return;
    }

    HTTPClient http;
    String url = String(SERVER_BASE_URL) + "/state";
    http.begin(url);

    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            bool fan   = doc["fan"]   | false;
            bool light = doc["light"] | false;

            digitalWrite(FAN_PIN,   fan   ? HIGH : LOW);
            digitalWrite(LIGHT_PIN, light ? HIGH : LOW);

            Serial.printf("[GET /state] fan=%s  light=%s\n",
                          fan ? "ON" : "OFF", light ? "ON" : "OFF");
        } else {
            Serial.printf("[GET /state] JSON parse error: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[GET /state] HTTP error: %d\n", code);
    }
    http.end();
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(LIGHT_PIN, OUTPUT);
    pinMode(FAN_PIN,   OUTPUT);
    pinMode(PIR_PIN,   INPUT);

    digitalWrite(LIGHT_PIN, LOW);
    digitalWrite(FAN_PIN,   LOW);

    tempSensor.begin();
    connectWiFi();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WARN] WiFi lost - reconnecting...");
        connectWiFi();
    }

    unsigned long now = millis();
    if (now - lastUpdate >= INTERVAL_MS) {
        lastUpdate = now;

        tempSensor.requestTemperatures();
        float temperature = tempSensor.getTempCByIndex(0);
        if (temperature == DEVICE_DISCONNECTED_C) {
            Serial.println("[ERROR] Temperature sensor disconnected");
            return;
        }

        bool presence = digitalRead(PIR_PIN) == HIGH;

        Serial.printf("[Sensors] temp=%.2f°C  presence=%s\n",
                      temperature, presence ? "true" : "false");

        postSensorData(temperature, presence);
        fetchAndApplyState();
    }
}