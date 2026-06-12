/**
 * wifi_sniffer_utils.cpp
 * Pure helper / decoder functions – no mutable global state.
 * All functions used by mgmt, data, ipv4, ipv6 parsers.
 */
#include "wifi_sniffer_common.h"

// ── DNS Helpers ───────────────────────────────────────────────────────────────

/// Decode a DNS label-encoded name (handles pointer compression).
String parseDnsName(const uint8_t* payload, int len, int offset) {
    String name;
    int idx   = offset;
    int depth = 0;
    while (idx < len && depth++ < 12) {
        uint8_t llen = payload[idx];
        if (llen == 0) break;
        if ((llen & 0xC0) == 0xC0) { name += "[ptr]"; break; }
        if (idx + 1 + llen > len)  break;
        if (name.length()) name += ".";
        for (int i = 0; i < llen; i++) name += (char)payload[idx + 1 + i];
        idx += 1 + llen;
    }
    return name;
}

/// Map DNS QTYPE number to readable string.
String parseDnsQtype(uint16_t qtype) {
    switch (qtype) {
        case 1:   return "A";
        case 2:   return "NS";
        case 5:   return "CNAME";
        case 6:   return "SOA";
        case 12:  return "PTR";
        case 15:  return "MX";
        case 16:  return "TXT";
        case 28:  return "AAAA";
        case 33:  return "SRV";
        case 255: return "ANY";
        default:  return "T" + String(qtype);
    }
}

/// Extract first A (IPv4) or AAAA (IPv6) answer from DNS answer section.
/// offset = byte offset of the first answer RR in payload.
String parseDnsAnswer(const uint8_t* payload, int len, int offset, uint16_t ancount) {
    int idx = offset;
    for (int i = 0; i < (int)ancount && idx < len - 10; i++) {
        if (idx >= len) break;
        // Skip name
        if ((payload[idx] & 0xC0) == 0xC0) {
            idx += 2;
        } else {
            while (idx < len && payload[idx] != 0) idx += payload[idx] + 1;
            idx++;
        }
        if (idx + 10 > len) break;
        uint16_t rtype  = (payload[idx] << 8)   | payload[idx+1];
        uint16_t rdlen  = (payload[idx+8] << 8) | payload[idx+9];
        int      rdstart = idx + 10;
        if (rdstart + rdlen > len) break;
        if (rtype == 1 && rdlen == 4) {
            char ip[16];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     payload[rdstart], payload[rdstart+1],
                     payload[rdstart+2], payload[rdstart+3]);
            return String(ip);
        }
        if (rtype == 28 && rdlen == 16) return "[IPv6]";
        idx = rdstart + rdlen;
    }
    return "";
}

// ── IPv6 ──────────────────────────────────────────────────────────────────────

/// Format a 16-byte IPv6 address to a compact string (best-effort).
String formatIPv6(const uint8_t* ip) {
    String compact;
    for (int i = 0; i < 8; i++) {
        uint16_t word = (ip[i*2] << 8) | ip[i*2+1];
        if (word == 0) {
            if (compact.length() == 0) compact = "::";
            else if (!compact.endsWith("::")) {
                if (compact.endsWith(":")) compact += ":";
                else compact += "::";
            }
        } else {
            if (compact.endsWith("::")) compact += String(word, HEX);
            else {
                if (compact.length() > 0 && !compact.endsWith(":")) compact += ":";
                compact += String(word, HEX);
            }
        }
    }
    while (compact.indexOf(":::") >= 0) compact.replace(":::", "::");
    return compact;
}

// ── Deauth ────────────────────────────────────────────────────────────────────

