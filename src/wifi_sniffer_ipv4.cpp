/**
 * wifi_sniffer_ipv4.cpp
 * IPv4 layer protocol handlers:
 *   ICMP, TCP (with HTTP peek + Host header), DNS, mDNS, DHCP, generic UDP.
 */
#include "wifi_sniffer_common.h"

// ── ICMP ──────────────────────────────────────────────────────────────────────

static void handleIcmp(const uint8_t* payload, int len,
                        int ip_offset, int ip_hl,
                        const char* src_ip, const char* dst_ip, int rssi) {
    s_icmpCount = s_icmpCount + 1;
    int icmp_off = ip_offset + ip_hl;
    if (len < icmp_off + 4) return;

    uint8_t icmp_type = payload[icmp_off];
    uint8_t icmp_code = payload[icmp_off+1];
    String  type_str  = getIcmpTypeStr(icmp_type, icmp_code);
    String  sub       = (icmp_type==8) ? "Ping" : (icmp_type==0) ? "Pong"
                      : (icmp_type==11) ? "TTL" : "T" + String(icmp_type);
    String  info      = type_str;

    if ((icmp_type == 8 || icmp_type == 0) && len >= icmp_off + 8) {
        uint16_t id  = (payload[icmp_off+4] << 8) | payload[icmp_off+5];
        uint16_t seq = (payload[icmp_off+6] << 8) | payload[icmp_off+7];
        info = type_str + "  id=" + String(id) + " seq=" + String(seq);
        
        int data_len = len - (icmp_off + 8);
        if (data_len > 0) {
            String data_peek = "";
            int peek_limit = data_len > 16 ? 16 : data_len;
            for (int i = 0; i < peek_limit; i++) {
                char c = payload[icmp_off + 8 + i];
                if (c >= 32 && c <= 126) data_peek += c;
                else data_peek += ".";
            }
            info += "  data=\"" + data_peek + "\"";
        }
    }
    addPacketLog("ICMP", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
    if (!s_concurrent) snifferLog("[SNIFFER] [ICMP] %s", info.c_str());
}

// ── TCP ───────────────────────────────────────────────────────────────────────

static void handleTcp(const uint8_t* payload, int len,
                       int ip_offset, int ip_hl,
                       const char* src_ip, const char* dst_ip, int rssi) {
    s_tcpCount = s_tcpCount + 1;
    int tcp_off = ip_offset + ip_hl;
    if (len < tcp_off + 20) return;

    uint16_t src_port = (payload[tcp_off]   << 8) | payload[tcp_off+1];
    uint16_t dst_port = (payload[tcp_off+2] << 8) | payload[tcp_off+3];
    uint32_t seq_num  = ((uint32_t)payload[tcp_off+4]<<24) | ((uint32_t)payload[tcp_off+5]<<16)
                      | ((uint32_t)payload[tcp_off+6]<<8)  |  (uint32_t)payload[tcp_off+7];
    uint8_t  tcp_flags = payload[tcp_off+13];

    // Build flags string
    String flags;
    if (tcp_flags & 0x02) flags = "SYN";
    if (tcp_flags & 0x10) { if (flags.length()) flags+="+"; flags+="ACK"; }
    if (tcp_flags & 0x01) { if (flags.length()) flags+="+"; flags+="FIN"; }
    if (tcp_flags & 0x04) { if (flags.length()) flags+="+"; flags+="RST"; }
    if (tcp_flags & 0x08) { if (flags.length()) flags+="+"; flags+="PSH"; }
    if (!flags.length())  flags = "DATA";

    String src_lbl = getTcpServiceName(src_port);
    String dst_lbl = getTcpServiceName(dst_port);
    if (!src_lbl.length()) src_lbl = String(src_port);
    if (!dst_lbl.length()) dst_lbl = String(dst_port);

    String state;
    if      ((tcp_flags & 0x12) == 0x02) state = "New";
    else if ((tcp_flags & 0x12) == 0x12) state = "HS";
    else if  (tcp_flags & 0x01)          state = "Fin";
    else if  (tcp_flags & 0x04)          state = "RST";
    else                                  state = "Data";

    // HTTP peek
    uint8_t tcp_hl    = ((payload[tcp_off+12] >> 4) & 0x0F) * 4;
    int     app_off   = tcp_off + tcp_hl;
    int     app_len   = len - app_off;
    bool    is_http   = (dst_port==80||dst_port==8080||src_port==80||src_port==8080);

    if (is_http && app_len > 4) {
        const uint8_t* app = &payload[app_off];
        if (memcmp(app,"GET ",4)==0 || memcmp(app,"POST",4)==0 ||
            memcmp(app,"PUT ",4)==0 || memcmp(app,"DELE",4)==0 ||
            memcmp(app,"HEAD",4)==0 || memcmp(app,"PATC",4)==0 ||
            memcmp(app,"OPTI",4)==0 || memcmp(app,"CONN",4)==0) {
            // Extract request line
            int rl = 0;
            while (rl < app_len && rl < 80 && app[rl]!='\r' && app[rl]!='\n') rl++;
            char req[81]; memcpy(req, app, rl); req[rl] = 0;
            
            // Parse Method and URL
            String method_str = "";
            String url_str = "";
            int sp1 = -1, sp2 = -1;
            for (int i = 0; i < rl; i++) {
                if (req[i] == ' ') {
                    if (sp1 == -1) sp1 = i;
                    else if (sp2 == -1) sp2 = i;
                }
            }
            if (sp1 != -1 && sp2 != -1) {
                req[sp1] = '\0';
                req[sp2] = '\0';
                method_str = String(req);
                url_str = String(&req[sp1+1]);
            } else {
                method_str = "REQ";
                url_str = String(req);
            }

            // Find Host header
            String host;
            for (int hi = 0; hi < app_len - 6; hi++) {
                if (memcmp(&app[hi], "Host: ", 6) == 0) {
                    int he = hi+6;
                    while (he < app_len && app[he]!='\r' && app[he]!='\n') he++;
                    int hl = he-(hi+6); if (hl>48) hl=48;
                    char hb[49]; memcpy(hb, &app[hi+6], hl); hb[hl]=0;
                    host = String(hb); break;
                }
            }
            // Find User-Agent header
            String ua;
            for (int hi = 0; hi < app_len - 12; hi++) {
                if (strncasecmp((const char*)&app[hi], "User-Agent: ", 12) == 0) {
                    int he = hi+12;
                    while (he < app_len && app[he]!='\r' && app[he]!='\n') he++;
                    int hl = he-(hi+12); if (hl>40) hl=40;
                    char uab[41]; memcpy(uab, &app[hi+12], hl); uab[hl]=0;
                    ua = String(uab); break;
                }
            }
            
            // Find Content-Type header (useful for POST/PUT)
            String ctype;
            if (method_str == "POST" || method_str == "PUT" || method_str == "PATCH") {
                for (int hi = 0; hi < app_len - 14; hi++) {
                    if (strncasecmp((const char*)&app[hi], "Content-Type: ", 14) == 0) {
                        int he = hi+14;
                        while (he < app_len && app[he]!='\r' && app[he]!='\n') he++;
                        int hl = he-(hi+14); if (hl>30) hl=30;
                        char ctb[31]; memcpy(ctb, &app[hi+14], hl); ctb[hl]=0;
                        ctype = String(ctb); break;
                    }
                }
            }

            // Find Referer header
            String referer;
            for (int hi = 0; hi < app_len - 9; hi++) {
                if (strncasecmp((const char*)&app[hi], "Referer: ", 9) == 0) {
                    int he = hi+9;
                    while (he < app_len && app[he]!='\r' && app[he]!='\n') he++;
                    int hl = he-(hi+9); if (hl>40) hl=40;
                    char refb[41]; memcpy(refb, &app[hi+9], hl); refb[hl]=0;
                    referer = String(refb); break;
                }
            }
            
            String info = method_str + " " + url_str + " [Port:" + String(dst_port) + "]";
            if (host.length()) info += "  [Host:" + host + "]";
            if (ctype.length()) info += "  [Type:" + ctype + "]";
            if (ua.length()) info += "  [UA:" + ua + "...]";
            if (referer.length()) info += "  [Ref:" + referer + "]";
            addPacketLog("HTTP", method_str, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
            return;
        }
        if (memcmp(app,"HTTP",4)==0) {
            int sl = 0;
            while (sl < app_len && sl < 32 && app[sl]!='\r' && app[sl]!='\n') sl++;
            char sb[33]; memcpy(sb, app, sl); sb[sl]=0;
            
            // Find Server header
            String server;
            for (int hi = 0; hi < app_len - 8; hi++) {
                if (strncasecmp((const char*)&app[hi], "Server: ", 8) == 0) {
                    int he = hi+8;
                    while (he < app_len && app[he]!='\r' && app[he]!='\n') he++;
                    int hl = he-(hi+8); if (hl>30) hl=30;
                    char srvb[31]; memcpy(srvb, &app[hi+8], hl); srvb[hl]=0;
                    server = String(srvb); break;
                }
            }
            String info = String(sb) + " [Port:" + String(src_port) + "]";
            if (server.length()) info += "  [Server:" + server + "]";
            addPacketLog("HTTP", "Resp", src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
            return;
        }
    }

    // MQTT peek
    if (dst_port == 1883 || src_port == 1883 || dst_port == 8883 || src_port == 8883) {
        if (app_len >= 2) {
            const uint8_t* app = &payload[app_off];
            uint8_t type = (app[0] >> 4) & 0x0F;
            String mqtt_sub = "Unknown";
            String mqtt_info = "";
            switch (type) {
                case 1: mqtt_sub = "CONNECT"; break;
                case 2: mqtt_sub = "CONNACK"; break;
                case 3: 
                    mqtt_sub = "PUBLISH"; 
                    if (app_len > 4) {
                        int topic_len = (app[2] << 8) | app[3];
                        if (app_len >= 4 + topic_len) {
                            char topic[64];
                            int tl = topic_len > 63 ? 63 : topic_len;
                            memcpy(topic, &app[4], tl);
                            topic[tl] = 0;
                            // Clean up unprintable chars
                            for(int i=0; i<tl; i++) if(topic[i]<32 || topic[i]>126) topic[i]='.';
                            mqtt_info = "Topic: " + String(topic);
                        }
                    }
                    break;
                case 4: mqtt_sub = "PUBACK"; break;
                case 8: mqtt_sub = "SUBSCRIBE"; break;
                case 9: mqtt_sub = "SUBACK"; break;
                case 12: mqtt_sub = "PINGREQ"; break;
                case 13: mqtt_sub = "PINGRESP"; break;
                case 14: mqtt_sub = "DISCONNECT"; break;
            }
            if (mqtt_sub != "Unknown") {
                s_mqttCount = s_mqttCount + 1;
                String proto = (dst_port == 8883 || src_port == 8883) ? "MQTTS" : "MQTT";
                addPacketLog(proto, mqtt_sub, src_ip, dst_ip, rssi, len, mqtt_info, nullptr, nullptr, payload, len);
                return;
            }
        }
    }

    // TLS peek
    if (dst_port == 443 || src_port == 443) {
        String sni = parseTlsSni(&payload[app_off], app_len);
        if (sni.length()) {
            addPacketLog("HTTPS", "ClientHello", src_ip, dst_ip, rssi, len, "Handshake SNI: " + sni + " [Port:" + String(dst_port) + "]", nullptr, nullptr, payload, len);
            return;
        }
        String shello = parseTlsServerHello(&payload[app_off], app_len);
        if (shello.length()) {
            addPacketLog("HTTPS", "ServerHello", src_ip, dst_ip, rssi, len, "Handshake: " + shello + " [Port:" + String(src_port) + "]", nullptr, nullptr, payload, len);
            return;
        }
    }

    // Log ALL TCP (removed filter)
    bool has_payload = (app_len > 0);
    String info = String(src_ip) + ":" + src_lbl + " -> " + dst_ip + ":" + dst_lbl
                + "  [" + flags + "]  " + state
                + "  seq=" + String(seq_num % 100000)
                + (has_payload ? "  len=" + String(app_len) : "");
    addPacketLog("TCP", flags, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

// ── UDP / DNS ─────────────────────────────────────────────────────────────────

static void handleUdpDns(const uint8_t* payload, int len, int pl_off,
                          const char* src_ip, const char* dst_ip,
                          uint16_t src_port, uint16_t dst_port, int rssi) {
    s_dnsCount = s_dnsCount + 1;
    String sub = "Query", info = "DNS";
    if (len >= pl_off + 12) {
        uint16_t txid    = (payload[pl_off]   << 8) | payload[pl_off+1];
        bool     qr      = (payload[pl_off+2] & 0x80) != 0;
        uint16_t qdcount = (payload[pl_off+4] << 8) | payload[pl_off+5];
        uint16_t ancount = (payload[pl_off+6] << 8) | payload[pl_off+7];

        int q_off = pl_off + 12;
        String qname = parseDnsName(payload, len, q_off);

        // Walk past qname to find QTYPE
        int skip = q_off;
        bool ptr_done = false;
        while (skip < len && payload[skip] != 0) {
            if ((payload[skip] & 0xC0) == 0xC0) { skip += 2; ptr_done = true; break; }
            skip += payload[skip] + 1;
        }
        if (!ptr_done) skip++;

        uint16_t qtype   = (skip+1 < len) ? ((payload[skip]<<8)|payload[skip+1]) : 0;
        String   qtype_s = parseDnsQtype(qtype);

        if (!qr && qdcount > 0) {
            sub  = "Query";
            info = "DNS Query: " + qname + " [" + qtype_s + "] id=0x" + String(txid, HEX);
        } else if (qr) {
            int ans_off = skip + 4; // past QTYPE + QCLASS
            String ans_ip = parseDnsAnswer(payload, len, ans_off, ancount);
            sub  = "Reply";
            info = "DNS Reply: " + qname + " -> "
                 + (ans_ip.length() ? ans_ip : "[" + qtype_s + "]")
                 + "  ans=" + String(ancount);
        }
    }
    addPacketLog("DNS", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
    snifferLog("[SNIFFER] [DNS %s] %s", sub.c_str(), info.c_str());
}

// ── UDP / mDNS ────────────────────────────────────────────────────────────────

static void handleUdpMdns(const uint8_t* payload, int len, int pl_off,
                           const char* src_ip, const char* dst_ip, int rssi) {
    s_mdnsCount = s_mdnsCount + 1;
    String sub = "Query", info = "mDNS";
    if (len >= pl_off + 12) {
        bool     qr      = (payload[pl_off+2] & 0x80) != 0;
        uint16_t qdcount = (payload[pl_off+4] << 8) | payload[pl_off+5];
        uint16_t ancount = (payload[pl_off+6] << 8) | payload[pl_off+7];

        int q_off = pl_off + 12;
        String name = parseDnsName(payload, len, q_off);
        String svc_tag = (name.indexOf("._tcp") > 0 || name.indexOf("._udp") > 0) ? " [svc]" : "";

        // Walk past name to find QTYPE
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
            sub  = "Query";
            info = "mDNS Query: " + name + svc_tag + " [" + qtype_s + "]";
        } else if (qr) {
            int ans_off = skip + 4; // past QTYPE + QCLASS
            String ans_ip = parseDnsAnswer(payload, len, ans_off, ancount);
            sub  = "Reply";
            info = "mDNS Resp: " + name + svc_tag + " -> "
                 + (ans_ip.length() ? ans_ip : "[" + qtype_s + "]")
                 + "  ans=" + String(ancount);
        } else {
            info = "mDNS " + name + svc_tag;
        }
    }
    addPacketLog("mDNS", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

// ── UDP / DHCP ────────────────────────────────────────────────────────────────

static void handleUdpDhcp(const uint8_t* payload, int len, int pl_off,
                           const char* src_ip, const char* dst_ip, int rssi) {
    s_dhcpCount = s_dhcpCount + 1;
    String sub = "?", dhcp_ip, dhcp_srv, dhcp_host, dhcp_gw, dhcp_dns, dhcp_mask, dhcp_vendor;
    uint32_t lease = 0;

    if (len >= pl_off + 240) {
        // yiaddr (offered IP)
        char yi[16];
        snprintf(yi, sizeof(yi), "%d.%d.%d.%d",
                 payload[pl_off+16], payload[pl_off+17],
                 payload[pl_off+18], payload[pl_off+19]);
        if (payload[pl_off+16] != 0) dhcp_ip = String(yi);

        // Check DHCP magic cookie
        if (payload[pl_off+236]==0x63 && payload[pl_off+237]==0x82 &&
            payload[pl_off+238]==0x53 && payload[pl_off+239]==0x63) {
            int oi = pl_off + 240;
            while (oi < len - 2) {
                uint8_t ot = payload[oi];
                if (ot == 255) break;
                if (ot == 0)  { oi++; continue; }
                uint8_t ol = payload[oi+1];
                if (oi + 2 + ol > len) break;

                if (ot == 53 && ol == 1) {
                    switch(payload[oi+2]) {
                        case 1: sub="Discover"; break; case 2: sub="Offer";   break;
                        case 3: sub="Request";  break; case 4: sub="Decline"; break;
                        case 5: sub="ACK";      break; case 6: sub="NAK";     break;
                        case 7: sub="Release";  break; case 8: sub="Inform";  break;
                        default: sub="Msg"+String(payload[oi+2]); break;
                    }
                } else if (ot == 50 && ol == 4) {
                    char r[16]; snprintf(r,sizeof(r),"%d.%d.%d.%d",
                        payload[oi+2],payload[oi+3],payload[oi+4],payload[oi+5]);
                    if (!dhcp_ip.length()) dhcp_ip = String(r);
                } else if (ot == 51 && ol == 4) {
                    lease = ((uint32_t)payload[oi+2]<<24)|((uint32_t)payload[oi+3]<<16)
                           |((uint32_t)payload[oi+4]<<8)|payload[oi+5];
                } else if (ot == 54 && ol == 4) {
                    char s[16]; snprintf(s,sizeof(s),"%d.%d.%d.%d",
                        payload[oi+2],payload[oi+3],payload[oi+4],payload[oi+5]);
                    dhcp_srv = String(s);
                } else if (ot == 12) {
                    int hl = ol < 32 ? ol : 32;
                    char hb[33]; memcpy(hb, &payload[oi+2], hl); hb[hl]=0;
                    dhcp_host = String(hb);
                } else if (ot == 60) {
                    int hl = ol < 32 ? ol : 32;
                    char vb[33]; memcpy(vb, &payload[oi+2], hl); vb[hl]=0;
                    dhcp_vendor = String(vb);
                } else if (ot == 1 && ol == 4) {
                    char r[16]; snprintf(r,sizeof(r),"%d.%d.%d.%d",
                        payload[oi+2],payload[oi+3],payload[oi+4],payload[oi+5]);
                    dhcp_mask = String(r);
                } else if (ot == 3 && ol >= 4) {
                    char r[16]; snprintf(r,sizeof(r),"%d.%d.%d.%d",
                        payload[oi+2],payload[oi+3],payload[oi+4],payload[oi+5]);
                    dhcp_gw = String(r);
                } else if (ot == 6 && ol >= 4) {
                    String dns_ips = "";
                    for (int d = 0; d < ol && d < 8; d += 4) {
                        char r[16]; snprintf(r,sizeof(r),"%d.%d.%d.%d",
                            payload[oi+2+d],payload[oi+3+d],payload[oi+4+d],payload[oi+5+d]);
                        if (dns_ips.length()) dns_ips += ",";
                        dns_ips += String(r);
                    }
                    dhcp_dns = dns_ips;
                }
                oi += 2 + ol;
            }
        }
    }

    String info = "DHCP " + sub;
    if (dhcp_ip.length())   info += "  ip="    + dhcp_ip;
    if (dhcp_srv.length())  info += "  srv="   + dhcp_srv;
    if (dhcp_host.length()) info += "  host="  + dhcp_host;
    if (dhcp_vendor.length()) info += "  vendor=" + dhcp_vendor;
    if (dhcp_mask.length()) info += "  mask="  + dhcp_mask;
    if (dhcp_gw.length())   info += "  gw="    + dhcp_gw;
    if (dhcp_dns.length())  info += "  dns="   + dhcp_dns;
    if (lease > 0)          info += "  lease=" + String(lease/3600) + "h";
    addPacketLog("DHCP", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
    snifferLog("[SNIFFER] [DHCP %s] %s", sub.c_str(), info.c_str());
}

// ── UDP dispatcher ────────────────────────────────────────────────────────────

static void handleUdp(const uint8_t* payload, int len,
                       int ip_offset, int ip_hl,
                       const char* src_ip, const char* dst_ip, int rssi) {
    s_udpCount = s_udpCount + 1;
    int udp_off = ip_offset + ip_hl;
    if (len < udp_off + 8) return;

    uint16_t src_port = (payload[udp_off]   << 8) | payload[udp_off+1];
    uint16_t dst_port = (payload[udp_off+2] << 8) | payload[udp_off+3];
    int      pl_off   = udp_off + 8;

    if (src_port == 53 || dst_port == 53) {
        handleUdpDns(payload, len, pl_off, src_ip, dst_ip, src_port, dst_port, rssi);
    } else if (src_port == 5353 || dst_port == 5353) {
        handleUdpMdns(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if ((src_port==67 && dst_port==68) || (src_port==68 && dst_port==67)) {
        handleUdpDhcp(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if (src_port == 1900 || dst_port == 1900) {
        handleUdpSsdp(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if (src_port == 123 || dst_port == 123) {
        handleUdpNtp(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if (src_port == 5355 || dst_port == 5355) {
        handleUdpLlmnr(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if (src_port == 137 || dst_port == 137) {
        handleUdpNbns(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if (src_port == 3702 || dst_port == 3702) {
        handleUdpWsd(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if (src_port == 5683 || dst_port == 5683) {
        handleUdpCoap(payload, len, pl_off, src_ip, dst_ip, rssi);
    } else if (dst_port == 443 || src_port == 443) {
        // Basic QUIC detection
        if (len - pl_off > 10) {
            uint8_t first = payload[pl_off];
            if (first & 0x80) { // Long header
                uint32_t version = ((uint32_t)payload[pl_off+1]<<24) | ((uint32_t)payload[pl_off+2]<<16) | 
                                   ((uint32_t)payload[pl_off+3]<<8) | payload[pl_off+4];
                if (version == 1 || (version & 0xFF000000) == 0xFF000000) { // QUIC v1 or Draft
                    s_quicCount = s_quicCount + 1;
                    addPacketLog("QUIC", "Init", src_ip, dst_ip, rssi, len, "QUIC Long Header v" + String(version, HEX), nullptr, nullptr, payload, len);
                    return;
                }
            } else if (first & 0x40) { // Short header (Fixed bit must be 1)
                s_quicCount = s_quicCount + 1;
                addPacketLog("QUIC", "Data", src_ip, dst_ip, rssi, len, "QUIC Short Header", nullptr, nullptr, payload, len);
                return;
            }
        }
        int udp_payload_len = len - pl_off;
        if (udp_payload_len > 0) {
            addPacketLog("UDP", "HTTPS", src_ip, dst_ip, rssi, len, "UDP/443 (likely QUIC)  len=" + String(udp_payload_len), nullptr, nullptr, payload, len);
        }
    } else {
        int udp_payload_len = len - pl_off;
        if (udp_payload_len > 0) {
            String svc = getTcpServiceName(dst_port);
            if (!svc.length()) svc = getTcpServiceName(src_port);
            String info = (svc.length() ? "[" + svc + "] " : "")
                        + String(src_ip) + ":" + String(src_port)
                        + " -> " + dst_ip + ":" + String(dst_port)
                        + "  len=" + String(udp_payload_len);
            addPacketLog("UDP", svc.length() ? svc : "UDP", src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
        }
    }
}

// ── Main IPv4 Dispatcher ──────────────────────────────────────────────────────

void dispatchIPv4Frame(const uint8_t* payload, int len,
                        int ip_offset, int /*llc_end*/,
                        const char* src_mac, const char* dst_mac, int rssi) {
    if (len < ip_offset + 20) return;

    uint8_t ip_hl    = (payload[ip_offset] & 0x0F) * 4;
    uint8_t protocol = payload[ip_offset+9];

    char src_ip[16], dst_ip[16];
    snprintf(src_ip, sizeof(src_ip), "%d.%d.%d.%d",
             payload[ip_offset+12], payload[ip_offset+13],
             payload[ip_offset+14], payload[ip_offset+15]);
    snprintf(dst_ip, sizeof(dst_ip), "%d.%d.%d.%d",
             payload[ip_offset+16], payload[ip_offset+17],
             payload[ip_offset+18], payload[ip_offset+19]);

    switch (protocol) {
        case 1:  handleIcmp(payload, len, ip_offset, ip_hl, src_ip, dst_ip, rssi); break;
        case 6:  handleTcp (payload, len, ip_offset, ip_hl, src_ip, dst_ip, rssi); break;
        case 17: handleUdp (payload, len, ip_offset, ip_hl, src_ip, dst_ip, rssi); break;
        default: break;
    }
}
