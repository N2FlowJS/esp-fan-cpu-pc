/**
 * wifi_sniffer_ipv6.cpp
 * IPv6 layer protocol handlers:
 *   ICMPv6, TCPv6, UDPv6 (DNSv6, mDNSv6).
 */
#include "wifi_sniffer_common.h"

// ── ICMPv6 ───────────────────────────────────────────────────────────────────

static void handleIcmpv6(const uint8_t* payload, int len,
                         int ip6_offset, const char* src_ip6, const char* dst_ip6, int rssi) {
    s_icmpCount++;
    int icmp6_offset = ip6_offset + 40;
    if (len < icmp6_offset + 4) return;

    uint8_t icmp6_type = payload[icmp6_offset];
    String info_str = getIcmpv6TypeStr(icmp6_type);
    
    String sub = "Info";
    if (icmp6_type == 128) sub = "Ping";
    else if (icmp6_type == 129) sub = "Pong";
    else if (icmp6_type == 135) {
        sub = "NS";
        if (len >= icmp6_offset + 24) {
            const uint8_t* target_ip6 = &payload[icmp6_offset + 8];
            info_str = "Neighbor Solicitation: Who has " + formatIPv6(target_ip6) + "?";
        }
    }
    else if (icmp6_type == 136) {
        sub = "NA";
        if (len >= icmp6_offset + 24) {
            const uint8_t* target_ip6 = &payload[icmp6_offset + 8];
            info_str = "Neighbor Advertisement: " + formatIPv6(target_ip6) + " is active";
        }
    }

    if ((icmp6_type == 128 || icmp6_type == 129) && len >= icmp6_offset + 8) {
        uint16_t id  = (payload[icmp6_offset+4] << 8) | payload[icmp6_offset+5];
        uint16_t seq = (payload[icmp6_offset+6] << 8) | payload[icmp6_offset+7];
        info_str = (icmp6_type == 128 ? "Echo Request" : "Echo Reply") + String("  id=") + String(id) + " seq=" + String(seq);
        
        int data_len = len - (icmp6_offset + 8);
        if (data_len > 0) {
            String data_peek = "";
            int peek_limit = data_len > 16 ? 16 : data_len;
            for (int i = 0; i < peek_limit; i++) {
                char c = payload[icmp6_offset + 8 + i];
                if (c >= 32 && c <= 126) data_peek += c;
                else data_peek += ".";
            }
            info_str += "  data=\"" + data_peek + "\"";
        }
    }

    addPacketLog("ICMP", sub, src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
}

// ── TCPv6 ────────────────────────────────────────────────────────────────────

static void handleTcpv6(const uint8_t* payload, int len,
                        int ip6_offset, const char* src_ip6, const char* dst_ip6, int rssi) {
    s_tcpCount++;
    int tcp_offset = ip6_offset + 40;
    if (len < tcp_offset + 20) return;

    uint16_t src_port = (payload[tcp_offset] << 8) | payload[tcp_offset + 1];
    uint16_t dst_port = (payload[tcp_offset + 2] << 8) | payload[tcp_offset + 3];
    uint8_t tcp_flags = payload[tcp_offset + 13];
    
    String flags_str = "";
    if (tcp_flags & 0x02) flags_str += "SYN ";
    if (tcp_flags & 0x10) flags_str += "ACK ";
    if (tcp_flags & 0x01) flags_str += "FIN ";
    if (tcp_flags & 0x04) flags_str += "RST ";
    if (tcp_flags & 0x08) flags_str += "PSH ";
    flags_str.trim();

    String info_str = "TCP6: Port " + String(src_port) + " -> " + String(dst_port) + " [" + flags_str + "]";
    
    uint8_t tcp_hl = ((payload[tcp_offset + 12] >> 4) & 0x0F) * 4;
    int app_offset = tcp_offset + tcp_hl;
    int app_len = len - app_offset;

    // Peek HTTPv6
    if (src_port == 80 || dst_port == 80) {
        if (app_len > 4) {
            const uint8_t* http_data = &payload[app_offset];
            if (memcmp(http_data, "GET ", 4) == 0 || 
                memcmp(http_data, "POST", 4) == 0 ||
                memcmp(http_data, "PUT ", 4) == 0 || 
                memcmp(http_data, "DELE", 4) == 0 ||
                memcmp(http_data, "HEAD", 4) == 0 || 
                memcmp(http_data, "PATC", 4) == 0 ||
                memcmp(http_data, "OPTI", 4) == 0 || 
                memcmp(http_data, "CONN", 4) == 0) {
                
                int line_len = 0;
                while (line_len < app_len && line_len < 64 && 
                       http_data[line_len] != '\r' && http_data[line_len] != '\n') {
                    line_len++;
                }
                char line_buf[65];
                memcpy(line_buf, http_data, line_len);
                line_buf[line_len] = '\0';
                
                // Find Host header
                String host;
                for (int hi = 0; hi < app_len - 6; hi++) {
                    if (memcmp(&http_data[hi], "Host: ", 6) == 0) {
                        int he = hi+6;
                        while (he < app_len && http_data[he]!='\r' && http_data[he]!='\n') he++;
                        int hl = he-(hi+6); if (hl>48) hl=48;
                        char hb[49]; memcpy(hb, &http_data[hi+6], hl); hb[hl]=0;
                        host = String(hb); break;
                    }
                }
                
                // Find User-Agent header
                String ua;
                for (int hi = 0; hi < app_len - 12; hi++) {
                    if (strncasecmp((const char*)&http_data[hi], "User-Agent: ", 12) == 0) {
                        int he = hi+12;
                        while (he < app_len && http_data[he]!='\r' && http_data[he]!='\n') he++;
                        int hl = he-(hi+12); if (hl>40) hl=40;
                        char uab[41]; memcpy(uab, &http_data[hi+12], hl); uab[hl]=0;
                        ua = String(uab); break;
                    }
                }
                
                // Find Referer header
                String referer;
                for (int hi = 0; hi < app_len - 9; hi++) {
                    if (strncasecmp((const char*)&http_data[hi], "Referer: ", 9) == 0) {
                        int he = hi+9;
                        while (he < app_len && http_data[he]!='\r' && http_data[he]!='\n') he++;
                        int hl = he-(hi+9); if (hl>40) hl=40;
                        char refb[41]; memcpy(refb, &http_data[hi+9], hl); refb[hl]=0;
                        referer = String(refb); break;
                    }
                }
                
                // Parse Method and URL
                String method_str = "";
                String url_str = "";
                int sp1 = -1, sp2 = -1;
                for (int i = 0; i < line_len; i++) {
                    if (line_buf[i] == ' ') {
                        if (sp1 == -1) sp1 = i;
                        else if (sp2 == -1) sp2 = i;
                    }
                }
                if (sp1 != -1 && sp2 != -1) {
                    line_buf[sp1] = '\0';
                    line_buf[sp2] = '\0';
                    method_str = String(line_buf);
                    url_str = String(&line_buf[sp1+1]);
                } else {
                    method_str = "REQ";
                    url_str = String(line_buf);
                }

                info_str = method_str + " " + url_str + " [Port:" + String(dst_port) + "]";
                if (host.length()) info_str += "  [Host:" + host + "]";
                if (ua.length()) info_str += "  [UA:" + ua + "...]";
                if (referer.length()) info_str += "  [Ref:" + referer + "]";
                addPacketLog("HTTP", method_str, src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
                return;
            }
            
            if (memcmp(http_data, "HTTP", 4) == 0) {
                int line_len = 0;
                while (line_len < app_len && line_len < 32 && 
                       http_data[line_len] != '\r' && http_data[line_len] != '\n') {
                    line_len++;
                }
                char line_buf[33];
                memcpy(line_buf, http_data, line_len);
                line_buf[line_len] = '\0';
                
                // Find Server header
                String server;
                for (int hi = 0; hi < app_len - 8; hi++) {
                    if (strncasecmp((const char*)&http_data[hi], "Server: ", 8) == 0) {
                        int he = hi+8;
                        while (he < app_len && http_data[he]!='\r' && http_data[he]!='\n') he++;
                        int hl = he-(hi+8); if (hl>30) hl=30;
                        char srvb[31]; memcpy(srvb, &http_data[hi+8], hl); srvb[hl]=0;
                        server = String(srvb); break;
                    }
                }
                info_str = String(line_buf) + " [Port:" + String(src_port) + "]";
                if (server.length()) info_str += "  [Server:" + server + "]";
                addPacketLog("HTTP", "Resp", src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
                return;
            }
        }
    }

    // Peek TLS v6
    if (src_port == 443 || dst_port == 443) {
        String sni = parseTlsSni(&payload[app_offset], app_len);
        if (sni.length()) {
            addPacketLog("HTTPS", "ClientHello", src_ip6, dst_ip6, rssi, len, "Handshake SNI: " + sni + " [Port:" + String(dst_port) + "]", nullptr, nullptr, payload, len);
            return;
        }
        String shello = parseTlsServerHello(&payload[app_offset], app_len);
        if (shello.length()) {
            addPacketLog("HTTPS", "ServerHello", src_ip6, dst_ip6, rssi, len, "Handshake: " + shello + " [Port:" + String(src_port) + "]", nullptr, nullptr, payload, len);
            return;
        }
    }
    
    // Log ALL TCPv6 (removed filter)
    bool has_payload = (app_len > 0);
    if (has_payload) {
        info_str += "  len=" + String(app_len);
    }
    addPacketLog("TCP", flags_str.length() ? flags_str : "Data", src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
}

// ── UDPv6 ────────────────────────────────────────────────────────────────────

static void handleUdpv6(const uint8_t* payload, int len,
                        int ip6_offset, const char* src_ip6, const char* dst_ip6, int rssi) {
    s_udpCount++;
    int udp_offset = ip6_offset + 40;
    if (len < udp_offset + 8) return;

    uint16_t src_port = (payload[udp_offset] << 8) | payload[udp_offset + 1];
    uint16_t dst_port = (payload[udp_offset + 2] << 8) | payload[udp_offset + 3];
    int udp_payload_offset = udp_offset + 8;

    // DNSv6
    if (src_port == 53 || dst_port == 53) {
        s_dnsCount++;
        String info_str = "DNS6 Port " + String(src_port) + " -> " + String(dst_port);
        bool qr = false;
        if (len >= udp_payload_offset + 12) {
            uint16_t q_count = (payload[udp_payload_offset + 4] << 8) | payload[udp_payload_offset + 5];
            qr = (payload[udp_payload_offset + 2] & 0x80) != 0;
            if (q_count > 0) {
                String query_name = parseDnsName(payload, len, udp_payload_offset + 12);
                if (qr) info_str = "DNS6 Response: " + query_name;
                else info_str = "DNS6 Query: " + query_name;
            }
        }
        addPacketLog("DNS", qr ? "Reply" : "Query", src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
    }
    // mDNSv6 (Port 5353)
    else if (src_port == 5353 || dst_port == 5353) {
        s_mdnsCount++;
        String sub = "Query", info_str = "mDNS6";
        if (len >= udp_payload_offset + 12) {
            bool qr = (payload[udp_payload_offset + 2] & 0x80) != 0;
            uint16_t qdcount = (payload[udp_payload_offset + 4] << 8) | payload[udp_payload_offset + 5];
            uint16_t ancount = (payload[udp_payload_offset + 6] << 8) | payload[udp_payload_offset + 7];
            
            int q_off = udp_payload_offset + 12;
            String name = parseDnsName(payload, len, q_off);
            String svc_tag = (name.indexOf("._tcp") > 0 || name.indexOf("._udp") > 0) ? " [svc]" : "";
            
            int skip = q_off;
            bool ptr_done = false;
            while (skip < len && payload[skip] != 0) {
                if ((payload[skip] & 0xC0) == 0xC0) { skip += 2; ptr_done = true; break; }
                skip += payload[skip] + 1;
            }
            if (!ptr_done) skip++;
            
            uint16_t qtype = (skip + 1 < len) ? ((payload[skip] << 8) | payload[skip + 1]) : 0;
            String qtype_s = parseDnsQtype(qtype);
            
            if (!qr && qdcount > 0) {
                sub = "Query";
                info_str = "mDNS6 Query: " + name + svc_tag + " [" + qtype_s + "]";
            } else if (qr) {
                int ans_off = skip + 4;
                String ans_ip = parseDnsAnswer(payload, len, ans_off, ancount);
                sub = "Reply";
                info_str = "mDNS6 Resp: " + name + svc_tag + " -> "
                         + (ans_ip.length() ? ans_ip : "[" + qtype_s + "]")
                         + "  ans=" + String(ancount);
            } else {
                info_str = "mDNS6 " + name + svc_tag;
            }
        }
        addPacketLog("mDNS", sub, src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
    }
    // WS-Discoveryv6 (Port 3702)
    else if (src_port == 3702 || dst_port == 3702) {
        handleUdpWsd(payload, len, udp_payload_offset, src_ip6, dst_ip6, rssi);
    }
    // CoAPv6 (Port 5683)
    else if (src_port == 5683 || dst_port == 5683) {
        handleUdpCoap(payload, len, udp_payload_offset, src_ip6, dst_ip6, rssi);
    }
    // SSDPv6
    else if (src_port == 1900 || dst_port == 1900) {
        handleUdpSsdp(payload, len, udp_payload_offset, src_ip6, dst_ip6, rssi);
    }
    // NTPv6
    else if (src_port == 123 || dst_port == 123) {
        handleUdpNtp(payload, len, udp_payload_offset, src_ip6, dst_ip6, rssi);
    }
    // LLMNRv6
    else if (src_port == 5355 || dst_port == 5355) {
        handleUdpLlmnr(payload, len, udp_payload_offset, src_ip6, dst_ip6, rssi);
    }
    // DHCPv6
    else if (src_port == 546 || src_port == 547 || dst_port == 546 || dst_port == 547) {
        String info_str = "DHCPv6 Packet";
        if (len >= udp_payload_offset + 1) {
            uint8_t msg_type = payload[udp_payload_offset];
            const char* type_s = (msg_type == 1) ? "Solicit" : (msg_type == 2) ? "Advertise" : 
                                 (msg_type == 3) ? "Request" : (msg_type == 7) ? "Reply" : "Msg";
            info_str = "DHCPv6 " + String(type_s) + " (" + String(msg_type) + ")";
            addPacketLog("DHCPv6", type_s, src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
        }
    }
    // Other UDPv6
    else {
        int udp_payload_len = len - udp_payload_offset;
        if (udp_payload_len > 0) {
            String info_str = "UDP6: Port " + String(src_port) + " -> " + String(dst_port) + "  len=" + String(udp_payload_len);
            addPacketLog("UDP", "UDP", src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
        }
    }
}

// ── Main Dispatcher ──────────────────────────────────────────────────────────

void dispatchIPv6Frame(const uint8_t* payload, int len,
                       int ip6_offset,
                       const char* src_mac, const char* dst_mac, int rssi) {
    if (len < ip6_offset + 40) return;

    uint8_t next_header = payload[ip6_offset + 6];
    
    const uint8_t* src_ip6 = &payload[ip6_offset + 8];
    const uint8_t* dst_ip6 = &payload[ip6_offset + 24];
    String src_ip6_str = formatIPv6(src_ip6);
    String dst_ip6_str = formatIPv6(dst_ip6);

    if (next_header == 58) { // ICMPv6
        handleIcmpv6(payload, len, ip6_offset, src_ip6_str.c_str(), dst_ip6_str.c_str(), rssi);
    } else if (next_header == 6) { // TCPv6
        handleTcpv6(payload, len, ip6_offset, src_ip6_str.c_str(), dst_ip6_str.c_str(), rssi);
    } else if (next_header == 17) { // UDPv6
        handleUdpv6(payload, len, ip6_offset, src_ip6_str.c_str(), dst_ip6_str.c_str(), rssi);
    }
}