String getDeauthReasonStr(uint16_t reason) {
    switch (reason) {
        case 1:  return "Unspecified";
        case 2:  return "Prev Auth Invalid";
        case 3:  return "STA Leaving";
        case 4:  return "Disassoc: Inactivity";
        case 5:  return "AP Busy";
        case 6:  return "Class2 Frame";
        case 7:  return "Class3 Frame";
        case 8:  return "STA Left (Disassoc)";
        case 9:  return "STA Not Auth";
        case 15: return "4-Way HS Timeout";
        case 16: return "GroupKey HS Timeout";
        case 23: return "802.1X Auth Failed";
        case 34: return "TDLS Teardown";
        default: return "Code " + String(reason);
    }
}

// ── ICMP ──────────────────────────────────────────────────────────────────────

String getIcmpTypeStr(uint8_t t, uint8_t code) {
    switch (t) {
        case 0:  return "Echo Reply (Pong)";
        case 3:  {
            const char* codes[] = {
                "Net Unreach","Host Unreach","Proto Unreach",
                "Port Unreach","Frag needed","Src route failed"
            };
            return String("Dest Unreachable: ") + (code < 6 ? codes[code] : String(code));
        }
        case 5:  return "Redirect";
        case 8:  return "Echo Request (Ping)";
        case 11: return code == 0 ? "TTL Exceeded (Traceroute hop)" : "Frag Reassembly TTL";
        case 12: return "Parameter Problem";
        case 13: return "Timestamp Request";
        case 14: return "Timestamp Reply";
        default: return "Type " + String(t);
    }
}

String getIcmpv6TypeStr(uint8_t t) {
    switch (t) {
        case 1:   return "Dest Unreachable";
        case 2:   return "Packet Too Big";
        case 3:   return "Time Exceeded";
        case 128: return "Echo Request (Ping6)";
        case 129: return "Echo Reply (Pong6)";
        case 133: return "Router Solicitation";
        case 134: return "Router Advertisement";
        case 135: return "Neighbor Solicitation (ARP6 Req)";
        case 136: return "Neighbor Advertisement (ARP6 Rep)";
        case 143: return "MLD Report";
        default:  return "ICMPv6 Type " + String(t);
    }
}

// ── TCP / UDP Port Mapping ────────────────────────────────────────────────────

String getTcpServiceName(uint16_t port) {
    switch (port) {
        case 20:   return "FTP-Data";
        case 21:   return "FTP";
        case 22:   return "SSH";
        case 23:   return "Telnet";
        case 25:   return "SMTP";
        case 53:   return "DNS";
        case 67:   return "DHCP-Srv";
        case 68:   return "DHCP-Cli";
        case 80:   return "HTTP";
        case 110:  return "POP3";
        case 143:  return "IMAP";
        case 161:  return "SNMP";
        case 443:  return "HTTPS";
        case 502:  return "Modbus";
        case 554:  return "RTSP";
        case 1883: return "MQTT";
        case 3306: return "MySQL";
        case 3389: return "RDP";
        case 4840: return "OPC-UA";
        case 5353: return "mDNS";
        case 5432: return "Postgres";
        case 5683: return "CoAP";
        case 6379: return "Redis";
        case 8080: return "HTTP-Alt";
        case 8883: return "MQTTS";
        case 9000: return "SonarQube";
        default:   return "";
    }
}

/**
 * parseTlsSni
 * Safely traverses a TLS Client Hello packet to extract the SNI hostname.
 * Returns the hostname string or empty if not found/invalid.
 */
