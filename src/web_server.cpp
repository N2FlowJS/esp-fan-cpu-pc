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
#include "rgb_led.h"

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
static String           s_savedSSID = "";
static String           s_savedPass = "";

static String getDefaultPassword() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[7];
    snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(buf);
}

/**
 * generateRandomToken
 * Generate a random string as Session Token
 */
static void generateRandomToken() {
    char buf[17];
    for (int i = 0; i < 16; i++) {
        uint8_t r = random(0, 16);
        buf[i] = (r < 10) ? ('0' + r) : ('a' + (r - 10));
    }
    buf[16] = '\0';
    s_sessionToken = String(buf);
    
    Preferences prefs;
    prefs.begin("sys", true);
    String activePass = prefs.getString("webpass", "");
    prefs.end();
    if (activePass.length() == 0) {
        activePass = getDefaultPassword();
    }
    
    Serial.println("========================================");
    Serial.println("  [AUTH] Session Initialized");
    Serial.printf ("  [AUTH] Active Password: %s\n", activePass.c_str());
    Serial.printf ("  [AUTH] Token (internal): %s\n", s_sessionToken.c_str());
    Serial.println("========================================");
}

void webServerInvalidateSession() {
    generateRandomToken();
    s_events.close();
    Serial.println("[AUTH] Active sessions invalidated.");
}


