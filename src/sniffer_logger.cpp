#include "wifi_sniffer_common.h"
#include "web_server.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <vector>
#include <mutex>

// PCAP structures
#pragma pack(push, 1)
struct pcap_global_header {
    uint32_t magic_number;  /* magic number */
    uint16_t version_major; /* major version number */
    uint16_t version_minor; /* minor version number */
    int32_t  thiszone;      /* GMT to local correction */
    uint32_t sigfigs;       /* accuracy of timestamps */
    uint32_t snaplen;       /* max length of captured packets, in octets */
    uint32_t network;       /* data link type (105 for IEEE 802.11) */
};

struct pcap_packet_header {
    uint32_t ts_sec;   /* timestamp seconds */
    uint32_t ts_usec;  /* timestamp microseconds */
    uint32_t incl_len; /* number of octets of packet saved in file */
    uint32_t orig_len; /* actual length of packet */
};
#pragma pack(pop)

bool s_pcapSerialActive = false;
bool s_jsonSerialActive = false;

void snifferSetPcapSerial(bool enable) {
    if (s_pcapSerialActive == enable) return;
    if (enable) {
        if (!Serial) {
            Serial.println("[ERR] USB Host app not detected. Cannot start PCAP.");
            return;
        }
        pcap_global_header global_hdr;
        global_hdr.magic_number = 0xa1b2c3d4;
        global_hdr.version_major = 2;
        global_hdr.version_minor = 4;
        global_hdr.thiszone = 0;
        global_hdr.sigfigs = 0;
        global_hdr.snaplen = 65535;
        global_hdr.network = 105; // LINKTYPE_IEEE802_11
        Serial.write((const uint8_t*)&global_hdr, sizeof(global_hdr));
    }
    s_pcapSerialActive = enable;
}

bool snifferIsPcapSerialActive() {
    return s_pcapSerialActive;
}

void snifferSetJsonSerial(bool enable) {
    if (enable && !Serial) {
        Serial.println("[ERR] USB Host app not detected. Cannot start DB Sync.");
        return;
    }
    s_jsonSerialActive = enable;
}

