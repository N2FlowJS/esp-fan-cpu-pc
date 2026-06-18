#include "wifi_sniffer.h"
#include "wifi_sniffer_common.h"
#include "web_server.h"
#include "sniffer_filters.h"
#include "sniffer_logger.h"
#include "sniffer_worker.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <vector>
#include <algorithm>
#include <mutex>
#include <Preferences.h>
#include "rgb_led.h"

#include <array>

void snifferLog(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.println(buf);
}

void snifferSetup() {
    // Initialize filter subsystem
    snifferInitFilters();

    // Add the ESP's own MACs to owner exclusion list
    uint8_t macSTA[6];
    uint8_t macAP[6];
    WiFi.macAddress(macSTA);
    WiFi.softAPmacAddress(macAP);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macSTA[0], macSTA[1], macSTA[2], macSTA[3], macSTA[4], macSTA[5]);
    snifferAddOwnerMac(String(buf));
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macAP[0], macAP[1], macAP[2], macAP[3], macAP[4], macAP[5]);
    snifferAddOwnerMac(String(buf));
}

// Packet queue and worker moved to sniffer_worker.{h,cpp}

// ── Global State Definition ──────────────────────────────────────────────────

bool s_sniffing = false;
bool s_hopEnabled = true;
bool s_concurrent = false;
uint8_t s_currentChannel = 1;

std::vector<SnifferDevice> s_devices;
std::vector<SnifferPacketLog> s_packetLogs;
std::mutex s_snifferMutex;

static JsonDocument s_pendingDevices;
static std::mutex   s_deviceMutex;

void addDeviceUpdate(const SnifferDevice& dev) {
    std::lock_guard<std::mutex> lock(s_deviceMutex);
    if (!s_pendingDevices.is<JsonArray>()) {
        s_pendingDevices.to<JsonArray>();
    }
    
    JsonArray arr = s_pendingDevices.as<JsonArray>();
    // Prevent buffer from growing too large, but 100 is safe for S3
    if (arr.size() >= 100) return;

    JsonObject obj = arr.add<JsonObject>();
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5]);
    obj["mac"] = String(mac_str);
    obj["rssi"] = dev.rssi;
    obj["ssid"] = dev.ssid;
    obj["isAP"] = dev.isAP;
    obj["channel"] = dev.channel;
    if (dev.security.length()) obj["security"] = dev.security;
    if (dev.wifiGen.length())  obj["wifiGen"]  = dev.wifiGen;
    if (dev.clients >= 0)      obj["clients"]  = dev.clients;
    if (dev.utilization >= 0)  obj["utilization"] = dev.utilization;
    if (dev.vendor.length())   obj["vendor"]   = dev.vendor;
    obj["packetCount"] = dev.packetCount;
    obj["lastSeen"] = (double)dev.lastSeen / 1000.0;
}

String snifferGetPendingDevicesJson() {
    std::lock_guard<std::mutex> lock(s_deviceMutex);
    if (!s_pendingDevices.is<JsonArray>() || s_pendingDevices.as<JsonArray>().size() == 0) {
        return "";
    }
    
    String json;
    serializeJson(s_pendingDevices, json);
    s_pendingDevices.clear();
    return json;
}

// ── Statistics Counters ───────────────────────────────────────────────────────

#include <atomic>

std::atomic<uint32_t> s_packetCount(0);
std::atomic<uint32_t> s_beaconCount(0);
std::atomic<uint32_t> s_probeReqCount(0);
std::atomic<uint32_t> s_deauthCount(0);
std::atomic<uint32_t> s_dataCount(0);
std::atomic<uint32_t> s_otherCount(0);

std::atomic<uint32_t> s_arpCount(0);
std::atomic<uint32_t> s_eapolCount(0);
std::atomic<uint32_t> s_dnsCount(0);
std::atomic<uint32_t> s_dhcpCount(0);
std::atomic<uint32_t> s_mdnsCount(0);
std::atomic<uint32_t> s_llmnrCount(0);
std::atomic<uint32_t> s_nbnsCount(0);
std::atomic<uint32_t> s_ssdpCount(0);
std::atomic<uint32_t> s_quicCount(0);
std::atomic<uint32_t> s_icmpCount(0);
std::atomic<uint32_t> s_tcpCount(0);
std::atomic<uint32_t> s_udpCount(0);
std::atomic<uint32_t> s_mqttCount(0);

std::atomic<uint32_t> s_rollingDeauthCount(0);

