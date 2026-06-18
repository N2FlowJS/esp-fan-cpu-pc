/**
 * wifi_sniffer_common.h
 * Internal shared header – structs, extern state, and addPacketLog declaration.
 * NEVER include this from wifi_sniffer.h (public API).
 */
#pragma once

#include <Arduino.h>
#include <vector>
#include <mutex>
#include "esp_wifi.h"

// ── Shared Structs ────────────────────────────────────────────────────────────

struct SnifferDevice {
    uint8_t  mac[6];
    String   ssid;
    int      rssi;
    uint32_t lastSeen;
    uint32_t packetCount; // Track number of packets from/to this device
    bool     isAP;
    uint8_t  channel;
    String   security;
    String   wifiGen;
    int      clients;
    int      utilization;
    String   vendor;
};

struct SnifferPacketLog {
    String   proto;
    String   subtype;     ///< e.g. "Query", "Reply", "SYN", "Ping"
    String   src;
    String   dst;
    int      rssi;
    int      len;
    String   info;
    uint32_t timestamp;
    uint8_t  channel;
    String   srcMac;
    String   dstMac;
    String   rawHex;
    int      ttl;
    int      srcPort;
    int      dstPort;
};

struct BeaconInfo {
    uint8_t channel;
    String security;
    String wifiGen;
    int clients;
    int utilization;
};

// ── Global State (defined in wifi_sniffer.cpp) ────────────────────────────────

extern bool     s_sniffing;
extern bool     s_hopEnabled;
extern bool     s_concurrent;
extern uint8_t  s_currentChannel;

extern std::vector<SnifferDevice>   s_devices;
extern std::vector<SnifferPacketLog> s_packetLogs;
extern std::mutex                   s_snifferMutex;

#include <atomic>

// ... inside statistics counters section
extern std::atomic<uint32_t> s_packetCount;
extern std::atomic<uint32_t> s_beaconCount;
extern std::atomic<uint32_t> s_probeReqCount;
extern std::atomic<uint32_t> s_deauthCount;
extern std::atomic<uint32_t> s_dataCount;
extern std::atomic<uint32_t> s_otherCount;
extern std::atomic<uint32_t> s_arpCount;
extern std::atomic<uint32_t> s_eapolCount;
extern std::atomic<uint32_t> s_dnsCount;
extern std::atomic<uint32_t> s_dhcpCount;
extern std::atomic<uint32_t> s_mdnsCount;
extern std::atomic<uint32_t> s_llmnrCount;
extern std::atomic<uint32_t> s_nbnsCount;
extern std::atomic<uint32_t> s_ssdpCount;
extern std::atomic<uint32_t> s_quicCount;
extern std::atomic<uint32_t> s_icmpCount;
extern std::atomic<uint32_t> s_tcpCount;
extern std::atomic<uint32_t> s_udpCount;
extern std::atomic<uint32_t> s_mqttCount;
extern std::atomic<uint32_t> s_rollingDeauthCount;

// ── Core Logging Function ─────────────────────────────────────────────────────

/// Thread-safe: adds a message to the deferred log queue.
void snifferLog(const char* format, ...);

/// Thread-safe: acquires s_snifferMutex internally.
void addPacketLog(const String& proto, const String& subtype,
                  const String& src,   const String& dst,
                  int rssi, int len,   const String& info,
                  const char* srcMac = nullptr, const char* dstMac = nullptr,
                  const uint8_t* rawPayload = nullptr, int rawLen = 0);

/// Thread-safe: adds a device update to the pending broadcast list.
void addDeviceUpdate(const SnifferDevice& dev);

// ── Layer Dispatcher Declarations ─────────────────────────────────────────────

void dispatchMgmtFrame (uint8_t fc, const uint8_t* payload, int len, int rssi);
void dispatchDataFrame (uint8_t fc, const uint8_t* payload, int len, int rssi);
void dispatchIPv4Frame (const uint8_t* payload, int len,
                        int ip_offset, int llc_end,
                        const char* src_mac, const char* dst_mac, int rssi);
void dispatchIPv6Frame (const uint8_t* payload, int len,
                        int ip6_offset,
                        const char* src_mac, const char* dst_mac, int rssi);

// ── Utility Helper Declarations (defined in wifi_sniffer_utils.cpp) ───────────

String parseDnsName(const uint8_t* payload, int len, int offset);
String parseDnsQtype(uint16_t qtype);
String parseDnsAnswer(const uint8_t* payload, int len, int offset, uint16_t ancount);
String formatIPv6(const uint8_t* ip);
String getDeauthReasonStr(uint16_t reason);
String getIcmpTypeStr(uint8_t t, uint8_t code);
String getIcmpv6TypeStr(uint8_t t);
String getTcpServiceName(uint16_t port);
void handleUdpSsdp(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi);
void handleUdpNtp(const uint8_t* payload, int len, int pl_off,
                 const char* src_ip, const char* dst_ip, int rssi);
void handleUdpLlmnr(const uint8_t* payload, int len, int pl_off,
                   const char* src_ip, const char* dst_ip, int rssi);
void handleUdpNbns(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi);
void handleUdpWsd(const uint8_t* payload, int len, int pl_off,
                  const char* src_ip, const char* dst_ip, int rssi);
void handleUdpCoap(const uint8_t* payload, int len, int pl_off,
                   const char* src_ip, const char* dst_ip, int rssi);
String parseTlsSni(const uint8_t* payload, int len);
String parseTlsServerHello(const uint8_t* payload, int len);
String parseSSID(const uint8_t* payload, int len, int offset);
BeaconInfo parseBeaconDetails(const uint8_t* payload, int len);
bool isRecentlySeen(const uint8_t* mac, bool isAP, int rssi,
                    const String& ssid = "", uint8_t channel = 0, const String& security = "",
                    const String& wifiGen = "", int clients = -1, int util = -1);
String getMacVendor(const uint8_t* mac);
void updateDeviceVendor(const char* macStr, const String& vendor);