void addPacketLog(const String& proto, const String& subtype,
                  const String& src, const String& dst,
                  int rssi, int len, const String& info,
                  const char* srcMac, const char* dstMac,
                  const uint8_t* rawPayload, int rawLen) {

    static uint32_t logCount = 0;
    logCount++;

    if (logCount % 10 == 0) {
        Serial.printf("[LOG] Adding packet #%d: %s %s %s->%s rssi=%d\n", logCount, proto.c_str(), subtype.c_str(), src.c_str(), dst.c_str(), rssi);
    }

    // Acquire shared packet log vector from wifi_sniffer.cpp
    extern std::mutex s_snifferMutex;
    extern std::vector<SnifferPacketLog> s_packetLogs;

    std::lock_guard<std::mutex> lock(s_snifferMutex);
    SnifferPacketLog entry;
    entry.proto     = proto;
    entry.subtype   = subtype;
    entry.src       = src;
    entry.dst       = dst;
    entry.rssi      = rssi;
    entry.len       = len;
    entry.info      = info;
    entry.timestamp = millis();
    // channel is filled by caller via s_currentChannel in wifi_sniffer.cpp
    extern uint8_t s_currentChannel;
    entry.channel   = s_currentChannel;
    entry.ttl       = -1;
    entry.srcPort   = -1;
    entry.dstPort   = -1;

    if (srcMac && strlen(srcMac) > 0) {
        entry.srcMac = srcMac;
    } else if (rawPayload && rawLen >= 16) {
        char sMac[18], dMac[18];
        snprintf(dMac, sizeof(dMac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 rawPayload[4], rawPayload[5], rawPayload[6], rawPayload[7], rawPayload[8], rawPayload[9]);
        snprintf(sMac, sizeof(sMac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 rawPayload[10], rawPayload[11], rawPayload[12], rawPayload[13], rawPayload[14], rawPayload[15]);
        entry.srcMac = sMac;
        entry.dstMac = dMac;

        uint8_t fc = rawPayload[0];
        uint8_t type_val = fc & 0x0C;
        if (type_val == 0x08) {
            bool encrypted = (rawPayload[1] & 0x40) != 0;
            if (!encrypted) {
                int header_len = ((fc & 0x80) != 0) ? 26 : 24;
                int llc_offset = header_len;
                if (rawLen >= llc_offset + 8) {
                    if (rawPayload[llc_offset] == 0xAA && rawPayload[llc_offset + 1] == 0xAA && rawPayload[llc_offset + 2] == 0x03) {
                        uint16_t ether_type = (rawPayload[llc_offset + 6] << 8) | rawPayload[llc_offset + 7];
                        int ip_offset = llc_offset + 8;
                        if (ether_type == 0x0800 && rawLen >= ip_offset + 20) {
                            entry.ttl = rawPayload[ip_offset + 8];
                            uint8_t protocol = rawPayload[ip_offset + 9];
                            uint8_t ip_hl = (rawPayload[ip_offset] & 0x0F) * 4;
                            int l4_offset = ip_offset + ip_hl;
                            if ((protocol == 6 || protocol == 17) && rawLen >= l4_offset + 4) {
                                entry.srcPort = (rawPayload[l4_offset] << 8) | rawPayload[l4_offset + 1];
                                entry.dstPort = (rawPayload[l4_offset + 2] << 8) | rawPayload[l4_offset + 3];
                            }
                        } else if (ether_type == 0x86DD && rawLen >= ip_offset + 40) {
                            entry.ttl = rawPayload[ip_offset + 7];
                            uint8_t next_hdr = rawPayload[ip_offset + 6];
                            int l4_offset = ip_offset + 40;
                            if ((next_hdr == 6 || next_hdr == 17) && rawLen >= l4_offset + 4) {
                                entry.srcPort = (rawPayload[l4_offset] << 8) | rawPayload[l4_offset + 1];
                                entry.dstPort = (rawPayload[l4_offset + 2] << 8) | rawPayload[l4_offset + 3];
                            }
                        }
                    }
                }
            }
        }
    } else {
        if (src.indexOf(':') > 0) entry.srcMac = src;
        else entry.srcMac = "";
    }

    if (dstMac && strlen(dstMac) > 0) entry.dstMac = dstMac;
    else if (entry.dstMac.length() == 0) {
        if (dst.indexOf(':') > 0 || dst == "Broadcast") entry.dstMac = dst;
        else entry.dstMac = "";
    }

    if (rawPayload && rawLen > 0) {
        int dumpLen = (rawLen > 48) ? 48 : rawLen;
        char dumpBuf[512];
        int dPos = 0;
        for (int i = 0; i < dumpLen; i += 16) {
            if (i > 0) dumpBuf[dPos++] = '\n';
            dPos += snprintf(&dumpBuf[dPos], sizeof(dumpBuf) - dPos, "%04X  ", i);
            int chunkLen = ((dumpLen - i) > 16) ? 16 : (dumpLen - i);
            for (int j = 0; j < 16; j++) {
                if (j < chunkLen) {
                    dPos += snprintf(&dumpBuf[dPos], sizeof(dumpBuf) - dPos, "%02X ", rawPayload[i + j]);
                } else { memcpy(&dumpBuf[dPos], "   ", 3); dPos += 3; }
                if (j == 7) dumpBuf[dPos++] = ' ';
            }
            memcpy(&dumpBuf[dPos], " | ", 3); dPos += 3;
            for (int j = 0; j < chunkLen; j++) {
                char c = rawPayload[i + j];
                dumpBuf[dPos++] = (c >= 32 && c <= 126) ? c : '.';
            }
            dumpBuf[dPos] = '\0';
        }
        entry.rawHex = String(dumpBuf);
    } else {
        entry.rawHex = "";
    }

    s_packetLogs.push_back(entry);
    if (s_packetLogs.size() > 20) s_packetLogs.erase(s_packetLogs.begin());

    webServerBroadcastLog(proto, subtype, src, dst, rssi, len, info, entry.channel, entry.srcMac, entry.dstMac, entry.rawHex, entry.ttl, entry.srcPort, entry.dstPort);

    if (s_jsonSerialActive && !s_pcapSerialActive) {
        if (!Serial) { s_jsonSerialActive = false; return; }
        DynamicJsonDocument doc(1024);
        doc["type"] = "packet";
        JsonObject obj = doc.createNestedObject("data");
        obj["proto"] = proto;
        obj["subtype"] = subtype;
        obj["src"] = src;
        obj["dst"] = dst;
        obj["rssi"] = rssi;
        obj["len"] = len;
        obj["info"] = info;
        obj["channel"] = entry.channel;
        obj["time"] = entry.timestamp;
        if (entry.srcMac.length()) obj["srcMac"] = entry.srcMac;
        if (entry.dstMac.length()) obj["dstMac"] = entry.dstMac;
        if (entry.ttl >= 0) obj["ttl"] = entry.ttl;
        if (entry.srcPort >= 0) obj["srcPort"] = entry.srcPort;
        if (entry.dstPort >= 0) obj["dstPort"] = entry.dstPort;
        serializeJson(doc, Serial);
        Serial.println();
    } else if (!s_pcapSerialActive) {
        Serial.printf("[SNIFFER] [%s %s] %s -> %s | %s\n", proto.c_str(), subtype.c_str(), src.c_str(), dst.c_str(), info.c_str());
    }
}