// ── Local State (for hopping/warnings) ────────────────────────────────────────

static uint32_t s_lastHopTime = 0;
static uint32_t s_lastWarningTime = 0;
// logging and serial/pcap moved to sniffer_logger.cpp
// ── Public API Queries ────────────────────────────────────────────────────────

bool snifferIsActive() {
    return s_sniffing;
}

bool snifferIsHopping() {
    return s_sniffing && s_hopEnabled;
}

uint8_t snifferGetChannel() {
    return s_currentChannel;
}

// ── Promiscuous Callback ──────────────────────────────────────────────────────

static uint32_t s_rawRxCount = 0;
static void wifi_promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_sniffing) return;
    
    // DEBUG
    static uint32_t lastDebug = 0;
    if (millis() - lastDebug > 5000) {
        Serial.printf("[RX_CB] RX CB CALLED (rawRxCount=%lu)\n", s_rawRxCount);
        lastDebug = millis();
    }
    
    s_rawRxCount++;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    // Log every 100 packets on RX side
    if (s_rawRxCount % 100 == 0) {
        Serial.printf("[RX] Raw RX count: %lu, len: %d, type: %d\n", s_rawRxCount, len, type);
    }

    // Filter out ESP's own traffic and excluded devices
    if (len >= 16) {
        uint8_t frame_control = payload[0];
        uint8_t type_val = frame_control & 0x0C;

        // Filter out Owner MACs (ESP itself and connected clients)
        // BUT allow Management Frames (type 0x00) to pass through to show AP activity/Beacons
        if (type_val != 0x00) {
            const uint8_t* ra = payload + 4;
            const uint8_t* ta = payload + 10;
            const uint8_t* addr3 = (len >= 22) ? (payload + 16) : nullptr;
            if (isOwnerMac(ra) || isOwnerMac(ta) || (addr3 && isOwnerMac(addr3))) return;
        }

        // Apply Whitelist / Blacklist (Always apply to all types)
        if (isMacFiltered(payload + 4) || isMacFiltered(payload + 10) || (len >= 22 && isMacFiltered(payload + 16))) {
            return;
        }
    }

    if (snifferIsPcapSerialActive()) {
        // Safety: If Serial is no longer connected (host closed port), auto-stop to avoid congestion
        if (!Serial) {
            snifferSetPcapSerial(false);
            return;
        }

        uint32_t now_ms = millis();
        uint32_t ts_sec = now_ms / 1000;
        uint32_t ts_usec = (now_ms % 1000) * 1000;
        uint32_t incl_len = len;
        uint32_t orig_len = len;
        uint8_t hdr[16];
        memcpy(&hdr[0], &ts_sec, 4);
        memcpy(&hdr[4], &ts_usec, 4);
        memcpy(&hdr[8], &incl_len, 4);
        memcpy(&hdr[12], &orig_len, 4);

        Serial.write(hdr, sizeof(hdr));
        Serial.write(payload, len);
        // Do not process further to save CPU during raw streaming
        return;
    }

    s_packetCount = s_packetCount + 1;

    if (len < 24) {
        s_otherCount = s_otherCount + 1;
        return;
    }

    // Queue packet for processing via sniffer_worker API
    if (isPacketQueueCreated()) {
        QueuedPacket qp;
        qp.timestamp = millis();
        qp.rssi = pkt->rx_ctrl.rssi;
        qp.len = (len > PACKET_CAPTURE_LEN) ? PACKET_CAPTURE_LEN : len;
        memcpy(qp.payload, payload, qp.len);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (snifferEnqueueFromISR(&qp, &xHigherPriorityTaskWoken) != pdPASS) {
            Serial.printf("[WARNING] Sniffer queue overflow - packet dropped!\n");
        } else {
            static uint32_t queuedCount = 0;
            queuedCount++;
            if (queuedCount % 50 == 0) {
                Serial.printf("[QUEUE] Queued %lu packets (fc=0x%02X, len=%d, rssi=%d)\n", queuedCount, payload[0], len, pkt->rx_ctrl.rssi);
            }
        }

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    } else {
        static uint32_t noQueueMsg = 0;
        noQueueMsg++;
        if (noQueueMsg == 1) {
            Serial.println("[ERROR] Packet queue is NULL in RX callback!");
        }
    }
}

// ── Public Control APIs ───────────────────────────────────────────────────────

