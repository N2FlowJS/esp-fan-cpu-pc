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
        if (rtype == 28 && rdlen == 16) {
            return formatIPv6(&payload[rdstart]);
        }
        if (rtype == 5) {
            return "CNAME: " + parseDnsName(payload, len, rdstart);
        }
        if (rtype == 16 && rdlen > 0) {
            String txt_str = "TXT: ";
            int t_idx = rdstart;
            int t_end = rdstart + rdlen;
            while (t_idx < t_end && t_idx < len) {
                uint8_t str_len = payload[t_idx++];
                if (t_idx + str_len > t_end || t_idx + str_len > len) break;
                
                if (txt_str.length() > 5) txt_str += ", ";
                for (int j = 0; j < str_len; j++) {
                    char c = payload[t_idx + j];
                    if (c >= 32 && c <= 126) txt_str += c;
                    else txt_str += ".";
                }
                t_idx += str_len;
            }
            return txt_str;
        }
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

    String sni = "";
    String alpn = "";

    // Iterate through extensions
    while (pos + 4 <= ext_end) {
        uint16_t etype = (payload[pos] << 8) | payload[pos + 1];
        uint16_t elen  = (payload[pos + 2] << 8) | payload[pos + 3];
        pos += 4;

        if (pos + elen > ext_end) break;

        // Server Name Indication (0x0000)
        if (etype == 0x0000 && elen >= 5) {
            int sni_pos = pos;
            uint16_t sn_list_len = (payload[sni_pos] << 8) | payload[sni_pos + 1];
            sni_pos += 2;

            if (sni_pos + sn_list_len <= pos + elen) {
                while (sni_pos + 3 <= pos + elen) {
                    uint8_t name_type = payload[sni_pos++];
                    uint16_t name_len = (payload[sni_pos] << 8) | payload[sni_pos + 1];
                    sni_pos += 2;

                    if (sni_pos + name_len <= pos + elen && name_type == 0x00) { // Host Name
                        char* hostname = (char*)malloc(name_len + 1);
                        if (hostname) {
                            memcpy(hostname, &payload[sni_pos], name_len);
                            hostname[name_len] = '\0';
                            sni = String(hostname);
                            free(hostname);
                        }
                    }
                    sni_pos += name_len;
                }
            }
        }
        // ALPN (Application-Layer Protocol Negotiation) (0x0010)
        else if (etype == 0x0010 && elen >= 3) {
            int alpn_pos = pos;
            uint16_t list_len = (payload[alpn_pos] << 8) | payload[alpn_pos + 1];
            alpn_pos += 2;
            
            if (alpn_pos + list_len <= pos + elen && list_len > 0) {
                uint8_t str_len = payload[alpn_pos++];
                if (alpn_pos + str_len <= pos + elen) {
                    char* alpn_str = (char*)malloc(str_len + 1);
                    if (alpn_str) {
                        memcpy(alpn_str, &payload[alpn_pos], str_len);
                        alpn_str[str_len] = '\0';
                        alpn = String(alpn_str);
                        free(alpn_str);
                    }
                }
            }
        }
        pos += elen;
    }

    if (sni.length() > 0) {
        if (alpn.length() > 0) {
            return sni + " [ALPN:" + alpn + "]";
        }
        return sni;
    }

    return "";
}

/**
 * handleUdpSsdp
 * Decodes SSDP (Simple Service Discovery Protocol) packets.
 */