String parseTlsSni(const uint8_t* payload, int len) {
    if (!payload || len < 43) return ""; // Minimum TLS Handshake + Client Hello

    int pos = 0;

    // Check Content Type: Handshake (0x16)
    if (payload[pos++] != 0x16) return "";

    // Skip Version (2 bytes) and Length (2 bytes)
    pos += 4;

    // Handshake Type: Client Hello (0x01)
    if (pos >= len || payload[pos++] != 0x01) return "";

    // Skip Handshake Length (3 bytes), Version (2 bytes), Random (32 bytes)
    pos += 37;

    // Session ID Length (1 byte)
    if (pos >= len) return "";
    uint8_t sid_len = payload[pos++];
    pos += sid_len;

    // Cipher Suites Length (2 bytes)
    if (pos + 2 > len) return "";
    uint16_t cs_len = (payload[pos] << 8) | payload[pos + 1];
    pos += 2 + cs_len;

    // Compression Methods Length (1 byte)
    if (pos >= len) return "";
    uint8_t cm_len = payload[pos++];
    pos += cm_len;

    // Extensions Length (2 bytes)
    if (pos + 2 > len) return "";
    uint16_t ext_len = (payload[pos] << 8) | payload[pos + 1];
    pos += 2;
    int ext_end = pos + ext_len;
    if (ext_end > len) ext_end = len;

    // Iterate through extensions
    while (pos + 4 <= ext_end) {
        uint16_t etype = (payload[pos] << 8) | payload[pos + 1];
        uint16_t elen  = (payload[pos + 2] << 8) | payload[pos + 3];
        pos += 4;

        if (pos + elen > ext_end) break;

        // Server Name Indication (0x0000)
        if (etype == 0x0000) {
            int sni_pos = pos;
            if (sni_pos + 2 > pos + elen) break;
            uint16_t sn_list_len = (payload[sni_pos] << 8) | payload[sni_pos + 1];
            sni_pos += 2;

            if (sni_pos + sn_list_len > pos + elen) break;

            while (sni_pos + 3 <= pos + elen) {
                uint8_t name_type = payload[sni_pos++];
                uint16_t name_len = (payload[sni_pos] << 8) | payload[sni_pos + 1];
                sni_pos += 2;

                if (sni_pos + name_len > pos + elen) break;

                if (name_type == 0x00) { // Host Name
                    char* hostname = (char*)malloc(name_len + 1);
                    if (hostname) {
                        memcpy(hostname, &payload[sni_pos], name_len);
                        hostname[name_len] = '\0';
                        String result(hostname);
                        free(hostname);
                        return result;
                    }
                }
                sni_pos += name_len;
            }
        }
        pos += elen;
    }

    return "";
}

/**
 * handleUdpSsdp
 * Decodes SSDP (Simple Service Discovery Protocol) packets.
 */
