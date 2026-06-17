#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "fan_control.h"
#include "web_server.h"
#include "wifi_sniffer.h"
#include "rgb_led.h"
#include <Preferences.h>
// ============================================================
//  ESP32-S3 CPU Fan Controller – Entry Point
//  Dual WiFi: AP (hotspot) and STA (router) simultaneous connection
// ============================================================

static DNSServer s_dns;
static String    s_staIP = "";   // IP on STA network (if connected)

// ── Expose STA IP to web_server.cpp ─────────────────────────
const String& getStaIP() { return s_staIP; }

// ─────────────────────────────────────────────────────────────
static void startWiFi() {
    // AP + STA simultaneously
    WiFi.mode(WIFI_AP_STA);

    // --- 1. Start Access Point ---
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);

    // DNS captive portal (AP only)
    s_dns.setTTL(300);
    s_dns.start(53, "*", apIP);

    Serial.println("========================================");
    Serial.printf("  [AP] SSID    : %s\n", WIFI_AP_SSID);
    Serial.printf("  [AP] Password: %s\n", WIFI_AP_PASS);
    Serial.printf("  [AP] IP      : http://%s\n", WIFI_AP_IP);

    // --- 2. Connect to STA network (if configured) ---
    Preferences prefs;
    prefs.begin("wifi", false); // false = read-write (creates namespace if not exists)
    String sta_ssid = "";
    String sta_pass = "";
    if (prefs.isKey("ssid")) {
        sta_ssid = prefs.getString("ssid", "");
    }
    if (prefs.isKey("pass")) {
        sta_pass = prefs.getString("pass", "");
    }
    prefs.end();

    if (sta_ssid.length() > 0) {
        Serial.printf("  [STA] Connecting → %s ...\n", sta_ssid.c_str());
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());

        // Wait up to 10 seconds
        uint8_t tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries < 20) {
            delay(500);
            Serial.print(".");
            tries++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            s_staIP = WiFi.localIP().toString();
            Serial.printf("  [STA] Connected! IP: http://%s\n", s_staIP.c_str());
            WiFi.setAutoReconnect(true); // Auto-reconnect if disconnected
        } else {
            Serial.println("  [STA] Connection failed – AP mode only");
            WiFi.disconnect(false);   // Don't affect AP
        }
    }

    // --- 3. Start mDNS (esp32-fan.local) ---
    if (MDNS.begin(WIFI_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("  [DNS] mDNS started: http://%s.local\n", WIFI_HOSTNAME);
    } else {
        Serial.println("  [DNS] Error setting up mDNS!");
    }

    Serial.println("========================================");
}

// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("========================================");
    Serial.println("  ESP32-S3 CPU Fan Controller  v1.0");
    Serial.println("========================================");

    randomSeed(micros());
    ledSetup();
    fanSetup(); // This function loads saved config from Flash (default AUTO)
    startWiFi();
    webServerSetup();

    Serial.println("[SYS] Ready.");
}

void loop() {
    s_dns.processNextRequest();   // captive portal DNS
    fanLoop();
    webServerLoop();
    
    // --- Handle Serial commands for Sniffer control ---
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "pcap_start") {
            snifferSetPcapSerial(true);
        } else if (cmd == "pcap_stop") {
            snifferSetPcapSerial(false);
            Serial.println("\n[SYS] Exit PCAP mode.");
        } else if (cmd == "db_sync_start") {
            snifferSetJsonSerial(true);
            snifferPrintDevicesJson(); // Initial dump of devices
            Serial.println("{\"type\":\"sys\",\"msg\":\"DB Sync Started\"}");
        } else if (cmd == "db_sync_stop") {
            snifferSetJsonSerial(false);
            Serial.println("{\"type\":\"sys\",\"msg\":\"DB Sync Stopped\"}");
        } else if (cmd.startsWith("sniffer_start")) {

            String arg = cmd.substring(13);
            arg.trim();
            uint8_t channel = 0; // Default to auto channel hopping
            bool valid = true;
            if (arg.length() > 0) {
                if (arg != "hop") {
                    int val = arg.toInt();
                    if (val >= 1 && val <= 13) {
                        channel = val;
                    } else if (val == 0 && arg == "0") {
                        channel = 0;
                    } else {
                        Serial.println("[ERR] Invalid channel (1-13). Enter 'sniffer_start [1-13]' or 'sniffer_start hop'/'sniffer_start 0' for auto channel hopping.");
                        valid = false;
                    }
                }
            }
            if (valid) {
                snifferStart(channel);
            }
        } else if (cmd == "sniffer_stop") {
            snifferStop();
        } else if (cmd == "sniffer_status" || cmd == "sniffer_stats") {
            snifferPrintStats();
        } else if (cmd.startsWith("set_password")) {
            String newPass = cmd.substring(12);
            newPass.trim();
            if (newPass.length() >= 4) {
                Preferences prefs;
                prefs.begin("sys", false);
                prefs.putString("webpass", newPass);
                prefs.end();
                Serial.printf("[AUTH] Web password updated to: %s\n", newPass.c_str());
                webServerInvalidateSession();
            } else {
                Serial.println("[ERR] Password too short (min 4 chars).");
            }
        } else if (cmd == "reset_password") {
            Preferences prefs;
            prefs.begin("sys", false);
            prefs.remove("webpass");
            prefs.end();
            Serial.println("[AUTH] Web password reset to default (admin).");
            webServerInvalidateSession();
        } else if (cmd.startsWith("set_led_pin")) {
            String arg = cmd.substring(11);
            arg.trim();
            if (arg.length() > 0) {
                int pin = arg.toInt();
                ledSetPin(pin);
            } else {
                Serial.println("[ERR] Usage: set_led_pin <pin_number>");
            }
        }
    }
    
    // Sniffer auto channel hopping loop (if running)
    snifferLoop();
    
    ledLoop();
    
    delay(10);
}

