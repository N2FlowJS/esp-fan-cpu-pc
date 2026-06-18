/**
 * wifi_sniffer_data.cpp
 * 802.11 Data Frame handlers:
 *   ARP, EAPOL, IPv4 dispatcher, IPv6 dispatcher.
 */
#include "wifi_sniffer_common.h"

// ── Handlers ──────────────────────────────────────────────────────────────────

static void handleArp(const uint8_t* payload, int len, int llc_offset,
                      const char* src_mac, const char* dst_mac, int rssi) {
    s_arpCount = s_arpCount + 1;
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
        if (strcmp(sender_ip, target_ip) == 0) {
            sub_str   = "Grat";
            info_str  = "Gratuitous ARP: Announcement for " + String(sender_ip);
        } else {
            sub_str   = "Req";
            info_str  = "Who has " + String(target_ip) + "? Tell " + String(sender_ip);
        }
    } else if (op == 2) { // Reply
        proto_str = "ARP";
        if (strcmp(sender_ip, target_ip) == 0) {
            sub_str   = "Grat";
            info_str  = "Gratuitous ARP: Reply for " + String(sender_ip);
        } else {
            sub_str   = "Rep";
            info_str  = String(sender_ip) + " is at " + String(sender_mac);
        }
    } else {
        proto_str = "ARP";
        sub_str   = "Op:" + String(op);
        info_str  = "Opcode: " + String(op);
    }

    addPacketLog(proto_str, sub_str, src_mac, dst_mac, rssi, len, info_str, nullptr, nullptr, payload, len);
    snifferLog("[SNIFFER] [%s] %s -> %s | %s", proto_str.c_str(), src_mac, dst_mac, info_str.c_str());
}

static void handleEapol(const uint8_t* payload, int len, int llc_offset,
                        const char* src_mac, const char* dst_mac, int rssi) {
    s_eapolCount = s_eapolCount + 1;
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
        
        uint64_t replay = 0;
        if (len >= eapol_offset + 17) {
            for (int i = 0; i < 8; i++) {
                replay = (replay << 8) | payload[eapol_offset + 9 + i];
            }
        }
        String replay_str = "  Replay=" + String((unsigned long)replay);
        
        proto_str = "WPA HS";
        uint8_t key_version = key_info & 0x0007;
        const char* wpa_ver = "WPA2";
        if (key_version == 3) wpa_ver = "WPA3/PMF";
        else if (key_version == 1) wpa_ver = "WPA";
        
        if (ack && !mic) {
            sub_str  = "M1";
            info_str = String(wpa_ver) + " M1 AP->Cli ANonce" + replay_str;
        } else if (!ack && mic && !secure) {
            sub_str  = "M2";
            info_str = String(wpa_ver) + " M2 Cli->AP SNonce+MIC" + replay_str;
        } else if (ack && mic && install) {
            sub_str  = "M3";
            info_str = String(wpa_ver) + " M3 AP->Cli GTK Install" + replay_str;
        } else if (!ack && mic && secure) {
            sub_str  = "M4";
            info_str = String(wpa_ver) + " M4 Cli->AP Done" + replay_str;
        } else {
            sub_str  = "Key";
            info_str = "EAPOL-Key MIC=" + String(mic ? "Y" : "N") + " ACK=" + String(ack ? "Y" : "N") + replay_str;
        }
    } else if (packet_type == 1) {
        sub_str  = "Start";
        info_str = "EAPOL-Start";
    } else if (packet_type == 2) {
        sub_str  = "Logoff";
        info_str = "EAPOL-Logoff";
    }
    
    addPacketLog(proto_str, sub_str, src_mac, dst_mac, rssi, len, info_str, nullptr, nullptr, payload, len);
    snifferLog("[SNIFFER] [%s %s] %s", proto_str.c_str(), sub_str.c_str(), info_str.c_str());
}

// ── Main Dispatcher ──────────────────────────────────────────────────────────

