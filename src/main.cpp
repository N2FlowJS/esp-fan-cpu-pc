#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "fan_control.h"
#include "web_server.h"
#include "wifi_sniffer.h"
#include <Preferences.h>
// ============================================================
//  ESP32-S3 CPU Fan Controller – Entry Point
//  Dual WiFi: vừa phát AP (hotspot), vừa kết nối STA (router)
// ============================================================

static DNSServer s_dns;
static String    s_staIP = "";   // IP trên mạng STA (nếu kết nối được)

// ── Expose STA IP cho web_server.cpp ─────────────────────────
const String& getStaIP() { return s_staIP; }

// ─────────────────────────────────────────────────────────────
static void startWiFi() {
    // AP + STA đồng thời
    WiFi.mode(WIFI_AP_STA);

    // --- 1. Khởi động Access Point ---
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);

    // DNS captive portal (chỉ trên AP)
    s_dns.setTTL(300);
    s_dns.start(53, "*", apIP);

    Serial.println("========================================");
    Serial.printf("  [AP] SSID    : %s\n", WIFI_AP_SSID);
    Serial.printf("  [AP] Password: %s\n", WIFI_AP_PASS);
    Serial.printf("  [AP] IP      : http://%s\n", WIFI_AP_IP);

    // --- 2. Kết nối vào mạng STA (nếu đã cấu hình) ---
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
        Serial.printf("  [STA] Kết nối → %s ...\n", sta_ssid.c_str());
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());

        // Chờ tối đa 10 giây
        uint8_t tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries < 20) {
            delay(500);
            Serial.print(".");
            tries++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            s_staIP = WiFi.localIP().toString();
            Serial.printf("  [STA] Đã kết nối! IP: http://%s\n", s_staIP.c_str());
            WiFi.setAutoReconnect(true); // Tự động kết nối lại nếu rớt mạng
        } else {
            Serial.println("  [STA] Không kết nối được – chỉ dùng AP");
            WiFi.disconnect(false);   // Không làm ảnh hưởng AP
        }
    }

    // --- 3. Khởi động mDNS (esp32-fan.local) ---
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

    fanSetup(); // Hàm này sẽ tự nạp cấu hình đã lưu từ Flash (mặc định AUTO)
    startWiFi();
    webServerSetup();

    Serial.println("[SYS] Ready.");
}

void loop() {
    s_dns.processNextRequest();   // captive portal DNS
    fanLoop();
    webServerLoop();
    
    // --- Xử lý lệnh Serial điều khiển Sniffer ---
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.startsWith("sniffer_start")) {
            String arg = cmd.substring(13);
            arg.trim();
            uint8_t channel = 0; // Mặc định tự động nhảy kênh
            bool valid = true;
            if (arg.length() > 0) {
                if (arg != "hop") {
                    int val = arg.toInt();
                    if (val >= 1 && val <= 13) {
                        channel = val;
                    } else if (val == 0 && arg == "0") {
                        channel = 0;
                    } else {
                        Serial.println("[ERR] Kenh khong hop le (1-13). Nhap 'sniffer_start [1-13]' hoac 'sniffer_start hop'/'sniffer_start 0' de tu dong nhay kenh.");
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
        }
    }
    
    // Vòng lặp tự động nhảy kênh của Sniffer (nếu đang chạy)
    snifferLoop();
    
    delay(10);
}
