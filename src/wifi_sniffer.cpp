#include "wifi_sniffer.h"
#include "wifi_sniffer_common.h"
#include "web_server.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <vector>
#include <algorithm>
#include <mutex>

// ── Global State Definition ──────────────────────────────────────────────────

bool s_sniffing = false;
bool s_hopEnabled = true;
bool s_concurrent = false;
uint8_t s_currentChannel = 1;

std::vector<SnifferDevice> s_devices;
std::vector<SnifferPacketLog> s_packetLogs;
std::mutex s_snifferMutex;

// ── Statistics Counters ───────────────────────────────────────────────────────

volatile uint32_t s_packetCount = 0;
volatile uint32_t s_beaconCount = 0;
volatile uint32_t s_probeReqCount = 0;
volatile uint32_t s_deauthCount = 0;
volatile uint32_t s_dataCount = 0;
volatile uint32_t s_otherCount = 0;

volatile uint32_t s_arpCount = 0;
volatile uint32_t s_eapolCount = 0;
volatile uint32_t s_dnsCount = 0;
volatile uint32_t s_dhcpCount = 0;
volatile uint32_t s_mdnsCount = 0;
volatile uint32_t s_icmpCount = 0;
volatile uint32_t s_tcpCount = 0;
volatile uint32_t s_udpCount = 0;

volatile uint32_t s_rollingDeauthCount = 0;

// ── Local State (for hopping/warnings) ────────────────────────────────────────

static uint32_t s_lastHopTime = 0;
static uint32_t s_lastWarningTime = 0;

// ── Core Logging Function ─────────────────────────────────────────────────────

