/**
 * wifi_sniffer_ipv4.cpp
 * IPv4 layer protocol handlers:
 *   ICMP, TCP (with HTTP peek + Host header), DNS, mDNS, DHCP, generic UDP.
 */
#include "wifi_sniffer_common.h"

// Declared in wifi_sniffer_utils.cpp
String parseDnsName   (const uint8_t*, int, int);
String parseDnsQtype  (uint16_t);
String parseDnsAnswer (const uint8_t*, int, int, uint16_t);
String getIcmpTypeStr (uint8_t, uint8_t);
String getTcpServiceName(uint16_t);
String parseTlsSni    (const uint8_t*, int);

// ── ICMP ──────────────────────────────────────────────────────────────────────

static void handleIcmp(const uint8_t* payload, int len,
                        int ip_offset, int ip_hl,
                        const char* src_ip, const char* dst_ip, int rssi) {
    s_icmpCount++;
    int icmp_off = ip_offset + ip_hl;
    if (len < icmp_off + 4) return;

    uint8_t icmp_type = payload[icmp_off];
    uint8_t icmp_code = payload[icmp_off+1];
    String  type_str  = getIcmpTypeStr(icmp_type, icmp_code);
    String  sub       = (icmp_type==8) ? "Ping" : (icmp_type==0) ? "Pong"
                      : (icmp_type==11) ? "TTL" : "T" + String(icmp_type);
    String  info      = type_str + "  " + src_ip + " -> " + dst_ip;

    if ((icmp_type == 8 || icmp_type == 0) && len >= icmp_off + 8) {
        uint16_t id  = (payload[icmp_off+4] << 8) | payload[icmp_off+5];
        uint16_t seq = (payload[icmp_off+6] << 8) | payload[icmp_off+7];
        info = type_str + "  " + src_ip + " -> " + dst_ip
             + "  id=" + String(id) + " seq=" + String(seq);
    }
    addPacketLog("ICMP", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
    if (!s_concurrent) Serial.printf("[SNIFFER] [ICMP] %s\n", info.c_str());
}

// ── TCP ───────────────────────────────────────────────────────────────────────

static void handleTcp(const uint8_t* payload, int len,
                       int ip_offset, int ip_hl,
                       const char* src_ip, const char* dst_ip, int rssi) {
    s_tcpCount++;
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
            memcmp(app,"HEAD",4)==0 || memcmp(app,"PATC",4)==0) {
            // Extract request line
            int rl = 0;
            while (rl < app_len && rl < 80 && app[rl]!='\r' && app[rl]!='\n') rl++;
            char req[81]; memcpy(req, app, rl); req[rl] = 0;
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
            String info = String(req);
            if (host.length()) info += "  [Host:" + host + "]";
            addPacketLog("HTTP", "Req", src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
            return;
        }
        if (memcmp(app,"HTTP",4)==0) {
            int sl = 0;
            while (sl < app_len && sl < 32 && app[sl]!='\r' && app[sl]!='\n') sl++;
            char sb[33]; memcpy(sb, app, sl); sb[sl]=0;
            addPacketLog("HTTP", "Resp", src_ip, dst_ip, rssi, len, String(sb), nullptr, nullptr, payload, len);
            return;
        }
    }

    // TLS SNI peek
    if (dst_port == 443 || src_port == 443) {
        String sni = parseTlsSni(&payload[app_off], app_len);
        if (sni.length()) {
            addPacketLog("HTTPS", "ClientHello", src_ip, dst_ip, rssi, len, "Handshake SNI: " + sni, nullptr, nullptr, payload, len);
            return;
        }
    }

    // Rate-limit generic TCP to every 5th packet
    if (s_packetCount % 5 == 0) {
        String info = String(src_ip) + ":" + src_lbl + " -> " + dst_ip + ":" + dst_lbl
                    + "  [" + flags + "]  " + state
                    + "  seq=" + String(seq_num % 100000);
        addPacketLog("TCP", flags, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
    }
}

// ── UDP / DNS ─────────────────────────────────────────────────────────────────

static void handleUdpDns(const uint8_t* payload, int len, int pl_off,
                          const char* src_ip, const char* dst_ip,
                          uint16_t src_port, uint16_t dst_port, int rssi) {
    s_dnsCount++;
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
    if (!s_concurrent) Serial.printf("[SNIFFER] [DNS %s] %s\n", sub.c_str(), info.c_str());
}

// ── UDP / mDNS ────────────────────────────────────────────────────────────────

static void handleUdpMdns(const uint8_t* payload, int len, int pl_off,
                           const char* src_ip, const char* dst_ip, int rssi) {
    s_mdnsCount++;
    String sub = "Query", info = "mDNS";
    if (len >= pl_off + 12) {
        bool     qr      = (payload[pl_off+2] & 0x80) != 0;
        uint16_t qdcount = (payload[pl_off+4] << 8) | payload[pl_off+5];
        uint16_t ancount = (payload[pl_off+6] << 8) | payload[pl_off+7];
        String   name    = parseDnsName(payload, len, pl_off + 12);
        String   svc_tag = (name.indexOf("._tcp")>0 || name.indexOf("._udp")>0) ? " [svc]" : "";
        if (!qr && qdcount > 0) {
            sub  = "Query";
            info = "mDNS Query: " + name + svc_tag;
        } else if (qr) {
            sub  = "Reply";
            info = "mDNS Resp: " + name + svc_tag + "  ans=" + String(ancount);
        } else {
            info = "mDNS " + name + svc_tag;
        }
    }
    addPacketLog("mDNS", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
}

// ── UDP / DHCP ────────────────────────────────────────────────────────────────

static void handleUdpDhcp(const uint8_t* payload, int len, int pl_off,
                           const char* src_ip, const char* dst_ip, int rssi) {
    s_dhcpCount++;
    String sub = "?", dhcp_ip, dhcp_srv, dhcp_host;
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
                }
                oi += 2 + ol;
            }
        }
    }

    String info = "DHCP " + sub;
    if (dhcp_ip.length())   info += "  ip="    + dhcp_ip;
    if (dhcp_srv.length())  info += "  srv="   + dhcp_srv;
    if (dhcp_host.length()) info += "  host="  + dhcp_host;
    if (lease > 0)          info += "  lease=" + String(lease/3600) + "h";
    addPacketLog("DHCP", sub, src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
    Serial.printf("[SNIFFER] [DHCP %s] %s\n", sub.c_str(), info.c_str());
}

// ── UDP dispatcher ────────────────────────────────────────────────────────────

static void handleUdp(const uint8_t* payload, int len,
                       int ip_offset, int ip_hl,
                       const char* src_ip, const char* dst_ip, int rssi) {
    s_udpCount++;
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
    } else if (s_packetCount % 10 == 0) {
        String svc = getTcpServiceName(dst_port);
        if (!svc.length()) svc = getTcpServiceName(src_port);
        String info = (svc.length() ? "[" + svc + "] " : "")
                    + String(src_ip) + ":" + String(src_port)
                    + " -> " + dst_ip + ":" + String(dst_port);
        addPacketLog("UDP", svc.length() ? svc : "UDP", src_ip, dst_ip, rssi, len, info, nullptr, nullptr, payload, len);
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
