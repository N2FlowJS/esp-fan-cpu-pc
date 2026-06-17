#include "wifi_sniffer.h"
#include "wifi_sniffer_common.h"
#include "web_server.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <vector>
#include <algorithm>
#include <mutex>
#include <Preferences.h>
#include "rgb_led.h"

// ── ESP MAC Filtering ───────────────────────────────────────────────────────
static bool s_macsInitialized = false;

// Temporarily store MACs of clients viewing the web interface and the ESP's own MAC
static std::vector<String> s_ownerMacs;

// ── MAC Whitelist / Blacklist ──────────────────────────────────────────────
static std::vector<String> s_whitelist;
static std::vector<String> s_blacklist;

static void loadFilterConfig() {
    Preferences prefs;
    prefs.begin("sniffer", true);
    String wl = prefs.getString("whitelist", "");
    String bl = prefs.getString("blacklist", "");
    prefs.end();

    s_whitelist.clear();
    s_blacklist.clear();

    // Helper to parse comma/newline separated MACs
    auto parseMACs = [](const String& input, std::vector<String>& target) {
        int start = 0;
        int end = input.indexOf(',');
        while (end != -1) {
            String mac = input.substring(start, end);
            mac.trim();
            mac.toUpperCase();
            if (mac.length() > 0) target.push_back(mac);
            start = end + 1;
            end = input.indexOf(',', start);
        }
        String mac = input.substring(start);
        mac.trim();
        mac.toUpperCase();
        if (mac.length() > 0) target.push_back(mac);
    };

    parseMACs(wl, s_whitelist);
    parseMACs(bl, s_blacklist);
}

static bool isMacFiltered(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    String macStr = String(buf);

    // Blacklist check: If in blacklist, it's filtered
    for (const auto& m : s_blacklist) {
        if (m == macStr) return true;
    }

    // Whitelist check: If whitelist is not empty, only those in whitelist are allowed
    if (!s_whitelist.empty()) {
        bool found = false;
        for (const auto& m : s_whitelist) {
            if (m == macStr) {
                found = true;
                break;
            }
        }
        if (!found) return true; // Not in whitelist, so filtered
    }

    return false;
}

static void initializeEspMacs() {
    if (s_macsInitialized) return;
    
    uint8_t macSTA[6];
    uint8_t macAP[6];
    WiFi.macAddress(macSTA);
    WiFi.softAPmacAddress(macAP);
    
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macSTA[0], macSTA[1], macSTA[2], macSTA[3], macSTA[4], macSTA[5]);
    s_ownerMacs.push_back(String(buf));
    
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", macAP[0], macAP[1], macAP[2], macAP[3], macAP[4], macAP[5]);
    s_ownerMacs.push_back(String(buf));

    loadFilterConfig();
    s_macsInitialized = true;
}

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
volatile uint32_t s_llmnrCount = 0;
volatile uint32_t s_nbnsCount = 0;
volatile uint32_t s_ssdpCount = 0;
volatile uint32_t s_quicCount = 0;
volatile uint32_t s_icmpCount = 0;
volatile uint32_t s_tcpCount = 0;
volatile uint32_t s_udpCount = 0;
volatile uint32_t s_mqttCount = 0;

volatile uint32_t s_rollingDeauthCount = 0;

// ── Local State (for hopping/warnings) ────────────────────────────────────────

static uint32_t s_lastHopTime = 0;
static uint32_t s_lastWarningTime = 0;
static bool s_pcapSerialActive = false;

// ── PCAP Structures (IEEE 802.11) ─────────────────────────────────────────────
#pragma pack(push, 1)
struct pcap_global_header {
    uint32_t magic_number;  /* magic number */
    uint16_t version_major; /* major version number */
    uint16_t version_minor; /* minor version number */
    int32_t  thiszone;      /* GMT to local correction */
    uint32_t sigfigs;       /* accuracy of timestamps */
    uint32_t snaplen;       /* max length of captured packets, in octets */
    uint32_t network;       /* data link type (105 for IEEE 802.11) */
};

struct pcap_packet_header {
    uint32_t ts_sec;   /* timestamp seconds */
    uint32_t ts_usec;  /* timestamp microseconds */
    uint32_t incl_len; /* number of octets of packet saved in file */
    uint32_t orig_len; /* actual length of packet */
};
#pragma pack(pop)

void snifferSetPcapSerial(bool enable) {
    if (s_pcapSerialActive == enable) return;
    s_pcapSerialActive = enable;
    if (enable) {
        // Send PCAP Global Header
        pcap_global_header global_hdr;
        global_hdr.magic_number = 0xa1b2c3d4;
        global_hdr.version_major = 2;
        global_hdr.version_minor = 4;
        global_hdr.thiszone = 0;
        global_hdr.sigfigs = 0;
        global_hdr.snaplen = 65535;
        global_hdr.network = 105; // LINKTYPE_IEEE802_11
        Serial.write((const uint8_t*)&global_hdr, sizeof(global_hdr));
    }
}