void handleUdpSsdp(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi) {
    int app_len = len - pl_off;
    if (app_len < 10) return;
    const char* app = (const char*)&payload[pl_off];

    String sub = "NOTIFY";
    if (memcmp(app, "M-SEARCH", 8) == 0) sub = "SEARCH";

    String info = "SSDP " + sub;

    // Look for ST: or NT: or Location:
    String extra;
    for (int i = 0; i < app_len - 5; i++) {
        if (strncasecmp(&app[i], "ST: ", 4) == 0 || strncasecmp(&app[i], "NT: ", 4) == 0) {
            int start = i + 4;
            int end = start;
            while (end < app_len && app[end] != '\r' && app[end] != '\n') end++;
            int l = end - start; if (l > 64) l = 64;
            char buf[65]; memcpy(buf, &app[start], l); buf[l] = 0;
            extra = String(buf);
            break;
        }
    }

    if (extra.length()) info += " (" + extra + ")";
    addPacketLog("SSDP", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

/**
 * handleUdpNtp
 * Decodes NTP (Network Time Protocol) packets.
 */
void handleUdpNtp(const uint8_t* payload, int len, int pl_off,
                 const char* src_ip, const char* dst_ip, int rssi) {
    if (len < pl_off + 48) return;
    uint8_t li_vn_mode = payload[pl_off];
    uint8_t vn = (li_vn_mode >> 3) & 0x07;
    uint8_t mode = li_vn_mode & 0x07;
    uint8_t stratum = payload[pl_off + 1];

    String sub = (mode == 3) ? "Request" : (mode == 4) ? "Response" : "Mode " + String(mode);
    String info = "NTP v" + String(vn) + " " + sub + " Stratum=" + String(stratum);
    addPacketLog("NTP", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

// ── 802.11 Tag Parsers ────────────────────────────────────────────────────────

/// Extract SSID string from 802.11 tagged parameters.
String parseSSID(const uint8_t* payload, int len, int offset) {
    int idx = offset;
    while (idx < len - 2) {
        uint8_t tnum = payload[idx];
        uint8_t tlen = payload[idx+1];
        if (idx + 2 + tlen > len) break;
        if (tnum == 0) {
            if (tlen == 0) return "<Hidden SSID>";
            int clen = tlen > 32 ? 32 : tlen;
            char buf[33];
            memcpy(buf, &payload[idx+2], clen);
            buf[clen] = '\0';
            return String(buf);
        }
        idx += 2 + tlen;
    }
    return "";
}

/// Parse channel and security type from Beacon/Probe-Response tagged parameters.
BeaconInfo parseBeaconDetails(const uint8_t* payload, int len) {
    BeaconInfo info;
    info.channel  = s_currentChannel;
    info.security = "Open";
    if (len < 36) return info;

    uint16_t cap = payload[34] | (payload[35] << 8);
    if (cap & 0x0010) info.security = "WEP";

    int idx = 36;
    while (idx < len - 2) {
        uint8_t tnum = payload[idx];
        uint8_t tlen = payload[idx+1];
        if (idx + 2 + tlen > len) break;

        if (tnum == 3 && tlen == 1) {
            info.channel = payload[idx+2];
        } else if (tnum == 48) {               // RSN → WPA2/WPA3
            info.security = "WPA2";
            if (tlen >= 18) {
                uint16_t pw_cnt = payload[idx+8] | (payload[idx+9] << 8);
                int akm_off = idx + 10 + pw_cnt * 4;
                if (akm_off + 2 <= idx + 2 + tlen) {
                    uint16_t akm_cnt = payload[akm_off] | (payload[akm_off+1] << 8);
                    int akm_list = akm_off + 2;
                    for (int k = 0; k < akm_cnt; k++) {
                        if (akm_list + k*4 + 4 <= idx + 2 + tlen) {
                            const uint8_t* akm = &payload[akm_list + k*4];
                            if (akm[0]==0x00 && akm[1]==0x0F && akm[2]==0xAC && akm[3]==0x08) {
                                info.security = "WPA3";
                                break;
                            }
                        }
                    }
                }
            }
        } else if (tnum == 221 && tlen >= 6) {  // Vendor (WPA)
            const uint8_t* oui = &payload[idx+2];
            if (oui[0]==0x00 && oui[1]==0x50 && oui[2]==0xF2 && oui[3]==1) {
                if (info.security != "WPA2" && info.security != "WPA3")
                    info.security = "WPA";
            }
        }
        idx += 2 + tlen;
    }
    return info;
}

// ── Device Cache ──────────────────────────────────────────────────────────────

/// Update existing device or add new one.  Returns true if recently seen (< 15 s).
bool isRecentlySeen(const uint8_t* mac, bool isAP, int rssi,
                    const String& ssid, uint8_t channel, const String& security) {
    uint32_t now = millis();
    std::lock_guard<std::mutex> lock(s_snifferMutex);
    for (auto& dev : s_devices) {
        if (memcmp(dev.mac, mac, 6) == 0 && dev.isAP == isAP) {
            dev.rssi = rssi;
            uint32_t diff = now - dev.lastSeen;
            dev.lastSeen  = now;
            if (ssid.length()     && ssid != "<Hidden SSID>") dev.ssid = ssid;
            if (channel > 0)      dev.channel  = channel;
            if (security.length()) dev.security = security;
            return diff < 15000;
        }
    }
    // New device
    SnifferDevice d;
    memcpy(d.mac, mac, 6);
    d.ssid     = ssid;
    d.rssi     = rssi;
    d.lastSeen = now;
    d.isAP     = isAP;
    d.channel  = channel > 0 ? channel : s_currentChannel;
    d.security = security.length() ? security : (isAP ? "Open" : "");
    if (s_devices.size() >= 100) s_devices.erase(s_devices.begin());
    s_devices.push_back(d);
    return false;
}