void snifferStart(uint8_t channel, bool concurrent) {
    if (s_sniffing) return;
    
    snifferSetup();

    // Create queue and start worker task via sniffer_worker API
    if (!isPacketQueueCreated()) {
        if (!createPacketQueue(MAX_QUEUED_PACKETS)) {
            Serial.println("[SNIFFER] FATAL: Queue creation failed! Not enough heap memory. Aborting sniffer start.");
            return;
        }
    }

    if (!startSnifferWorker()) {
        Serial.println("[SNIFFER] ERROR: Worker task creation failed!");
        deletePacketQueue();
        return;
    }

    s_concurrent = concurrent;
    {
        std::lock_guard<std::mutex> lock(s_snifferMutex);
        s_devices.clear();
    }
    
    // ... reset statistics ...
    s_packetCount = 0;
    s_beaconCount = 0;
    s_probeReqCount = 0;
    s_deauthCount = 0;
    s_dataCount = 0;
    s_otherCount = 0;
    
    s_arpCount = 0;
    s_eapolCount = 0;
    s_dnsCount = 0;
    s_dhcpCount = 0;
    s_mdnsCount = 0;
    s_llmnrCount = 0;
    s_nbnsCount = 0;
    s_ssdpCount = 0;
    s_quicCount = 0;
    s_icmpCount = 0;
    s_tcpCount = 0;
    s_udpCount = 0;
    
    s_rollingDeauthCount = 0;
    s_lastWarningTime = millis();
    s_lastHopTime = millis();
    
    {
        std::lock_guard<std::mutex> lock(s_snifferMutex);
        s_packetLogs.clear();
    }
    
    if (s_concurrent) {
        // If caller requested channel 0 we should enable hopping even in concurrent
        // mode so the sniffer observes all channels. Otherwise operate on a
        // fixed channel while keeping WiFi functions active.
        if (channel == 0) {
            s_hopEnabled = true;
            s_currentChannel = WiFi.channel();
            if (s_currentChannel == 0) s_currentChannel = 1;
            snifferLog("[SNIFFER] Khoi dong Sniffer DONG THOI (CONCURRENT) tren kenh %d (HOPPING)...", s_currentChannel);
        } else {
            s_hopEnabled = false;
            s_currentChannel = constrain(channel, 1, 13);
            snifferLog("[SNIFFER] Khoi dong Sniffer DONG THOI (CONCURRENT) tren kenh CO DINH %d...", s_currentChannel);
        }

        // Disable power save to ensure we don't miss packets while sleeping
        esp_wifi_set_ps(WIFI_PS_NONE);

        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx_cb);

        wifi_promiscuous_filter_t filter;
        // Use ALL to ensure we don't miss any frame types
        filter.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
        esp_wifi_set_promiscuous_filter(&filter);

        esp_wifi_set_channel(s_currentChannel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(true);
        s_sniffing = true;
    } else {
        if (channel == 0) {
            s_currentChannel = 1;
            s_hopEnabled = true;
            snifferLog("[SNIFFER] Khoi dong Sniffer chuyen dung voi che do NHAY KENH...");
        } else {
            s_currentChannel = constrain(channel, 1, 13);
            s_hopEnabled = false;
            snifferLog("[SNIFFER] Khoi dong Sniffer chuyen dung tren kenh CO DINH %d...", s_currentChannel);
        }
        
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        delay(100);
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        esp_wifi_set_mode(WIFI_MODE_NULL);
        esp_wifi_start();
        
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx_cb);
        
        wifi_promiscuous_filter_t filter;
        filter.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
        esp_wifi_set_promiscuous_filter(&filter);
        
        esp_wifi_set_channel(s_currentChannel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(true);
        
        s_sniffing = true;
    }
    
    ledSetStatusColor(LED_COLOR_SNIFFING, true); // Blinking blue for sniffing
    snifferLog("[SNIFFER] Sniffer is active!");
}