void addPacketLog(const String& proto, const String& subtype,
                  const String& src, const String& dst,
                  int rssi, int len, const String& info,
                  const char* srcMac, const char* dstMac,
                  const uint8_t* rawPayload, int rawLen) {
    std::lock_guard<std::mutex> lock(s_snifferMutex);
    SnifferPacketLog entry;
    entry.proto     = proto;
    entry.subtype   = subtype;
    entry.src       = src;
    entry.dst       = dst;
    entry.rssi      = rssi;
    entry.len       = len;
    entry.info      = info;
    entry.timestamp = millis();
    entry.channel   = s_currentChannel;

    // Fill MAC addresses
    if (srcMac && strlen(srcMac) > 0) {
        entry.srcMac = srcMac;
    } else if (rawPayload && rawLen >= 16) {
        char sMac[18], dMac[18];
        snprintf(dMac, sizeof(dMac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 rawPayload[4], rawPayload[5], rawPayload[6], rawPayload[7], rawPayload[8], rawPayload[9]);
        snprintf(sMac, sizeof(sMac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 rawPayload[10], rawPayload[11], rawPayload[12], rawPayload[13], rawPayload[14], rawPayload[15]);
        entry.srcMac = sMac;
        entry.dstMac = dMac;
    } else {
        if (src.indexOf(':') > 0) entry.srcMac = src;
        else entry.srcMac = "";
    }

    if (dstMac && strlen(dstMac) > 0) {
        entry.dstMac = dstMac;
    } else if (entry.dstMac.length() == 0) {
        if (dst.indexOf(':') > 0 || dst == "Broadcast") entry.dstMac = dst;
        else entry.dstMac = "";
    }

    // Convert raw payload to Hex Dump
    if (rawPayload && rawLen > 0) {
        int dumpLen = (rawLen > 48) ? 48 : rawLen;
        char hexBuf[150];
        int pos = 0;
        for (int i = 0; i < dumpLen; i++) {
            pos += snprintf(&hexBuf[pos], sizeof(hexBuf) - pos, "%02X ", rawPayload[i]);
        }
        if (pos > 0 && hexBuf[pos-1] == ' ') {
            hexBuf[pos-1] = '\0';
        }
        entry.rawHex = String(hexBuf);
    } else {
        entry.rawHex = "";
    }

    s_packetLogs.push_back(entry);
    if (s_packetLogs.size() > 20) {
        s_packetLogs.erase(s_packetLogs.begin());
    }

    // Broadcast via SSE
    webServerBroadcastLog(proto, subtype, src, dst, rssi, len, info, s_currentChannel, entry.srcMac, entry.dstMac, entry.rawHex);
}

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

static void wifi_promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    s_packetCount++;

    int len = pkt->rx_ctrl.sig_len;
    uint8_t* payload = pkt->payload;
    if (len < 24) {
        s_otherCount++;
        return;
    }

    uint8_t frame_control = payload[0];
    int rssi = pkt->rx_ctrl.rssi;

    uint8_t type_val = frame_control & 0x0C;
    if (type_val == 0x00) { // Management Frame
        dispatchMgmtFrame(frame_control, payload, len, rssi);
    } else if (type_val == 0x08) { // Data Frame
        dispatchDataFrame(frame_control, payload, len, rssi);
    } else {
        s_otherCount++;
    }
}

// ── Public Control APIs ───────────────────────────────────────────────────────

void snifferStart(uint8_t channel, bool concurrent) {
    if (s_sniffing) return;
    
    s_concurrent = concurrent;
    {
        std::lock_guard<std::mutex> lock(s_snifferMutex);
        s_devices.clear();
    }
    
    // Reset statistics counters
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
        s_currentChannel = WiFi.channel();
        if (s_currentChannel == 0) s_currentChannel = 1;
        s_hopEnabled = false;
        
        Serial.printf("[SNIFFER] Khoi dong Sniffer DONG THOI (CONCURRENT) tren kenh %d...\n", s_currentChannel);
        
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx_cb);
        
        wifi_promiscuous_filter_t filter;
        filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
        esp_wifi_set_promiscuous_filter(&filter);
        
        esp_wifi_set_promiscuous(true);
        s_sniffing = true;
    } else {
        if (channel == 0) {
            s_currentChannel = 1;
            s_hopEnabled = true;
            Serial.println("[SNIFFER] Khoi dong Sniffer chuyen dung voi che do NHAY KENH...");
        } else {
            s_currentChannel = constrain(channel, 1, 13);
            s_hopEnabled = false;
            Serial.printf("[SNIFFER] Khoi dong Sniffer chuyen dung tren kenh CO DINH %d...\n", s_currentChannel);
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
        filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
        esp_wifi_set_promiscuous_filter(&filter);
        
        esp_wifi_set_channel(s_currentChannel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(true);
        
        s_sniffing = true;
    }
    
    Serial.println("[SNIFFER] Sniffer dang hoat dong!");
}

void snifferStop() {
    if (!s_sniffing) return;
    
    esp_wifi_set_promiscuous(false);
    s_sniffing = false;
    {
        std::lock_guard<std::mutex> lock(s_snifferMutex);
        s_devices.clear();
    }
    
    if (s_concurrent) {
        Serial.println("[SNIFFER] Da dung Sniffer dong thoi.");
    } else {
        Serial.println("[SNIFFER] Da dung Sniffer offline. Restarting...");
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

void snifferGetStatsJson(JsonDocument& doc) {
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
    doc["icmp"] = (uint32_t)s_icmpCount;
    doc["tcp"] = (uint32_t)s_tcpCount;
    doc["udp"] = (uint32_t)s_udpCount;
    
    doc["jammingAlert"] = (s_rollingDeauthCount > 10);
    
    JsonArray devicesArray = doc["devices"].to<JsonArray>();
    
    std::lock_guard<std::mutex> lock(s_snifferMutex);
    for (const auto& dev : s_devices) {
        JsonObject obj = devicesArray.add<JsonObject>();
        obj["isAP"] = dev.isAP;
        
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5]);
        obj["mac"] = String(mac_str);
        obj["rssi"] = dev.rssi;
        obj["ssid"] = dev.ssid;
        obj["channel"] = dev.channel;
        obj["security"] = dev.security;
    }

    JsonArray logsArray = doc["logs"].to<JsonArray>();
    for (const auto& log : s_packetLogs) {
        JsonObject obj = logsArray.add<JsonObject>();
        obj["proto"] = log.proto;
        obj["subtype"] = log.subtype;
        obj["src"] = log.src;
        obj["dst"] = log.dst;
        obj["rssi"] = log.rssi;
        obj["len"] = log.len;
        obj["info"] = log.info;
        obj["channel"] = log.channel;
        obj["srcMac"] = log.srcMac;
        obj["dstMac"] = log.dstMac;
        obj["rawHex"] = log.rawHex;
        obj["time"] = (double)log.timestamp / 1000.0;
    }
}

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
