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
#include "wifi_stress_test.h"
#include "wifi_ping.h"
#include "rgb_led.h"

// Declare external function from main.cpp
extern const String& getStaIP();

// ============================================================
//  Web Server Implementation
// ============================================================

static AsyncWebServer s_server(WEB_SERVER_PORT);
static AsyncEventSource s_events("/api/events");
static AsyncEventSource s_deviceEvents("/api/events/devices");
static String           s_sessionToken = "";
static bool             s_shouldReboot = false;
static uint32_t         s_rebootTime = 0;
static String           s_savedSSID = "";
static String           s_savedPass = "";
static bool             s_staEnabled = true;
static const char*      FIRMWARE_VERSION = "1.0.2";

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
    s_deviceEvents.close();
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
    if (strcmp(event, "devices") == 0) {
        s_deviceEvents.send(data, event, millis());
    } else {
        s_events.send(data, event, millis());
    }
}

static void broadcastStatus() {
    JsonDocument doc;
    doc["rpm"]   = fanGetRPM();
    doc["temp"]  = round(fanGetTemp() * 10.0f) / 10.0f;
    doc["speed"] = fanGetSpeedPct();
    doc["mode"]  = (fanGetMode() == FanMode::AUTO) ? "auto" : "manual";
    doc["uptime"] = millis() / 1000;
    doc["healthAlert"] = fanHasHealthAlert();
    doc["hostTempActive"] = fanIsHostTempActive();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["totalHeap"] = ESP.getHeapSize();
    doc["freePsram"] = ESP.getFreePsram();
    doc["totalPsram"] = ESP.getPsramSize();
    doc["chip"] = ESP.getChipModel();
    doc["chipRev"] = ESP.getChipRevision();
    doc["cpu"] = ESP.getCpuFreqMHz();
    doc["flash"] = ESP.getFlashChipSize();
    doc["spiffsTotal"] = LittleFS.totalBytes();
    doc["spiffsUsed"]  = LittleFS.usedBytes();
    doc["version"] = FIRMWARE_VERSION;
    doc["sdk"] = ESP.getSdkVersion();
    doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    
    String ip = WiFi.localIP().toString();
    doc["staIP"] = (ip == "0.0.0.0") ? "" : ip;
    doc["apIP"] = WiFi.softAPIP().toString();

    String activeSSID = WiFi.SSID();
    if (activeSSID.length() == 0 && s_savedSSID.length() > 0) {
        activeSSID = s_savedSSID;
    }
    doc["staSSID"] = activeSSID;
    doc["staPass"] = s_savedPass;
    doc["staEnabled"] = s_staEnabled;

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

#include <mutex>

static JsonDocument s_pendingLogs;
static uint32_t     s_lastLogBroadcast = 0;
static std::mutex   s_logMutex;

bool webServerLogBufferFull() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (!s_pendingLogs.is<JsonArray>()) return false;
    return s_pendingLogs.as<JsonArray>().size() >= 100;
}