void snifferStop() {
    if (!s_sniffing) return;
    
    // CRITICAL SEQUENCE: Must prevent race conditions between ISR and cleanup
    // Step 1: Set flag FIRST to signal callback to exit early
    s_sniffing = false;
    
    // Step 2: Small delay to let any in-flight ISRs check the flag and complete
    // This is critical - without this, the callback might still execute and access
    // deleted queue/task, causing crashes or watchdog timeouts
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Step 3: Now safe to unregister the callback
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    
    // Step 4: Disable promiscuous mode
    esp_wifi_set_promiscuous(false);
    
    // Step 5: Clean up worker task and queue BEFORE WiFi state transitions
    // This frees CPU resources so WiFi driver can complete state transitions
    // Stop worker and delete packet queue via sniffer_worker API
    stopSnifferWorker();
    deletePacketQueue();
    
    if (s_concurrent) {
        // For concurrent mode: restore normal WiFi operation
        // This is crucial - WiFi driver must complete state transition without blocking async_tcp
        
        // Disable power save mode to ensure clean transition
        esp_wifi_set_ps(WIFI_PS_NONE);
        
        // Let WiFi driver settle (allows async_tcp and other tasks to run)
        vTaskDelay(pdMS_TO_TICKS(100));
        
        ledSetStatusColor(LED_COLOR_NORMAL, false);
        snifferLog("[SNIFFER] Concurrent Sniffer stopped. WiFi restored to normal mode.");
    } else {
        snifferLog("[SNIFFER] Offline Sniffer stopped. Restarting...");
        esp_wifi_stop();
        ESP.restart(); 
    }
}

void snifferSetChannel(uint8_t channel) {
    if (!s_sniffing) return;
    s_currentChannel = constrain(channel, 1, 13);
    esp_wifi_set_channel(s_currentChannel, WIFI_SECOND_CHAN_NONE);
}

void snifferPrintStats() {
    std::lock_guard<std::mutex> lock(s_snifferMutex);
    Serial.println("\n=== THONG KE GOI TIN WIFI SNIFFER ===");
    Serial.printf("Trang thai: %s\n", s_sniffing ? "DANG QUET (ACTIVE)" : "DUNG (INACTIVE)");
    if (s_sniffing) {
        Serial.printf("Kenh hien tai: %d | Che do: %s | Tu dong nhay kenh: %s\n", 
                      s_currentChannel, 
                      s_concurrent ? "Dong thoi (Concurrent)" : "Chuyen dung (Dedicated)",
                      s_hopEnabled ? "BAT (ON)" : "TAT (OFF)");
    }
    Serial.printf("Tong so goi tin bat duoc: %u\n", (uint32_t)s_packetCount);
    Serial.printf("  - Beacon Frames:         %u\n", (uint32_t)s_beaconCount);
    Serial.printf("  - Probe Requests:        %u\n", (uint32_t)s_probeReqCount);
    Serial.printf("  - Deauth/Disassociation: %u\n", (uint32_t)s_deauthCount);
    Serial.printf("  - Data Packets (chung):  %u\n", (uint32_t)s_dataCount);
    Serial.printf("    + ARP:                 %u\n", (uint32_t)s_arpCount);
    Serial.printf("    + EAPOL (Handshake):   %u\n", (uint32_t)s_eapolCount);
    Serial.printf("    + DNS:                 %u\n", (uint32_t)s_dnsCount);
    Serial.printf("    + DHCP:                %u\n", (uint32_t)s_dhcpCount);
    Serial.printf("    + mDNS:                %u\n", (uint32_t)s_mdnsCount);
    Serial.printf("    + LLMNR:               %u\n", (uint32_t)s_llmnrCount);
    Serial.printf("    + NBNS:                %u\n", (uint32_t)s_nbnsCount);
    Serial.printf("    + SSDP:                %u\n", (uint32_t)s_ssdpCount);
    Serial.printf("    + QUIC (HTTP/3):       %u\n", (uint32_t)s_quicCount);
    Serial.printf("    + ICMP (Ping):         %u\n", (uint32_t)s_icmpCount);
    Serial.printf("    + TCP:                 %u\n", (uint32_t)s_tcpCount);
    Serial.printf("    + UDP (khac):          %u\n", (uint32_t)s_udpCount);
    Serial.printf("  - Goi tin khac:          %u\n", (uint32_t)s_otherCount);
    
    Serial.printf("\nDanh sach %u thiet bi trong bo nho dem:\n", s_devices.size());
    Serial.println("----------------------------------------------------------------------------------------------------");
    Serial.printf("%-20s | %-17s | %-8s | %-4s | %s\n", "Loai thiet bi", "MAC Address", "RSSI (dBm)", "Kenh", "Thong tin bo sung (SSID)");
    Serial.println("----------------------------------------------------------------------------------------------------");
    for (const auto& dev : s_devices) {
        const char* type_str = dev.isAP ? "Access Point (AP)" : "Client Station";
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5]);
        
        Serial.printf("%-20s | %s | %-8d | %-4s | %s\n", 
                      type_str, mac_str, dev.rssi, 
                      dev.isAP ? String(dev.channel).c_str() : "-", 
                      dev.isAP ? (dev.ssid + " (" + dev.security + ")").c_str() : (dev.ssid.length() > 0 ? ("Dang tim WiFi: " + dev.ssid).c_str() : "(Tim kiem chung)"));
    }
    Serial.println("=====================================\n");
}

