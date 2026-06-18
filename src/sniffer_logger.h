#pragma once
#include <Arduino.h>

// PCAP structures (shared with wifi_sniffer for raw streaming)
#pragma pack(push, 1)
struct pcap_global_header {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
};

struct pcap_packet_header {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};
#pragma pack(pop)

void snifferSetPcapSerial(bool enable);
bool snifferIsPcapSerialActive();
void snifferSetJsonSerial(bool enable);

void addPacketLog(const String& proto, const String& subtype,
                  const String& src, const String& dst,
                  int rssi, int len, const String& info,
                  const char* srcMac, const char* dstMac,
                  const uint8_t* rawPayload, int rawLen);
