/**
 * wifi_sniffer_data.cpp
 * 802.11 Data Frame handlers:
 *   ARP, EAPOL, IPv4 dispatcher, IPv6 dispatcher.
 */
#include "wifi_sniffer_common.h"

// ── Handlers ──────────────────────────────────────────────────────────────────

static void handleArp(const uint8_t* payload, int len, int llc_offset,
                      const char* src_mac, const char* dst_mac, int rssi) {
    s_arpCount++;
    int arp_offset = llc_offset + 8;
    if (len < arp_offset + 28) return;

    uint16_t op = (payload[arp_offset + 6] << 8) | payload[arp_offset + 7];
    
    char sender_ip[16], target_ip[16];
    snprintf(sender_ip, sizeof(sender_ip), "%d.%d.%d.%d",
             payload[arp_offset + 14], payload[arp_offset + 15],
             payload[arp_offset + 16], payload[arp_offset + 17]);
    snprintf(target_ip, sizeof(target_ip), "%d.%d.%d.%d",
             payload[arp_offset + 24], payload[arp_offset + 25],
             payload[arp_offset + 26], payload[arp_offset + 27]);

    char sender_mac[18];
    snprintf(sender_mac, sizeof(sender_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[arp_offset + 8],  payload[arp_offset + 9],  payload[arp_offset + 10],
             payload[arp_offset + 11], payload[arp_offset + 12], payload[arp_offset + 13]);

    String info_str;
    String proto_str;
    String sub_str;

    if (op == 1) { // Request
        proto_str = "ARP";
        sub_str   = "Req";
        info_str  = "Who has " + String(target_ip) + "? Tell " + String(sender_ip);
    } else if (op == 2) { // Reply
        proto_str = "ARP";
        sub_str   = "Rep";
        info_str  = String(sender_ip) + " is at " + String(sender_mac);
    } else {
        proto_str = "ARP";
        sub_str   = "Op:" + String(op);
        info_str  = "Opcode: " + String(op);
    }

    addPacketLog(proto_str, sub_str, src_mac, dst_mac, rssi, len, info_str, nullptr, nullptr, payload, len);
    if (!s_concurrent) {
        Serial.printf("[SNIFFER] [%s] %s -> %s | %s\n", proto_str.c_str(), src_mac, dst_mac, info_str.c_str());
    }
}

static void handleEapol(const uint8_t* payload, int len, int llc_offset,
                        const char* src_mac, const char* dst_mac, int rssi) {
    s_eapolCount++;
    int eapol_offset = llc_offset + 8;
    if (len < eapol_offset + 7) return;

    uint8_t packet_type = payload[eapol_offset + 1];
    String info_str = "EAPOL Packet Type: " + String(packet_type);
    String proto_str = "EAPOL";
    String sub_str = "Type " + String(packet_type);
    
    if (packet_type == 3) { // EAPOL-Key
        uint8_t key_desc = payload[eapol_offset + 2];
        uint16_t key_info = (payload[eapol_offset + 5] << 8) | payload[eapol_offset + 6];
        bool mic = (key_info & 0x0100) != 0;
        bool ack = (key_info & 0x0080) != 0;
        bool install = (key_info & 0x0040) != 0;
        bool secure = (key_info & 0x0200) != 0;
        
        proto_str = "WPA HS";
        const char* wpa_ver = (key_desc >= 2) ? "WPA2" : "WPA";
        if (ack && !mic) {
            sub_str  = "M1";
            info_str = String(wpa_ver) + " M1 AP->Cli ANonce";
        } else if (!ack && mic && !secure) {
            sub_str  = "M2";
            info_str = String(wpa_ver) + " M2 Cli->AP SNonce+MIC";
        } else if (ack && mic && install) {
            sub_str  = "M3";
            info_str = String(wpa_ver) + " M3 AP->Cli GTK Install";
        } else if (!ack && mic && secure) {
            sub_str  = "M4";
            info_str = String(wpa_ver) + " M4 Cli->AP Done";
        } else {
            sub_str  = "Key";
            info_str = "EAPOL-Key MIC=" + String(mic ? "Y" : "N") + " ACK=" + String(ack ? "Y" : "N");
        }
    } else if (packet_type == 1) {
        sub_str  = "Start";
        info_str = "EAPOL-Start";
    } else if (packet_type == 2) {
        sub_str  = "Logoff";
        info_str = "EAPOL-Logoff";
    }
    
    addPacketLog(proto_str, sub_str, src_mac, dst_mac, rssi, len, info_str, nullptr, nullptr, payload, len);
    Serial.printf("[SNIFFER] [%s %s] %s\n", proto_str.c_str(), sub_str.c_str(), info_str.c_str());
}

// ── Main Dispatcher ──────────────────────────────────────────────────────────

void dispatchDataFrame(uint8_t fc, const uint8_t* payload, int len, int rssi) {
    s_dataCount++;
    
    uint8_t subtype = fc & 0xF0;
    // Skip Null/QoS-Null data frames
    if (subtype == 0x40 || subtype == 0xC0 || subtype == 0x48 || subtype == 0xC8 || 
        subtype == 0x04 || subtype == 0x0C || subtype == 0x24 || subtype == 0x2C) {
        return;
    }

    bool encrypted = (payload[1] & 0x40) != 0;
    int header_len = 24;
    if ((fc & 0x80) != 0) { // QoS Data
        header_len = 26;
    }

    if (encrypted) {
        // Cannot parse encrypted WPA payloads
        return;
    }

    int llc_offset = header_len;
    if (len < llc_offset + 8) return;

    // Check SNAP encapsulation (LLC header: DSAP=AA, SSAP=AA, Ctrl=03)
    if (payload[llc_offset] == 0xAA && payload[llc_offset + 1] == 0xAA && payload[llc_offset + 2] == 0x03) {
        uint16_t ether_type = (payload[llc_offset + 6] << 8) | payload[llc_offset + 7];
        
        char src_mac[18], dst_mac[18];
        snprintf(dst_mac, sizeof(dst_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
        snprintf(src_mac, sizeof(src_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 payload[10], payload[11], payload[12], payload[13], payload[14], payload[15]);

        if (ether_type == 0x0806) {
            handleArp(payload, len, llc_offset, src_mac, dst_mac, rssi);
        } else if (ether_type == 0x888E) {
            handleEapol(payload, len, llc_offset, src_mac, dst_mac, rssi);
        } else if (ether_type == 0x0800) {
            dispatchIPv4Frame(payload, len, llc_offset + 8, llc_offset, src_mac, dst_mac, rssi);
        } else if (ether_type == 0x86DD) {
            dispatchIPv6Frame(payload, len, llc_offset + 8, src_mac, dst_mac, rssi);
        }
    }
}