void snifferGetStatsJson(JsonDocument& doc, bool includeLogs) {
    doc["active"] = s_sniffing;
    doc["hopping"] = s_hopEnabled;
    doc["channel"] = s_currentChannel;
    doc["concurrent"] = s_concurrent;
    
    doc["packets"] = (uint32_t)s_packetCount;
    doc["beacons"] = (uint32_t)s_beaconCount;
    doc["probes"] = (uint32_t)s_probeReqCount;
    doc["deauths"] = (uint32_t)s_deauthCount;
    doc["data"] = (uint32_t)s_dataCount;
    doc["other"] = (uint32_t)s_otherCount;
    
    doc["arp"] = (uint32_t)s_arpCount;
    doc["eapol"] = (uint32_t)s_eapolCount;
    doc["dns"] = (uint32_t)s_dnsCount;
    doc["dhcp"] = (uint32_t)s_dhcpCount;
    doc["mdns"] = (uint32_t)s_mdnsCount;
    doc["llmnr"] = (uint32_t)s_llmnrCount;
    doc["nbns"] = (uint32_t)s_nbnsCount;
    doc["ssdp"] = (uint32_t)s_ssdpCount;
    doc["quic"] = (uint32_t)s_quicCount;
    doc["icmp"] = (uint32_t)s_icmpCount;
    doc["tcp"] = (uint32_t)s_tcpCount;
    doc["udp"] = (uint32_t)s_udpCount;
    doc["mqtt"] = (uint32_t)s_mqttCount;

    doc["jammingAlert"] = (s_rollingDeauthCount > 10);
    
    std::vector<SnifferPacketLog> logsCopy;
    {
        std::lock_guard<std::mutex> lock(s_snifferMutex);
        if (includeLogs) logsCopy = s_packetLogs;
    }

    if (includeLogs) {
        JsonArray logsArray = doc["logs"].to<JsonArray>();
        for (const auto& log : logsCopy) {
            JsonObject obj = logsArray.add<JsonObject>();
            obj["time"] = log.timestamp;
            obj["proto"] = log.proto;
            if (log.subtype.length() > 0) obj["subtype"] = log.subtype;
            obj["src"] = log.src;
            obj["dst"] = log.dst;
            obj["rssi"] = log.rssi;
            obj["len"] = log.len;
            obj["info"] = log.info;
            if (log.srcMac.length() > 0) obj["srcMac"] = log.srcMac;
            if (log.dstMac.length() > 0) obj["dstMac"] = log.dstMac;
            if (log.rawHex.length() > 0) obj["rawHex"] = log.rawHex;
            if (log.ttl != -1) obj["ttl"] = log.ttl;
            if (log.srcPort != -1) obj["srcPort"] = log.srcPort;
            if (log.dstPort != -1) obj["dstPort"] = log.dstPort;
        }
    }
}

// Filter config and owner-MAC helpers implemented in sniffer_filters.cpp

void snifferLoop() {
    if (!s_sniffing) return;
    
    uint32_t now = millis();
    
    if (now - s_lastWarningTime >= 2000) {
        if (s_rollingDeauthCount > 10) {
            Serial.printf("\n********************************************************************************\n");
            Serial.printf("[CANH BAO] PHAT HIEN PHONG SONG/TAN CONG DEAUTH (WIFI JAMMING)!\n");
            Serial.printf("Nhan duoc %u goi tin Deauth/Disassoc tren kenh %d trong 2 giay qua.\n",
                          (uint32_t)s_rollingDeauthCount, s_currentChannel);
            Serial.printf("********************************************************************************\n\n");
            ledSetStatusColor(LED_COLOR_JAMMING, true); // Blinking red for jamming
        } else {
            ledSetStatusColor(LED_COLOR_SNIFFING, true); // Return to blinking blue if no jamming
        }
        s_rollingDeauthCount = 0;
        s_lastWarningTime = now;
    }

    if (s_hopEnabled) {
        if (now - s_lastHopTime >= 3000) {
            s_lastHopTime = now;
            uint8_t nextChan = (s_currentChannel % 13) + 1;
            snifferSetChannel(nextChan);
        }
    }
}
