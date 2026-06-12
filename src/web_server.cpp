#include "web_server.h"
#include "fan_control.h"
#include "config.h"
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <vector>
#include <algorithm>
#include "wifi_sniffer.h"

// Declare external function from main.cpp
extern const String& getStaIP();

// ============================================================
//  Web Server Implementation
// ============================================================

static AsyncWebServer s_server(WEB_SERVER_PORT);
static AsyncEventSource s_events("/api/events");
static String           s_sessionToken = "";
static bool             s_shouldReboot = false;
static uint32_t         s_rebootTime = 0;

/**
 * generateRandomToken
 * Tạo một chuỗi ngẫu nhiên làm Session Token
 */
static void generateRandomToken() {
    char buf[17];
    for (int i = 0; i < 16; i++) {
        uint8_t r = random(0, 16);
        buf[i] = (r < 10) ? ('0' + r) : ('a' + (r - 10));
    }
    buf[16] = '\0';
    s_sessionToken = String(buf);
    Serial.println("========================================");
    Serial.print("  [AUTH] New Session Token: ");
    Serial.println(s_sessionToken);
    Serial.println("========================================");
}

/**
 * isReqAuth
 * Kiểm tra xem request có token hợp lệ hay không
 */
static bool isReqAuth(AsyncWebServerRequest* req) {
    if (req->hasHeader("X-Token")) {
        return req->header("X-Token") == s_sessionToken;
    }
    if (req->hasParam("token")) {
        return req->getParam("token")->value() == s_sessionToken;
    }
    return false;
}

void webServerBroadcast(const char* event, const char* data) {
    s_events.send(data, event, millis());
}

void webServerBroadcastLog(const String& proto, const String& subtype, const String& src, const String& dst, int rssi, int len, const String& info, uint8_t channel, const String& srcMac, const String& dstMac, const String& rawHex) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.add<JsonObject>();
    obj["proto"] = proto;
    obj["subtype"] = subtype;
    obj["src"] = src;
    obj["dst"] = dst;
    obj["rssi"] = rssi;
    obj["len"] = len;
    obj["info"] = info;
    obj["channel"] = channel;
    obj["srcMac"] = srcMac;
    obj["dstMac"] = dstMac;
    obj["rawHex"] = rawHex;
    obj["time"] = (double)millis() / 1000.0;
    
    String json;
    serializeJson(doc, json);
    webServerBroadcast("logs", json.c_str());
}

// ============================================================
//  REST API Handlers
// ============================================================