void handleUdpSsdp(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi) {
    s_ssdpCount++;
    int app_len = len - pl_off;
    if (app_len < 10) return;
    const char* app = (const char*)&payload[pl_off];

    String sub = "NOTIFY";
    if (memcmp(app, "M-SEARCH", 8) == 0) sub = "SEARCH";

    String info = "SSDP " + sub;

    // Look for ST: or NT: or Location: or USN: or Server: or User-Agent:
    String extra;
    String location;
    String usn;
    String server;
    for (int i = 0; i < app_len - 10; i++) {
        if (extra.length() == 0 && (strncasecmp(&app[i], "ST: ", 4) == 0 || strncasecmp(&app[i], "NT: ", 4) == 0)) {
            int start = i + 4;
            int end = start;
            while (end < app_len && app[end] != '\r' && app[end] != '\n') end++;
            int l = end - start; if (l > 64) l = 64;
            char buf[65]; memcpy(buf, &app[start], l); buf[l] = 0;
            extra = String(buf);
        }
        if (location.length() == 0 && strncasecmp(&app[i], "LOCATION: ", 10) == 0) {
            int start = i + 10;
            int end = start;
            while (end < app_len && app[end] != '\r' && app[end] != '\n') end++;
            int l = end - start; if (l > 128) l = 128;
            char buf[129]; memcpy(buf, &app[start], l); buf[l] = 0;
            location = String(buf);
        }
        if (usn.length() == 0 && strncasecmp(&app[i], "USN: ", 5) == 0) {
            int start = i + 5;
            int end = start;
            while (end < app_len && app[end] != '\r' && app[end] != '\n') end++;
            int l = end - start; if (l > 48) l = 48;
            char buf[49]; memcpy(buf, &app[start], l); buf[l] = 0;
            usn = String(buf);
        }
        if (server.length() == 0 && strncasecmp(&app[i], "SERVER: ", 8) == 0) {
            int start = i + 8;
            int end = start;
            while (end < app_len && app[end] != '\r' && app[end] != '\n') end++;
            int l = end - start; if (l > 48) l = 48;
            char buf[49]; memcpy(buf, &app[start], l); buf[l] = 0;
            server = String(buf);
        }
        if (server.length() == 0 && strncasecmp(&app[i], "USER-AGENT: ", 12) == 0) {
            int start = i + 12;
            int end = start;
            while (end < app_len && app[end] != '\r' && app[end] != '\n') end++;
            int l = end - start; if (l > 48) l = 48;
            char buf[49]; memcpy(buf, &app[start], l); buf[l] = 0;
            server = String(buf); // Map user-agent to server field for brevity
        }
    }

    if (server.length()) info += " [Dev: " + server + "]";
    if (extra.length()) info += " (" + extra + ")";
    if (location.length()) info += " [Loc: " + location + "]";
    if (usn.length()) info += " [USN: " + usn + "]";
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

/**
 * handleUdpLlmnr
 * Link-Local Multicast Name Resolution (Port 5355).
 * Similar to DNS but used by Windows.
 */
void handleUdpLlmnr(const uint8_t* payload, int len, int pl_off,
                   const char* src_ip, const char* dst_ip, int rssi) {
    s_llmnrCount++;
    if (len < pl_off + 12) return;
    bool qr = (payload[pl_off + 2] & 0x80) != 0;
    uint16_t qdcount = (payload[pl_off + 4] << 8) | payload[pl_off + 5];
    String name = parseDnsName(payload, len, pl_off + 12);
    
    String sub = qr ? "Reply" : "Query";
    String info = "LLMNR " + sub + ": " + name;
    addPacketLog("LLMNR", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

/**
 * handleUdpNbns
 * NetBIOS Name Service (Port 137).
 */
void handleUdpNbns(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi) {
    s_nbnsCount++;
    if (len < pl_off + 12) return;
    bool qr = (payload[pl_off + 2] & 0x80) != 0;
    uint16_t qdcount = (payload[pl_off + 4] << 8) | payload[pl_off + 5];
    
    String name = "";
    int name_off = pl_off + 12;
    if (len > name_off + 32 && payload[name_off] == 32) {
        // NetBIOS encoded name (32 bytes)
        char decoded[17];
        for (int i = 0; i < 16; i++) {
            uint8_t c1 = payload[name_off + 1 + i*2] - 'A';
            uint8_t c2 = payload[name_off + 2 + i*2] - 'A';
            decoded[i] = (c1 << 4) | c2;
        }
        decoded[16] = '\0';
        name = String(decoded);
        name.trim();
    }

    String sub = qr ? "Reply" : "Query";
    String info = "NBNS " + sub + (name.length() ? ": " + name : "");
    addPacketLog("NBNS", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

/**
 * handleUdpWsd
 * Decodes WS-Discovery (Web Services Dynamic Discovery) packets (Port 3702).
 */
void handleUdpWsd(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi) {
    int app_len = len - pl_off;
    if (app_len < 10) return;

    int copy_len = (app_len > 512) ? 512 : app_len;
    char* xml = (char*)malloc(copy_len + 1);
    if (!xml) return;
    memcpy(xml, &payload[pl_off], copy_len);
    xml[copy_len] = '\0';

    String sub = "Msg";
    if (strstr(xml, "/ProbeMatches") || strstr(xml, "ProbeMatches")) {
        sub = "ProbeMatches";
    } else if (strstr(xml, "/Probe") || strstr(xml, "Probe")) {
        sub = "Probe";
    } else if (strstr(xml, "/ResolveMatches") || strstr(xml, "ResolveMatches")) {
        sub = "ResolveMatches";
    } else if (strstr(xml, "/Resolve") || strstr(xml, "Resolve")) {
        sub = "Resolve";
    } else if (strstr(xml, "/Hello") || strstr(xml, "Hello")) {
        sub = "Hello";
    } else if (strstr(xml, "/Bye") || strstr(xml, "Bye")) {
        sub = "Bye";
    }

    String info = "WSD " + sub;

    char* types_ptr = strstr(xml, "<Types>");
    if (!types_ptr) types_ptr = strstr(xml, ":Types>");
    if (types_ptr) {
        char* start = strchr(types_ptr, '>');
        if (start) {
            start++;
            char* end = strchr(start, '<');
            if (end && (end - start < 64)) {
                char type_buf[65];
                int l = end - start;
                memcpy(type_buf, start, l);
                type_buf[l] = '\0';
                info += " Types: " + String(type_buf);
            }
        }
    }

    char* xaddrs_ptr = strstr(xml, "<XAddrs>");
    if (!xaddrs_ptr) xaddrs_ptr = strstr(xml, ":XAddrs>");
    if (xaddrs_ptr) {
        char* start = strchr(xaddrs_ptr, '>');
        if (start) {
            start++;
            char* end = strchr(start, '<');
            if (end && (end - start < 128)) {
                char addr_buf[129];
                int l = end - start;
                memcpy(addr_buf, start, l);
                addr_buf[l] = '\0';
                info += " [Addr: " + String(addr_buf) + "]";
            }
        }
    }

    free(xml);
    addPacketLog("WSD", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

/**
 * handleUdpCoap
 * Decodes CoAP (Constrained Application Protocol) packets (Port 5683).
 */
void handleUdpCoap(const uint8_t* payload, int len, int pl_off,
                   const char* src_ip, const char* dst_ip, int rssi) {
    int app_len = len - pl_off;
    if (app_len < 4) return;

    uint8_t first = payload[pl_off];
    uint8_t ver = (first >> 6) & 0x03;
    if (ver != 1) return;

    uint8_t type = (first >> 4) & 0x03;
    uint8_t tkl = first & 0x0F;
    uint8_t code = payload[pl_off + 1];
    uint16_t msg_id = (payload[pl_off + 2] << 8) | payload[pl_off + 3];

    const char* type_str = (type == 0) ? "CON" : (type == 1) ? "NON" : (type == 2) ? "ACK" : "RST";

    String code_str;
    if (code == 0) {
        code_str = "Empty";
    } else if (code >= 1 && code <= 31) {
        const char* reqs[] = {"GET", "POST", "PUT", "DELETE"};
        if (code >= 1 && code <= 4) {
            code_str = reqs[code - 1];
        } else {
            code_str = "REQ(" + String(code) + ")";
        }
    } else {
        uint8_t cls = code >> 5;
        uint8_t det = code & 0x1F;
        code_str = String(cls) + "." + (det < 10 ? "0" : "") + String(det);
    }

    String info = "CoAP " + String(type_str) + " " + code_str + " id=" + String(msg_id);

    int opt_off = pl_off + 4 + tkl;
    if (opt_off < len) {
        int curr_opt_num = 0;
        int idx = opt_off;
        String path = "";

        while (idx < len) {
            uint8_t opt_byte = payload[idx];
            if (opt_byte == 0xFF) break;

            uint8_t opt_delta = opt_byte >> 4;
            uint8_t opt_len = opt_byte & 0x0F;
            idx++;

            int actual_delta = opt_delta;
            if (opt_delta == 13) {
                if (idx < len) actual_delta = payload[idx++] + 13;
            } else if (opt_delta == 14) {
                if (idx + 1 < len) {
                    actual_delta = ((payload[idx] << 8) | payload[idx+1]) + 269;
                    idx += 2;
                }
            }

            int actual_len = opt_len;
            if (opt_len == 13) {
                if (idx < len) actual_len = payload[idx++] + 13;
            } else if (opt_len == 14) {
                if (idx + 1 < len) {
                    actual_len = ((payload[idx] << 8) | payload[idx+1]) + 269;
                    idx += 2;
                }
            }

            curr_opt_num += actual_delta;

            if (idx + actual_len > len) break;

            if (curr_opt_num == 11) {
                char* p_str = (char*)malloc(actual_len + 1);
                if (p_str) {
                    memcpy(p_str, &payload[idx], actual_len);
                    p_str[actual_len] = '\0';
                    if (path.length() > 0) path += "/";
                    path += String(p_str);
                    free(p_str);
                }
            }

            idx += actual_len;
        }

        if (path.length() > 0) {
            info += " /" + path;
        }
    }

    addPacketLog("CoAP", type_str, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
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
    info.wifiGen  = "WiFi 4/Legacy";
    info.clients  = -1;
    info.utilization = -1;
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
        } else if (tnum == 11 && tlen >= 5) {  // QBSS Load
            info.clients = payload[idx+2] | (payload[idx+3] << 8);
            info.utilization = (payload[idx+4] * 100) / 255;
        } else if (tnum == 45) {               // HT
            if (info.wifiGen == "WiFi 4/Legacy") info.wifiGen = "WiFi 4 (N)";
        } else if (tnum == 191) {              // VHT
            if (info.wifiGen != "WiFi 6 (AX)" && info.wifiGen != "WiFi 7 (BE)") info.wifiGen = "WiFi 5 (AC)";
        } else if (tnum == 255 && tlen > 0) {  // Ext Element
            if (payload[idx+2] == 35) info.wifiGen = "WiFi 6 (AX)";
            else if (payload[idx+2] == 108) info.wifiGen = "WiFi 7 (BE)";
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
                    const String& ssid, uint8_t channel, const String& security,
                    const String& wifiGen, int clients, int util) {
    uint32_t now = millis();
    std::lock_guard<std::mutex> lock(s_snifferMutex);
    for (auto& dev : s_devices) {
        if (memcmp(dev.mac, mac, 6) == 0 && dev.isAP == isAP) {
            dev.rssi = rssi;
            uint32_t diff = now - dev.lastSeen;
            dev.lastSeen  = now;
            dev.packetCount++;
            if (ssid.length()     && ssid != "<Hidden SSID>") dev.ssid = ssid;
            if (channel > 0)      dev.channel  = channel;
            if (security.length()) dev.security = security;
            if (wifiGen.length())  dev.wifiGen = wifiGen;
            if (clients >= 0)      dev.clients = clients;
            if (util >= 0)         dev.utilization = util;
            return diff < 15000;
        }
    }
    // New device
    SnifferDevice d;
    memcpy(d.mac, mac, 6);
    d.ssid     = ssid;
    d.rssi     = rssi;
    d.lastSeen = now;
    d.packetCount = 1;
    d.isAP     = isAP;
    d.channel  = channel > 0 ? channel : s_currentChannel;
    d.security = security.length() ? security : (isAP ? "Open" : "");
    d.wifiGen  = wifiGen.length() ? wifiGen : "";
    d.clients  = clients;
    d.utilization = util;
    d.vendor   = getMacVendor(mac);
    if (s_devices.size() >= 100) s_devices.erase(s_devices.begin());
    s_devices.push_back(d);
    return false;
}

/// Lookup common MAC Vendors (basic mapping)
String getMacVendor(const uint8_t* mac) {
    if (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF) return "Broadcast";
    if (mac[0] & 0x01) return "Multicast";
    if (mac[0] & 0x02) return "Local Admin/Random";

    uint32_t oui = (mac[0] << 16) | (mac[1] << 8) | mac[2];
    switch (oui) {
        case 0x001A11: case 0x286F7F: return "Google";
        case 0x0024E4: case 0x04F13E: case 0x0C8BFD: case 0x1040F3: case 0x10A5D0: case 0x147DD3: case 0x18F643: case 0x1C53F9: case 0x1CE85D: case 0x2038AE: case 0x203C2E: case 0x240A64: case 0x242446: case 0x2462AB: case 0x246F28: case 0x24AB81: case 0x24B2DE: case 0x2C3AE8: case 0x2CE412: case 0x30AEA4: case 0x30C6F7: case 0x34865D: case 0x348A7B: case 0x349454: case 0x34AB95: case 0x34B472: case 0x3C6105: case 0x3C71BF: case 0x4022D8: case 0x409151: case 0x441793: case 0x4827E2: case 0x483FDA: case 0x485519: case 0x48E729: case 0x4C11AE: case 0x4C7525: case 0x500291: case 0x543204: case 0x5443B2: case 0x58BF25: case 0x5C8084: case 0x5CCF7F: case 0x600194: case 0x6055F9: case 0x64B708: case 0x68B253: case 0x68C63A: case 0x6C4008: case 0x70039F: case 0x70041D: case 0x70B8F6: case 0x7440BB: case 0x782184: case 0x7C87CE: case 0x7C9EBD: case 0x807D3A: case 0x840D8E: case 0x84CCA8: case 0x84F3EB: case 0x8C4B14: case 0x8CC681: case 0x8CEAA9: case 0x90380C: case 0x9097D5: case 0x943CC6: case 0x94B97E: case 0x98CDAC: case 0x9C9C1F: case 0xA020A6: case 0xA0B765: case 0xA0DD6C: case 0xA47B9D: case 0xA4CF12: case 0xA4E57C: case 0xA8032A: case 0xAC0BFB: case 0xAC67B2: case 0xACC1EE: case 0xACD074: case 0xB0A732: case 0xB0B98A: case 0xB4E62D: case 0xB883BA: case 0xB8C385: case 0xB8D61A: case 0xB8F009: case 0xBCDD22: case 0xC049EF: case 0xC44F33: case 0xC4DD57: case 0xC82B96: case 0xCC50E3: case 0xCCF411: case 0xD4D4DA: case 0xD8A01D: case 0xD8BFC0: case 0xD8F15B: case 0xDC4F22: case 0xE05A1B: case 0xE09861: case 0xE831CD: case 0xE868E7: case 0xE89F6D: case 0xE8B41D: case 0xE8DB84: case 0xEC64C9: case 0xEC94CB: case 0xECDA3B: case 0xECFAB1: case 0xF008D1: case 0xF412FA: case 0xF4CFE2: case 0xFCF528: return "Espressif";
        case 0x000000: return "Xerox";
        case 0x000A27: case 0x000C29: case 0x005056: return "Apple"; // Just a few examples
        case 0x001BDC: case 0x001405: return "Samsung";
        // Just generic placeholder to show functionality
    }

    if ((mac[0] == 0x00 && mac[1] == 0x1B && mac[2] == 0xDC) || (mac[0] == 0xCC && mac[1] == 0xB8 && mac[2] == 0xA8)) return "Samsung";
    if ((mac[0] == 0xF0 && mac[1] == 0x18 && mac[2] == 0x98) || (mac[0] == 0x6C && mac[1] == 0x40 && mac[2] == 0x08)) return "Apple";

    return "";
}

/**
 * parseTlsServerHello
 * Parses a TLS Server Hello handshake packet to extract the TLS version and cipher suite.
 */
String parseTlsServerHello(const uint8_t* payload, int len) {
    if (!payload || len < 43) return ""; // Minimum TLS Handshake + Server Hello

    int pos = 0;

    // Check Content Type: Handshake (0x16)
    if (payload[pos++] != 0x16) return "";

    // Skip Version (2 bytes) and Length (2 bytes)
    pos += 4;

    // Handshake Type: Server Hello (0x02)
    if (pos >= len || payload[pos++] != 0x02) return "";

    // Skip Handshake Length (3 bytes)
    pos += 3;

    // Server Version (2 bytes)
    if (pos + 2 > len) return "";
    uint16_t ver = (payload[pos] << 8) | payload[pos + 1];
    pos += 2;

    // Skip Random (32 bytes)
    pos += 32;

    // Session ID Length (1 byte)
    if (pos >= len) return "";
    uint8_t sid_len = payload[pos++];
    pos += sid_len;

    // Cipher Suite (2 bytes)
    if (pos + 2 > len) return "";
    uint16_t cipher = (payload[pos] << 8) | payload[pos + 1];
    pos += 2;

    // Map TLS Version
    String ver_str = "TLSv1.2";
    if (ver == 0x0304) ver_str = "TLSv1.3";
    else if (ver == 0x0303) ver_str = "TLSv1.2";
    else if (ver == 0x0302) ver_str = "TLSv1.1";
    else if (ver == 0x0301) ver_str = "TLSv1.0";
    else ver_str = "TLS 0x" + String(ver, HEX);

    // Map some common cipher suites
    String cipher_str;
    switch (cipher) {
        // TLS 1.3 Cipher Suites
        case 0x1301: cipher_str = "AES_128_GCM_SHA256"; break;
        case 0x1302: cipher_str = "AES_256_GCM_SHA384"; break;
        case 0x1303: cipher_str = "CHACHA20_POLY1305_SHA256"; break;
        // TLS 1.2 Cipher Suites
        case 0xC02B: cipher_str = "ECDHE_ECDSA_AES_128_GCM_SHA256"; break;
        case 0xC02F: cipher_str = "ECDHE_RSA_AES_128_GCM_SHA256"; break;
        case 0xC02C: cipher_str = "ECDHE_ECDSA_AES_256_GCM_SHA384"; break;
        case 0xC030: cipher_str = "ECDHE_RSA_AES_256_GCM_SHA384"; break;
        case 0xCCA8: cipher_str = "ECDHE_RSA_CHACHA20_POLY1305_SHA256"; break;
        case 0xCCA9: cipher_str = "ECDHE_ECDSA_CHACHA20_POLY1305_SHA256"; break;
        default: cipher_str = "0x" + String(cipher, HEX); break;
    }

    return ver_str + " (" + cipher_str + ")";
}