void webServerBroadcastLog(const String& proto, const String& subtype, const String& src, const String& dst, int rssi, int len, const String& info, uint8_t channel, const String& srcMac, const String& dstMac, const String& rawHex, int ttl, int srcPort, int dstPort) {
    std::lock_guard<std::mutex> lock(s_logMutex);
    
    if (!s_pendingLogs.is<JsonArray>()) {
        s_pendingLogs.to<JsonArray>();
    }
    
    JsonArray arr = s_pendingLogs.as<JsonArray>();
    
    // Debug: log every 50 packets
    static uint32_t bcastCount = 0;
    bcastCount++;
    if (bcastCount % 50 == 0) {
        Serial.printf("[BCAST] Broadcasting packet #%d, pending queue size: %d\n", bcastCount, arr.size());
    }
    
    // Circular buffer: remove oldest if full to maintain latest logs
    if (arr.size() >= 100) {
        arr.remove(0);
    }

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
    doc["healthAlert"] = fanHasHealthAlert();
    doc["hostTempActive"] = fanIsHostTempActive();

    doc["chip"]    = ESP.getChipModel();
    doc["chipRev"] = ESP.getChipRevision();
    doc["cpu"]     = ESP.getCpuFreqMHz();
    doc["uptime"]  = millis() / 1000;
    doc["freeHeap"]   = ESP.getFreeHeap();
    doc["totalHeap"]  = ESP.getHeapSize();
    doc["freePsram"]  = ESP.getFreePsram();
    doc["totalPsram"] = ESP.getPsramSize();
    doc["flash"]      = ESP.getFlashChipSize();
    doc["spiffsTotal"] = LittleFS.totalBytes();
    doc["spiffsUsed"]  = LittleFS.usedBytes();
    doc["version"] = FIRMWARE_VERSION;
    
    String ip = WiFi.localIP().toString();
    doc["staIP"] = (ip == "0.0.0.0") ? "" : ip;
    doc["apIP"] = WiFi.softAPIP().toString();

    String activeSSID = WiFi.SSID();
    if (activeSSID.length() == 0 && s_savedSSID.length() > 0) {
        activeSSID = s_savedSSID;
    }
    doc["staSSID"] = activeSSID;
    doc["staPass"] = s_savedPass;
    doc["rssi"]    = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    doc["sdk"]     = ESP.getSdkVersion();
    doc["staEnabled"] = s_staEnabled;

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
    // Add CORS headers for development
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, X-Token");

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
        bool auth = isReqAuth(request);
        if (!auth) {
            if (request->client()) {
                Serial.printf("[SSE] Auth failed for client: %s\n", request->client()->remoteIP().toString().c_str());
            }
        }
        return auth;
    });
    s_events.onConnect([](AsyncEventSourceClient* client) {
        if (client->client()) {
            Serial.printf("[SSE] Client authorized and connected: %s\n", client->client()->remoteIP().toString().c_str());
        }
        client->send("hello", NULL, millis(), 1000);
    });
    s_server.addHandler(&s_events);

    s_deviceEvents.authorizeConnect([](AsyncWebServerRequest* request) {
        return isReqAuth(request);
    });
    s_deviceEvents.onConnect([](AsyncEventSourceClient* client) {
        client->send("hello devices", NULL, millis(), 1000);
    });
    s_server.addHandler(&s_deviceEvents);

    // --- Root handler ---
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/index.html", "text/html");
    });

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


    s_server.on("/api/fancurve", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!isReqAuth(req)) { req->send(401); return; }
        
        TempSpeedPoint points[MAX_FAN_CURVE_POINTS];
        size_t count = fanGetCurve(points, MAX_FAN_CURVE_POINTS);
        
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (size_t i = 0; i < count; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["temp"] = points[i].tempC;
            obj["speed"] = points[i].speedPct;
        }
        
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    s_server.on(
        "/api/fancurve", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            if (!doc.is<JsonArray>()) {
                req->send(400, "application/json", "{\"error\":\"Expected JSON array\"}");
                return;
            }
            
            JsonArray arr = doc.as<JsonArray>();
            size_t count = arr.size();
            if (count == 0) {
                req->send(400, "application/json", "{\"error\":\"Empty array\"}");
                return;
            }
            
            if (count > MAX_FAN_CURVE_POINTS) count = MAX_FAN_CURVE_POINTS;
            
            TempSpeedPoint points[MAX_FAN_CURVE_POINTS];
            for (size_t i = 0; i < count; i++) {
                points[i].tempC = arr[i]["temp"] | 0.0f;
                points[i].speedPct = arr[i]["speed"] | 0;
            }
            
            fanSetCurve(points, count);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    s_server.on(
        "/api/temp/host", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            if (doc["temp"].is<float>()) {
                float t = doc["temp"];
                fanSetHostTemp(t);
                req->send(200, "application/json", "{\"ok\":true}");
            } else {
                req->send(400, "application/json", "{\"error\":\"Missing temp field\"}");
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

    s_server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!isReqAuth(req)) { req->send(401); return; }
        
        int n = WiFi.scanComplete();
        if (n == -2) {
            WiFi.scanNetworks(true); // Async scan
            req->send(202, "application/json", "{\"status\":\"scanning\"}");
        } else if (n == -1) {
            req->send(202, "application/json", "{\"status\":\"scanning\"}");
        } else {
            JsonDocument doc;
            JsonArray networks = doc.to<JsonArray>();
            for (int i = 0; i < n; ++i) {
                JsonObject net = networks.add<JsonObject>();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
                net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
                net["channel"] = WiFi.channel(i);
            }
            WiFi.scanDelete();
            String json;
            serializeJson(doc, json);
            req->send(200, "application/json", json);
            // Trigger a new scan for the next request
            WiFi.scanNetworks(true);
        }
    });

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
        "/api/wifi/toggle", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            bool enabled = doc["enabled"] | false;

            Preferences prefs;
            prefs.begin("wifi", false);
            prefs.putBool("enabled", enabled);
            prefs.end();

            req->send(200, "application/json", "{\"ok\":true}");

            s_shouldReboot = true;
            Serial.println("[SYS] WiFi station toggle saved. ESP32 will reboot shortly...");
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

    // Diagnostic endpoint to check pending logs in buffer
    s_server.on("/api/sniffer/logs-debug", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!isReqAuth(req)) { req->send(401); return; }
        JsonDocument doc;
        {
            std::lock_guard<std::mutex> lock(s_logMutex);
            doc["pendingLogsCount"] = s_pendingLogs.is<JsonArray>() ? s_pendingLogs.as<JsonArray>().size() : 0;
            if (s_pendingLogs.is<JsonArray>() && s_pendingLogs.as<JsonArray>().size() > 0) {
                doc["latestLogs"] = s_pendingLogs;
            }
        }
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    // REST endpoint to fetch pending sniffer packets (returns and clears buffer)
    s_server.on("/api/sniffer/packets", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!isReqAuth(req)) { req->send(401); return; }
        String json;
        {
            std::lock_guard<std::mutex> lock(s_logMutex);
            if (s_pendingLogs.is<JsonArray>() && s_pendingLogs.as<JsonArray>().size() > 0) {
                serializeJson(s_pendingLogs, json);
                // Clear buffer after serializing so caller takes ownership of the data
                s_pendingLogs.clear();
            } else {
                json = "[]";
            }
        }
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
            
            // Broadcast status immediately for better UI responsiveness
            JsonDocument snifDoc;
            snifferGetStatsJson(snifDoc, false);
            String snifJson;
            serializeJson(snifDoc, snifJson);
            webServerBroadcast("sniffer", snifJson.c_str());

            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    s_server.on("/api/stress/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!isReqAuth(req)) { req->send(401); return; }
        JsonDocument doc;
        doc["active"] = stressTestIsActive();
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    s_server.on(
        "/api/ping", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            
            bool stop = doc["stop"] | false;
            if (stop) {
                pingStop();
                req->send(200, "application/json", "{\"ok\":true}");
                return;
            }

            String target = doc["target"] | "";
            int count = doc["count"] | 4;
            
            if (target.length() > 0) {
                pingStart(target, count);
                req->send(200, "application/json", "{\"ok\":true}");
            } else {
                req->send(400, "application/json", "{\"error\":\"Missing target\"}");
            }
        }
    );

    s_server.on(
        "/api/stress/control", HTTP_POST,
        [](AsyncWebServerRequest* req) { if (!isReqAuth(req)) req->send(401); },
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            if (!isReqAuth(req)) return;
            JsonDocument doc;
            deserializeJson(doc, data, len);
            bool active = doc["active"] | false;
            if (active) {
                StressTestConfig config;
                String typeStr = doc["type"] | "deauth";
                if (typeStr == "deauth") config.type = STRESS_TYPE_DEAUTH;
                else if (typeStr == "beacon") config.type = STRESS_TYPE_BEACON_FLOOD;
                else if (typeStr == "probe") config.type = STRESS_TYPE_PROBE_FLOOD;
                else config.type = STRESS_TYPE_DEAUTH;

                config.targetMac = doc["targetMac"] | "";
                config.clientMac = doc["clientMac"] | "FF:FF:FF:FF:FF:FF";
                config.channel = doc["channel"] | 1;
                config.rate = doc["rate"] | 10;
                config.active = true;
                stressTestStart(config);
            } else {
                stressTestStop();
            }
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
    s_server.onNotFound([](AsyncWebServerRequest* req) {
        if (req->method() == HTTP_OPTIONS) {
            req->send(200);
            return;
        }
        const String& url = req->url();
        String host = req->host();
        String apIP = WiFi.softAPIP().toString();
        bool isViaAP = (req->client()->localIP() == WiFi.softAPIP());
        
        // 1. Handle workbox with hash (e.g. /workbox-9c191d2f.js)
        if (url.startsWith("/workbox-") && url.endsWith(".js")) {
            if (LittleFS.exists(url)) {
                AsyncWebServerResponse* resp = req->beginResponse(LittleFS, url, "application/javascript");
                resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                req->send(resp);
                return;
            }
        }

        // 2. Return 404 for missing static files (URLs with file extensions)
        int lastSlash = url.lastIndexOf('/');
        int lastDot = url.lastIndexOf('.');
        if (lastDot > lastSlash) {
            req->send(404, "text/plain", "Not Found");
            return;
        }

        // 3. For all other unmatched routes (probes, foreign domains, SPA routes)
        if (isViaAP && host != apIP && host != String(WIFI_HOSTNAME) + ".local" && host != "esp32-fan.local") {
            // Redirect foreign domains to portal root
            req->redirect("http://" + apIP + "/");
        } else {
            // Serve index.html for SPA routes on our own host
            req->send(LittleFS, "/index.html", "text/html");
        }
    });

    s_server.begin();
    Serial.printf("[WEB] Server started on port %d\n", WEB_SERVER_PORT);
    
    // Initial broadcast to ensure any early clients get data
    broadcastStatus();

    // Also broadcast sniffer and stress status immediately
    JsonDocument snifDoc;
    snifferGetStatsJson(snifDoc, false);
    String snifJson;
    serializeJson(snifDoc, snifJson);
    webServerBroadcast("sniffer", snifJson.c_str());

    JsonDocument stressDoc;
    stressDoc["active"] = stressTestIsActive();
    String stressJson;
    serializeJson(stressDoc, stressJson);
    webServerBroadcast("stress", stressJson.c_str());
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

        // Always broadcast sniffer status to keep UI in sync
        JsonDocument snifDoc;
        snifferGetStatsJson(snifDoc, false); // No logs, just stats and devices
        String snifJson;
        serializeJson(snifDoc, snifJson);
        webServerBroadcast("sniffer", snifJson.c_str());

        // Always broadcast stress status to keep UI in sync
        JsonDocument stressDoc;
        stressDoc["active"] = stressTestIsActive();
        String stressJson;
        serializeJson(stressDoc, stressJson);
        webServerBroadcast("stress", stressJson.c_str());

        // Broadcast any pending device updates
        String deviceJson = snifferGetPendingDevicesJson();
        if (deviceJson.length() > 0) {
            webServerBroadcast("devices", deviceJson.c_str());
        }
    }

    if (millis() - s_lastLogBroadcast >= 250) {
        s_lastLogBroadcast = millis();
        String logJson;
        bool hasLogs = false;
        int logCount = 0;
        
        {
            std::lock_guard<std::mutex> lock(s_logMutex);
            if (s_pendingLogs.is<JsonArray>() && s_pendingLogs.as<JsonArray>().size() > 0) {
                logCount = s_pendingLogs.as<JsonArray>().size();
                serializeJson(s_pendingLogs, logJson);
                s_pendingLogs.clear();
                s_pendingLogs.to<JsonArray>();
                hasLogs = true;
            }
        }
        
        if (hasLogs) {
            Serial.printf("[LOOP] Broadcasting %d pending logs to SSE clients\n", logCount);
            webServerBroadcast("logs", logJson.c_str());
        }
    }
}
