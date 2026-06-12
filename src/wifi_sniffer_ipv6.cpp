/**
 * wifi_sniffer_ipv6.cpp
 * IPv6 layer protocol handlers:
 *   ICMPv6, TCPv6, UDPv6 (DNSv6, mDNSv6).
 */
#include "wifi_sniffer_common.h"

// Declared in wifi_sniffer_utils.cpp
String parseDnsName(const uint8_t* payload, int len, int offset);
String formatIPv6(const uint8_t* ip);
String getIcmpv6TypeStr(uint8_t t);
String parseTlsSni(const uint8_t* payload, int len);
void handleUdpSsdp(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi);
void handleUdpNtp(const uint8_t* payload, int len, int pl_off,
                 const char* src_ip, const char* dst_ip, int rssi);

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
    else if (icmp6_type == 135) sub = "NS";
    else if (icmp6_type == 136) sub = "NA";

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
    
    // Peek HTTPv6
    if (src_port == 80 || dst_port == 80) {
        uint8_t tcp_hl = ((payload[tcp_offset + 12] >> 4) & 0x0F) * 4;
        int http_offset = tcp_offset + tcp_hl;
        int http_len = len - http_offset;
        if (http_len > 4) {
            const uint8_t* http_data = &payload[http_offset];
            if (memcmp(http_data, "GET ", 4) == 0 || 
                memcmp(http_data, "POST", 4) == 0 ||
                memcmp(http_data, "HTTP", 4) == 0) {
                
                int line_len = 0;
                while (line_len < http_len && line_len < 64 && 
                       http_data[line_len] != '\r' && http_data[line_len] != '\n') {
                    line_len++;
                }
                char line_buf[65];
                memcpy(line_buf, http_data, line_len);
                line_buf[line_len] = '\0';
                info_str = "HTTP: " + String(line_buf);
                addPacketLog("HTTP", "Req", src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
                return;
            }
        }
    }

    // Peek TLS SNIv6
    if (src_port == 443 || dst_port == 443) {
        uint8_t tcp_hl = ((payload[tcp_offset + 12] >> 4) & 0x0F) * 4;
        int app_offset = tcp_offset + tcp_hl;
        int app_len = len - app_offset;
        String sni = parseTlsSni(&payload[app_offset], app_len);
        if (sni.length()) {
            addPacketLog("HTTPS", "ClientHello", src_ip6, dst_ip6, rssi, len, "Handshake SNI: " + sni, nullptr, nullptr, payload, len);
            return;
        }
    }
    
    if (s_packetCount % 5 == 0) {
        addPacketLog("TCP", flags_str.length() ? flags_str : "Data", src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
    }
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
        String info_str = "mDNS6 Query/Response";
        bool qr = false;
        if (len >= udp_payload_offset + 12) {
            uint16_t q_count = (payload[udp_payload_offset + 4] << 8) | payload[udp_payload_offset + 5];
            qr = (payload[udp_payload_offset + 2] & 0x80) != 0;
            if (q_count > 0) {
                String mdns_name = parseDnsName(payload, len, udp_payload_offset + 12);
                if (qr) info_str = "mDNS6 Resp: " + mdns_name;
                else info_str = "mDNS6 Query: " + mdns_name;
            }
        }
        addPacketLog("mDNS", qr ? "Reply" : "Query", src_ip6, dst_ip6, rssi, len, info_str, nullptr, nullptr, payload, len);
    }
    // SSDPv6
    else if (src_port == 1900 || dst_port == 1900) {
        handleUdpSsdp(payload, len, udp_payload_offset, src_ip6, dst_ip6, rssi);
    }
    // NTPv6
    else if (src_port == 123 || dst_port == 123) {
        handleUdpNtp(payload, len, udp_payload_offset, src_ip6, dst_ip6, rssi);
    }
    // Other UDPv6
    else {
        String info_str = "UDP6: Port " + String(src_port) + " -> " + String(dst_port);
        if (s_packetCount % 10 == 0) {
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