/**
 * isReqAuth
 * Check if the request has a valid token
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

static void broadcastStatus() {
    JsonDocument doc;
    doc["rpm"]   = fanGetRPM();
    doc["temp"]  = round(fanGetTemp() * 10.0f) / 10.0f;
    doc["speed"] = fanGetSpeedPct();
    doc["mode"]  = (fanGetMode() == FanMode::AUTO) ? "auto" : "manual";
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["totalHeap"] = ESP.getHeapSize();
    doc["freePsram"] = ESP.getFreePsram();
    doc["totalPsram"] = ESP.getPsramSize();
    doc["chip"] = ESP.getChipModel();
    doc["cpu"] = ESP.getCpuFreqMHz();
    doc["flash"] = ESP.getFlashChipSize();
    doc["sdk"] = ESP.getSdkVersion();
    doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    doc["staIP"] = WiFi.localIP().toString();
    String activeSSID = WiFi.SSID();
    if (activeSSID.length() == 0 && s_savedSSID.length() > 0) {
        activeSSID = s_savedSSID;
    }
    doc["staSSID"] = activeSSID;
    doc["staPass"] = s_savedPass;

    doc["ledMode"] = s_ledIsAuto ? "auto" : "manual";
    doc["ledBrightness"] = s_ledBrightness;
    doc["ledPin"] = s_ledPin;
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%06X", s_ledManualColor & 0xFFFFFF);
    doc["ledColor"] = String(colorHex);

    String json;
    serializeJson(doc, json);
    webServerBroadcast("status", json.c_str());
}

void webServerBroadcastLog(const String& proto, const String& subtype, const String& src, const String& dst, int rssi, int len, const String& info, uint8_t channel, const String& srcMac, const String& dstMac, const String& rawHex, int ttl, int srcPort, int dstPort) {
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
    obj["ttl"] = ttl;
    obj["srcPort"] = srcPort;
    obj["dstPort"] = dstPort;
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
    String activeSSID = WiFi.SSID();
    if (activeSSID.length() == 0 && s_savedSSID.length() > 0) {
        activeSSID = s_savedSSID;
    }
    doc["staSSID"] = activeSSID;
    doc["staPass"] = s_savedPass;
    doc["rssi"]    = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    doc["sdk"]     = ESP.getSdkVersion();

    doc["ledMode"] = s_ledIsAuto ? "auto" : "manual";
    doc["ledBrightness"] = s_ledBrightness;
    doc["ledPin"] = s_ledPin;
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%06X", s_ledManualColor & 0xFFFFFF);
    doc["ledColor"] = String(colorHex);

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

    // Initialize default password in preferences if not present,
    // or if it is set to "admin" (the old insecure default).
    Preferences prefs;
    prefs.begin("sys", false);
    bool shouldReset = !prefs.isKey("webpass");
    if (prefs.isKey("webpass")) {
        String currentPass = prefs.getString("webpass", "");
        if (currentPass == "admin") {
            shouldReset = true;
        }
    }
    if (shouldReset) {
        prefs.putString("webpass", getDefaultPassword());
    }
    prefs.end();

    Preferences wifiPrefs;
    wifiPrefs.begin("wifi", true);
    s_savedSSID = wifiPrefs.getString("ssid", "");
    s_savedPass = wifiPrefs.getString("pass", "");
    wifiPrefs.end();

    generateRandomToken();

    s_events.authorizeConnect([](AsyncWebServerRequest* request) {
        return isReqAuth(request);
    });
    s_server.addHandler(&s_events);


    // --- Captive Portal handlers ---
    s_server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* req) { req->redirect("http://192.168.4.1/"); });
    s_server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest* req) { req->redirect("http://192.168.4.1/"); });
    s_server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "Microsoft Connect Test"); });
    s_server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "Microsoft NCSI"); });
    s_server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/html", "Success"); });

    // --- API ---

    s_server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["mac"] = WiFi.macAddress();
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    s_server.on(
        "/api/login", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
                return;
            }
            
            String pass = doc["password"] | "";

            Preferences prefs;
            prefs.begin("sys", false);
            String storedPass = getDefaultPassword();
            if (prefs.isKey("webpass")) {
                storedPass = prefs.getString("webpass", storedPass);
                if (storedPass == "admin") {
                    storedPass = getDefaultPassword();
                    prefs.putString("webpass", storedPass);
                }
            } else {
                // Initialize default if not found to avoid future errors
                prefs.putString("webpass", storedPass);
            }
            prefs.end();

            if (pass == storedPass) {
                Serial.println("[AUTH] Password matched successfully!");
                String resp = "{\"ok\":true,\"token\":\"" + s_sessionToken + "\"}";
                req->send(200, "application/json", resp);
            } else {
                Serial.printf("[AUTH] Password mismatch! Received len: %d, Stored len: %d\n", pass.length(), storedPass.length());
                Serial.printf("[AUTH] Hint: Received: '%s', Stored: '%s'\n", pass.c_str(), storedPass.c_str());
                req->send(401, "application/json", "{\"ok\":false,\"error\":\"Invalid Key\"}");
            }
        }
    );

    s_server.on(
        "/api/password", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            String newPass = doc["password"] | "";

            if (newPass.length() >= 4) {
                Preferences prefs;
                prefs.begin("sys", false);
                prefs.putString("webpass", newPass);
                prefs.end();

                req->send(200, "application/json", "{\"ok\":true}");
                Serial.println("[AUTH] Web password updated via API.");
                webServerInvalidateSession();
            } else {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"Password too short (min 4 chars)\"}");
            }
        }
    );


    s_server.on("/api/status", handleGetStatus);

    s_server.on("/api/sniffer/filters", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!isReqAuth(req)) { req->send(401); return; }
        JsonDocument doc;
        snifferGetFilterConfig(doc);
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    s_server.on(
        "/api/sniffer/filters", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);

            String wl = "";
            if (doc["whitelist"].is<JsonArray>()) {
                JsonArray wlArr = doc["whitelist"].as<JsonArray>();
                for (JsonVariant v : wlArr) {
                    if (wl.length() > 0) wl += ",";
                    wl += v.as<String>();
                }
            }

            String bl = "";
            if (doc["blacklist"].is<JsonArray>()) {
                JsonArray blArr = doc["blacklist"].as<JsonArray>();
                for (JsonVariant v : blArr) {
                    if (bl.length() > 0) bl += ",";
                    bl += v.as<String>();
                }
            }

            snifferSetFilterConfig(wl, bl);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    s_server.on(
        "/api/sniffer/owner", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            if (doc["mac"].is<String>()) {
                snifferAddOwnerMac(doc["mac"].as<String>());
            }
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

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

                s_savedSSID = ssid;
                s_savedPass = pass;

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
            broadcastStatus();
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
            broadcastStatus();
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
            broadcastStatus();
        }
    );

    s_server.on(
        "/api/led", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            String mode = doc["mode"] | "";
            if (mode == "auto") {
                ledSetMode(true);
            } else if (mode == "manual") {
                ledSetMode(false);
                String colorStr = doc["color"] | "";
                if (colorStr.startsWith("#") && colorStr.length() == 7) {
                    long color = strtol(colorStr.c_str() + 1, NULL, 16);
                    uint8_t r = (color >> 16) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = color & 0xFF;
                    ledSetManualColor(r, g, b);
                }
            }
            
            if (!doc["brightness"].is<JsonArray>() && !doc["brightness"].is<JsonObject>() && !doc["brightness"].isNull()) {
                int b = doc["brightness"] | 128;
                ledSetBrightness(constrain(b, 0, 255));
            }
            
            if (!doc["pin"].is<JsonArray>() && !doc["pin"].is<JsonObject>() && !doc["pin"].isNull()) {
                int pin = doc["pin"] | 48;
                ledSetPin(pin);
            }

            req->send(200, "application/json", "{\"ok\":true}");
            broadcastStatus();
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

    // registerSW.js
    s_server.on("/registerSW.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/registerSW.js", "application/javascript");
        resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        req->send(resp);
    });

    // Web App Manifest
    auto manifestHandler = [](AsyncWebServerRequest* req) {
        if (LittleFS.exists("/manifest.webmanifest")) {
            AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/manifest.webmanifest", "application/json");
            resp->addHeader("Access-Control-Allow-Origin", "*");
            req->send(resp);
        } else {
            req->send(404, "text/plain", "Manifest not found");
        }
    };
    s_server.on("/manifest.webmanifest", HTTP_GET, manifestHandler);
    s_server.on("/manifest.json", HTTP_GET, manifestHandler);

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

    // --- Static assets (index.html, style.css, ...) ---
    // Moved below specific handlers to avoid shadowing
    s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // --- 404 / SPA fallback ---
    // API routes that are truly missing → captive-portal redirect
    // All other paths → serve index.html so the SPA router handles them
    s_server.onNotFound([](AsyncWebServerRequest* req) {
        const String& url = req->url();
        
        // Handle workbox with hash (e.g. /workbox-9c191d2f.js)
        if (url.startsWith("/workbox-") && url.endsWith(".js")) {
            if (LittleFS.exists(url)) {
                AsyncWebServerResponse* resp = req->beginResponse(LittleFS, url, "application/javascript");
                resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(resp);
                return;
            }
        }

        // Captive portal probes & missing API calls → redirect
        if (url.startsWith("/api/") || url == "/generate_204" || url == "/gen_204") {
            req->redirect("http://192.168.4.1/");
            return;
        }

        // Return 404 for missing static files (URLs with file extensions) instead of falling back to index.html
        int lastSlash = url.lastIndexOf('/');
        int lastDot = url.lastIndexOf('.');
        if (lastDot > lastSlash) {
            req->send(404, "text/plain", "Not Found");
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
        broadcastStatus();

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
      }
    }
}