static void handleGetStatus(AsyncWebServerRequest* req) {
    if (!isReqAuth(req)) { req->send(401); return; }
    
    JsonDocument doc;
    doc["rpm"]   = fanGetRPM();
    doc["temp"]  = round(fanGetTemp() * 10.0f) / 10.0f;
    doc["speed"] = fanGetSpeedPct();
    doc["mode"]  = (fanGetMode() == FanMode::AUTO) ? "auto" : "manual";

    doc["chip"]    = ESP.getChipModel();
    doc["cpu"]     = ESP.getCpuFreqMHz();
    doc["uptime"]  = millis() / 1000;
    doc["freeHeap"]   = ESP.getFreeHeap();
    doc["totalHeap"]  = ESP.getHeapSize();
    doc["freePsram"]  = ESP.getFreePsram();
    doc["totalPsram"] = ESP.getPsramSize();
    doc["flash"]      = ESP.getFlashChipSize();
    doc["spiffsTotal"] = LittleFS.totalBytes();
    doc["spiffsUsed"]  = LittleFS.usedBytes();
    doc["staIP"]   = WiFi.localIP().toString();
    doc["staSSID"] = WiFi.SSID();
    doc["rssi"]    = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    doc["sdk"]     = ESP.getSdkVersion();

    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

// ============================================================
//  Setup
// ============================================================

void webServerSetup() {
    if (!LittleFS.begin(true)) {
        Serial.println("[WEB] LittleFS mount failed!");
        return;
    }

    generateRandomToken();

    s_server.addHandler(&s_events);

    // --- Captive Portal handlers ---
    s_server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* req) { req->redirect("http://192.168.4.1/"); });
    s_server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest* req) { req->redirect("http://192.168.4.1/"); });
    s_server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "Microsoft Connect Test"); });
    s_server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "Microsoft NCSI"); });
    s_server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/html", "Success"); });

    // --- API ---

    s_server.on(
        "/api/login", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            deserializeJson(doc, data, len);
            String pass = doc["password"] | "";

            Preferences prefs;
            prefs.begin("sys", false);
            String storedPass = prefs.getString("webpass", "admin");
            prefs.end();

            if (pass == storedPass) {
                Serial.println("[AUTH] Password matched successfully!");
                String resp = "{\"ok\":true,\"token\":\"" + s_sessionToken + "\"}";
                req->send(200, "application/json", resp);
            } else {
                Serial.printf("[AUTH] Password mismatch! Received: '%s', Stored: '%s'\n", pass.c_str(), storedPass.c_str());
                req->send(401, "application/json", "{\"ok\":false,\"error\":\"Invalid Key\"}");
            }
        }
    );

    s_server.on("/api/status", HTTP_GET, handleGetStatus);

    s_server.on(
        "/api/wifi", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            String ssid = doc["ssid"] | "";
            String pass = doc["pass"] | "";

            if (ssid.length() > 0) {
                Preferences prefs;
                prefs.begin("wifi", false);
                prefs.putString("ssid", ssid);
                prefs.putString("pass", pass);
                prefs.end();

                req->send(200, "application/json", "{\"ok\":true}");

                s_shouldReboot = true;
                Serial.println("[SYS] WiFi credentials saved. ESP32 will reboot shortly...");
            } else {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"SSID cannot be empty\"}");
            }
        }
    );

    s_server.on(
        "/api/speed", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            int speed = doc["speed"] | 0;
            fanSetSpeed(static_cast<uint8_t>(constrain(speed, 0, 100)));
            fanSetMode(FanMode::MANUAL);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    s_server.on(
        "/api/step", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            int step = doc["step"] | 0;
            int newPct = (int)fanGetSpeedPct() + step;
            newPct = constrain(newPct, 0, 100);
            fanSetSpeed(static_cast<uint8_t>(newPct));
            fanSetMode(FanMode::MANUAL);
            req->send(200, "application/json", "{\"ok\":true,\"speed\":" + String(newPct) + "}");
        }
    );

    s_server.on(
        "/api/mode", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            String mode = doc["mode"] | "";
            if (mode == "auto") fanSetMode(FanMode::AUTO);
            else if (mode == "manual") fanSetMode(FanMode::MANUAL);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    s_server.on("/api/sniffer/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!isReqAuth(req)) { req->send(401); return; }
        JsonDocument doc;
        snifferGetStatsJson(doc);
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    s_server.on(
        "/api/sniffer/control", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            bool active = doc["active"] | false;
            uint8_t channel = doc["channel"] | 0;
            bool concurrent = doc["concurrent"] | false;
            if (active) snifferStart(channel, concurrent);
            else snifferStop();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // --- PWA / Service Worker files (no-cache) ---
    // sw.js must be served with Service-Worker-Allowed header and no cache
    s_server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/sw.js", "application/javascript");
        resp->addHeader("Service-Worker-Allowed", "/");
        resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        req->send(resp);
    });

    // workbox runtime – also no-cache so updates propagate
    s_server.on("/workbox-9c191d2f.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/workbox-9c191d2f.js", "application/javascript");
        resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        req->send(resp);
    });

    // registerSW.js
    s_server.on("/registerSW.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/registerSW.js", "application/javascript");
        resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        req->send(resp);
    });

    // Web App Manifest
    s_server.on("/manifest.webmanifest", HTTP_GET, [](AsyncWebServerRequest* req) {
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/manifest.webmanifest", "application/manifest+json");
        resp->addHeader("Cache-Control", "no-store");
        req->send(resp);
    });

    // PWA icons
    s_server.on("/pwa-192x192.png", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/pwa-192x192.png", "image/png");
    });
    s_server.on("/pwa-512x512.png", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/pwa-512x512.png", "image/png");
    });
    s_server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/apple-touch-icon.png", "image/png");
    });
    s_server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/favicon.ico", "image/x-icon");
    });

    // --- Static assets (index.html, style.css, …) ---
    s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // --- 404 / SPA fallback ---
    // API routes that are truly missing → captive-portal redirect
    // All other paths → serve index.html so the SPA router handles them
    s_server.onNotFound([](AsyncWebServerRequest* req) {
        const String& url = req->url();
        // Captive portal probes & missing API calls → redirect
        if (url.startsWith("/api/") || url == "/generate_204" || url == "/gen_204") {
            req->redirect("http://192.168.4.1/");
            return;
        }
        // SPA fallback: send index.html for any unmatched route
        if (LittleFS.exists("/index.html")) {
            req->send(LittleFS, "/index.html", "text/html");
        } else {
            req->redirect("http://192.168.4.1/");
        }
    });

    s_server.begin();
    Serial.printf("[WEB] Server started on port %d\n", WEB_SERVER_PORT);
}

static uint32_t s_lastBroadcast = 0;
void webServerLoop() {
    if (s_shouldReboot) {
        if (s_rebootTime == 0) {
            s_rebootTime = millis();
        } else if (millis() - s_rebootTime >= 2000) {
            Serial.println("[SYS] Rebooting now...");
            ESP.restart();
        }
    }

    if (millis() - s_lastBroadcast >= 1000) {
        s_lastBroadcast = millis();
        
        JsonDocument doc;
        doc["rpm"]   = fanGetRPM();
        doc["temp"]  = round(fanGetTemp() * 10.0f) / 10.0f;
        doc["speed"] = fanGetSpeedPct();
        doc["mode"]  = (fanGetMode() == FanMode::AUTO) ? "auto" : "manual";
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
        doc["staIP"] = WiFi.localIP().toString();

        String json;
        serializeJson(doc, json);
        webServerBroadcast("status", json.c_str());

        if (snifferIsActive()) {
            JsonDocument snifDoc;
            snifferGetStatsJson(snifDoc);
            snifDoc.remove("logs");
            String snifJson;
            serializeJson(snifDoc, snifJson);
            webServerBroadcast("sniffer", snifJson.c_str());
        }
    }
}