bool snifferIsPcapSerialActive() {
    return s_pcapSerialActive;
}

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
    entry.ttl       = -1;
    entry.srcPort   = -1;
    entry.dstPort   = -1;

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
        
        // Auto extract TTL, srcPort, dstPort if it's a Data Frame
        uint8_t fc = rawPayload[0];
        uint8_t type_val = fc & 0x0C;
        if (type_val == 0x08) { // Data Frame
            bool encrypted = (rawPayload[1] & 0x40) != 0;
            if (!encrypted) {
                int header_len = ((fc & 0x80) != 0) ? 26 : 24; // QoS Data vs Data
                int llc_offset = header_len;
                if (rawLen >= llc_offset + 8) {
                    if (rawPayload[llc_offset] == 0xAA && rawPayload[llc_offset + 1] == 0xAA && rawPayload[llc_offset + 2] == 0x03) {
                        uint16_t ether_type = (rawPayload[llc_offset + 6] << 8) | rawPayload[llc_offset + 7];
                        int ip_offset = llc_offset + 8;
                        
                        if (ether_type == 0x0800 && rawLen >= ip_offset + 20) { // IPv4
                            entry.ttl = rawPayload[ip_offset + 8];
                            uint8_t protocol = rawPayload[ip_offset + 9];
                            uint8_t ip_hl = (rawPayload[ip_offset] & 0x0F) * 4;
                            int l4_offset = ip_offset + ip_hl;
                            
                            if ((protocol == 6 || protocol == 17) && rawLen >= l4_offset + 4) { // TCP or UDP
                                entry.srcPort = (rawPayload[l4_offset] << 8) | rawPayload[l4_offset + 1];
                                entry.dstPort = (rawPayload[l4_offset + 2] << 8) | rawPayload[l4_offset + 3];
                            }
                        } else if (ether_type == 0x86DD && rawLen >= ip_offset + 40) { // IPv6
                            entry.ttl = rawPayload[ip_offset + 7]; // Hop Limit
                            uint8_t next_hdr = rawPayload[ip_offset + 6];
                            int l4_offset = ip_offset + 40;
                            
                            if ((next_hdr == 6 || next_hdr == 17) && rawLen >= l4_offset + 4) { // TCP or UDP
                                entry.srcPort = (rawPayload[l4_offset] << 8) | rawPayload[l4_offset + 1];
                                entry.dstPort = (rawPayload[l4_offset + 2] << 8) | rawPayload[l4_offset + 3];
                            }
                        }
                    }
                }
            }
        }
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

    // Convert raw payload to Hex Dump with ASCII side-by-side
    if (rawPayload && rawLen > 0) {
        int dumpLen = (rawLen > 48) ? 48 : rawLen;
        String dumpStr = "";
        for (int i = 0; i < dumpLen; i += 16) {
            char rowBuf[100];
            int pos = 0;
            // 1. Offset
            pos += snprintf(&rowBuf[pos], sizeof(rowBuf) - pos, "%04X  ", i);
            
            // 2. Hex bytes
            int chunkLen = ((dumpLen - i) > 16) ? 16 : (dumpLen - i);
            for (int j = 0; j < 16; j++) {
                if (j < chunkLen) {
                    pos += snprintf(&rowBuf[pos], sizeof(rowBuf) - pos, "%02X ", rawPayload[i + j]);
                } else {
                    pos += snprintf(&rowBuf[pos], sizeof(rowBuf) - pos, "   ");
                }
                if (j == 7) {
                    pos += snprintf(&rowBuf[pos], sizeof(rowBuf) - pos, " ");
                }
            }
            
            pos += snprintf(&rowBuf[pos], sizeof(rowBuf) - pos, " | ");
            
            // 3. ASCII chars
            for (int j = 0; j < chunkLen; j++) {
                char c = rawPayload[i + j];
                if (c >= 32 && c <= 126) {
                    rowBuf[pos++] = c;
                } else {
                    rowBuf[pos++] = '.';
                }
            }
            rowBuf[pos] = '\0';
            if (dumpStr.length() > 0) dumpStr += "\n";
            dumpStr += rowBuf;
        }
        entry.rawHex = dumpStr;
    } else {
        entry.rawHex = "";
    }

    s_packetLogs.push_back(entry);
    if (s_packetLogs.size() > 20) {
        s_packetLogs.erase(s_packetLogs.begin());
    }

    // Broadcast via SSE
    webServerBroadcastLog(proto, subtype, src, dst, rssi, len, info, s_currentChannel, entry.srcMac, entry.dstMac, entry.rawHex, entry.ttl, entry.srcPort, entry.dstPort);

    if (s_jsonSerialActive && !s_pcapSerialActive) {
        JsonDocument doc;
        doc["type"] = "packet";
        JsonObject obj = doc["data"].to<JsonObject>();
        obj["proto"] = proto;
        obj["subtype"] = subtype;
        obj["src"] = src;
        obj["dst"] = dst;
        obj["rssi"] = rssi;
        obj["len"] = len;
        obj["info"] = info;
        obj["channel"] = s_currentChannel;
        obj["time"] = entry.timestamp;
        if (entry.srcMac.length()) obj["srcMac"] = entry.srcMac;
        if (entry.dstMac.length()) obj["dstMac"] = entry.dstMac;
        if (entry.ttl >= 0) obj["ttl"] = entry.ttl;
        if (entry.srcPort >= 0) obj["srcPort"] = entry.srcPort;
        if (entry.dstPort >= 0) obj["dstPort"] = entry.dstPort;
        serializeJson(doc, Serial);
        Serial.println();
    } else if (!s_concurrent && !s_pcapSerialActive) {
        Serial.printf("[SNIFFER] [%s %s] %s -> %s | %s\n", proto.c_str(), subtype.c_str(), src.c_str(), dst.c_str(), info.c_str());
    }
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
    uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    // Filter out ESP's own traffic and excluded devices
    if (len >= 16) {
        initializeEspMacs();

        // Filter out Owner MACs (ESP itself and connected clients)
        if (!s_ownerMacs.empty()) {
            char bufRa[18];
            char bufTa[18];
            char bufAddr3[18] = "";
            snprintf(bufRa, sizeof(bufRa), "%02X:%02X:%02X:%02X:%02X:%02X",
                     payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
            snprintf(bufTa, sizeof(bufTa), "%02X:%02X:%02X:%02X:%02X:%02X",
                     payload[10], payload[11], payload[12], payload[13], payload[14], payload[15]);
            if (len >= 22) {
                snprintf(bufAddr3, sizeof(bufAddr3), "%02X:%02X:%02X:%02X:%02X:%02X",
                         payload[16], payload[17], payload[18], payload[19], payload[20], payload[21]);
            }
            String raStr = String(bufRa);
            String taStr = String(bufTa);
            String addr3Str = String(bufAddr3);
            for (const auto& m : s_ownerMacs) {
                if (m == raStr || m == taStr || m == addr3Str) return;
            }
        }

        // Apply Whitelist / Blacklist
        if (isMacFiltered(payload + 4) || isMacFiltered(payload + 10) || (len >= 22 && isMacFiltered(payload + 16))) {
            return;
        }
    }

    if (s_pcapSerialActive) {
        uint32_t now_ms = millis();
        pcap_packet_header pkt_hdr;
        pkt_hdr.ts_sec = now_ms / 1000;
        pkt_hdr.ts_usec = (now_ms % 1000) * 1000;
        pkt_hdr.incl_len = len;
        pkt_hdr.orig_len = len;
        
        Serial.write((const uint8_t*)&pkt_hdr, sizeof(pkt_hdr));
        Serial.write(payload, len);
        // Do not process further to save CPU during raw streaming
        return;
    }

    s_packetCount++;

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
    
    // Ensure MACs are loaded before we potentially change WiFi mode
    initializeEspMacs();

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
    
    ledSetStatusColor(LED_COLOR_SNIFFING, true); // Blinking blue for sniffing
    Serial.println("[SNIFFER] Sniffer is active!");
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
        ledSetStatusColor(LED_COLOR_NORMAL, false);
        Serial.println("[SNIFFER] Concurrent Sniffer stopped.");
    } else {
        Serial.println("[SNIFFER] Offline Sniffer stopped. Restarting...");
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
    doc["llmnr"] = (uint32_t)s_llmnrCount;
    doc["nbns"] = (uint32_t)s_nbnsCount;
    doc["ssdp"] = (uint32_t)s_ssdpCount;
    doc["quic"] = (uint32_t)s_quicCount;
    doc["icmp"] = (uint32_t)s_icmpCount;
    doc["tcp"] = (uint32_t)s_tcpCount;
    doc["udp"] = (uint32_t)s_udpCount;
    doc["mqtt"] = (uint32_t)s_mqttCount;

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
        obj["ttl"] = log.ttl;
        obj["srcPort"] = log.srcPort;
        obj["dstPort"] = log.dstPort;
        obj["time"] = (double)log.timestamp / 1000.0;
    }
}

void snifferGetFilterConfig(JsonDocument& doc) {
    JsonArray wlArr = doc["whitelist"].to<JsonArray>();
    for (const auto& m : s_whitelist) wlArr.add(m);

    JsonArray blArr = doc["blacklist"].to<JsonArray>();
    for (const auto& m : s_blacklist) blArr.add(m);
}

void snifferSetFilterConfig(const String& whitelist, const String& blacklist) {
    Preferences prefs;
    prefs.begin("sniffer", false);
    prefs.putString("whitelist", whitelist);
    prefs.putString("blacklist", blacklist);
    prefs.end();
    loadFilterConfig();
}

void snifferAddOwnerMac(const String& mac) {
    std::lock_guard<std::mutex> lock(s_snifferMutex);
    String upperMac = mac;
    upperMac.toUpperCase();
    if (std::find(s_ownerMacs.begin(), s_ownerMacs.end(), upperMac) == s_ownerMacs.end()) {
        s_ownerMacs.push_back(upperMac);
        Serial.printf("[SNIFFER] Added Owner MAC to exclusion: %s\n", upperMac.c_str());
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