void dispatchDataFrame(uint8_t fc, const uint8_t* payload, int len, int rssi) {
    s_dataCount = s_dataCount + 1;
    
    // Skip Null/QoS-Null data frames
    uint8_t subtype = (fc >> 4) & 0x0F;
    if (subtype == 4 || subtype == 12 || subtype == 5 || subtype == 13 || 
        subtype == 6 || subtype == 14 || subtype == 7 || subtype == 15) {
        return;
    }

    bool encrypted = (payload[1] & 0x40) != 0;
    
    // Calculate header length correctly
    // Base is 24 bytes (FC, Dur, Addr1, Addr2, Addr3, Seq)
    int header_len = 24;
    
    // Addr4 (6 bytes) if "To DS" and "From DS" both 1
    if ((payload[1] & 0x03) == 0x03) header_len += 6;
    
    // QoS Control (2 bytes) if subtype has bit 3 set (8, 9, 10, 11)
    if (subtype >= 8) header_len += 2;

    char src_mac[18], dst_mac[18];
    snprintf(dst_mac, sizeof(dst_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
    snprintf(src_mac, sizeof(src_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[10], payload[11], payload[12], payload[13], payload[14], payload[15]);

    if (encrypted) {
        // Log encrypted frames with visible MAC metadata and QoS hints
        String info = "Encrypted WPA Data";
        String subtype_str = "Data";
        String heuristic = "";
        
        // Basic length heuristics
        if (len < 100) {
            heuristic = " (Likely TCP ACK or Keep-Alive)";
        } else if (len > 1200) {
            heuristic = " (Likely Bulk Transfer / Streaming)";
        }

        if (subtype >= 8) {
            // QoS Control field is at header_len - 2
            uint8_t tid = payload[header_len - 2] & 0x0F;
            String ac = "Best Effort";
            if (tid == 1 || tid == 2) { 
                ac = "Background"; subtype_str = "BK"; 
                if (len > 1000) heuristic = " (File Download/Update)";
            }
            else if (tid == 0 || tid == 3) { 
                ac = "Best Effort"; subtype_str = "BE"; 
            }
            else if (tid == 4 || tid == 5) { 
                ac = "Video"; subtype_str = "VI"; 
                if (len > 1000) heuristic = " (Video Streaming)";
            }
            else if (tid == 6 || tid == 7) { 
                ac = "Voice"; subtype_str = "VO"; 
                if (len > 100 && len < 400) heuristic = " (VoIP/Gaming)";
            }
            
            info += " [QoS: " + ac + " (TID " + String(tid) + ")]";
        }
        
        info += heuristic;
        
        addPacketLog("DATA", subtype_str, src_mac, dst_mac, rssi, len, info, nullptr, nullptr, payload, len);
        return;
    }

    int llc_offset = header_len;
    if (len < llc_offset + 8) return;

    // Check SNAP encapsulation (LLC header: DSAP=AA, SSAP=AA, Ctrl=03)
    if (payload[llc_offset] == 0xAA && payload[llc_offset + 1] == 0xAA && payload[llc_offset + 2] == 0x03) {
        uint16_t ether_type = (payload[llc_offset + 6] << 8) | payload[llc_offset + 7];
        
        if (ether_type == 0x0806) {
            handleArp(payload, len, llc_offset, src_mac, dst_mac, rssi);
        } else if (ether_type == 0x888E) {
            handleEapol(payload, len, llc_offset, src_mac, dst_mac, rssi);
        } else if (ether_type == 0x0800) {
            // IPv4: IP offset is LLC (8 bytes) after header
            dispatchIPv4Frame(payload, len, llc_offset + 8, llc_offset + 8, src_mac, dst_mac, rssi);
        } else if (ether_type == 0x86DD) {
            // IPv6
            dispatchIPv6Frame(payload, len, llc_offset + 8, src_mac, dst_mac, rssi);
        } else {
            char eth_hex[5]; snprintf(eth_hex, 5, "%04X", ether_type);
            addPacketLog("DATA", String("0x")+eth_hex, src_mac, dst_mac, rssi, len, "SNAP Encapsulated Frame", nullptr, nullptr, payload, len);
        }
    } else {
        // Non-SNAP data frame
        addPacketLog("DATA", "LLC", src_mac, dst_mac, rssi, len, "Raw LLC Data Frame", nullptr, nullptr, payload, len);
    }
}