/**
 * wifi_sniffer_mgmt.cpp
 * 802.11 Management Frame handlers:
 *   Beacon (0x80), Probe Response (0x50), Probe Request (0x40),
 *   Assoc Request (0x00), Assoc Response (0x10),
 *   Authentication (0xB0), Deauthentication (0xC0), Disassociation (0xA0).
 */
#include "wifi_sniffer_common.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static inline void macToStr(char out[18], const uint8_t* m) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);
}

// ── Beacon / Probe Response ───────────────────────────────────────────────────

static void handleBeacon(uint8_t fc, const uint8_t* payload, int len, int rssi) {
    s_beaconCount++;
    // SSID tagged params start at offset 36 for Beacon / 36 for Probe Resp
    String ssid = parseSSID(payload, len, 36);
    if (ssid.length() == 0) return;

    const uint8_t* bssid = &payload[10]; // Source MAC
    BeaconInfo info = parseBeaconDetails(payload, len);
    if (!isRecentlySeen(bssid, true, rssi, ssid, info.channel, info.security, info.wifiGen, info.clients, info.utilization)) {
        const char* ftype = (fc == 0x80) ? "BEACON" : "PROBE RESP";
        String extra = "";
        if (info.clients >= 0) extra += " clients:" + String(info.clients) + " util:" + String(info.utilization) + "%";
        Serial.printf("[SNIFFER] [%s] SSID:%-20s BSSID:%02X:%02X:%02X:%02X:%02X:%02X ch:%d %s %s%s\n",
                      ftype, ssid.c_str(),
                      bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5],
                      info.channel, info.security.c_str(), info.wifiGen.c_str(), extra.c_str());
    }
}

// ── Probe Request ─────────────────────────────────────────────────────────────

static void handleProbeRequest(const uint8_t* payload, int len, int rssi) {
    s_probeReqCount++;
    String ssid = parseSSID(payload, len, 24);
    const uint8_t* cli = &payload[10];
    char cli_str[18]; macToStr(cli_str, cli);

    String sub      = ssid.length() ? ssid : "(wildcard)";
    String info_str = ssid.length()
        ? "Looking for: " + ssid + "  ch=" + String(s_currentChannel)
        : "Wildcard scan  ch=" + String(s_currentChannel);
    addPacketLog("PROBE REQ", sub, cli_str, "Broadcast", rssi, len, info_str, nullptr, nullptr, payload, len);

    if (!isRecentlySeen(cli, false, rssi, ssid)) {
        Serial.printf("[SNIFFER] [PROBE REQ] %s rssi=%d ssid:%s\n",
                      cli_str, rssi, ssid.length() ? ssid.c_str() : "(wildcard)");
    }
}

// ── Association / Reassociation Request ───────────────────────────────────────

static void handleAssocRequest(uint8_t fc, const uint8_t* payload, int len, int rssi) {
    bool isReassoc = (fc == 0x20);
    int ssid_off = isReassoc ? 34 : 28;
    String ssid = parseSSID(payload, len, ssid_off);
    const uint8_t* cli = &payload[10];
    const uint8_t* ap  = &payload[4];
    char cli_str[18], ap_str[18];
    macToStr(cli_str, cli);
    macToStr(ap_str,  ap);

    const char* type_s = isReassoc ? "REASSOC REQ" : "ASSOC REQ";
    String info_str = String(type_s) + " → AP:" + String(ap_str)
                    + "  SSID:" + (ssid.length() ? ssid : "?");
    addPacketLog(type_s, "Req", cli_str, ap_str, rssi, len, info_str, nullptr, nullptr, payload, len);

    if (!isRecentlySeen(cli, false, rssi, ssid)) {
        Serial.printf("[SNIFFER] [%s] %s -> %s ssid:%s\n",
                      type_s, cli_str, ap_str, ssid.length() ? ssid.c_str() : "?");
    }
}

// ── Association / Reassociation Response ──────────────────────────────────────

static void handleAssocResponse(uint8_t fc, const uint8_t* payload, int len, int rssi) {
    if (len < 30) return;
    const uint8_t* cli = &payload[4];
    const uint8_t* ap  = &payload[10];
    uint16_t status    = payload[26] | (payload[27] << 8);
    char cli_str[18], ap_str[18];
    macToStr(cli_str, cli);
    macToStr(ap_str,  ap);

    bool isReassoc = (fc == 0x30);
    const char* type_s = isReassoc ? "REASSOC RESP" : "ASSOC RESP";
    String outcome  = (status == 0) ? "Success" : "Denied(" + String(status) + ")";
    String info_str = String(type_s) + ": " + outcome + "  AP:" + ap_str;
    addPacketLog(type_s, outcome, ap_str, cli_str, rssi, len, info_str, nullptr, nullptr, payload, len);
}

// ── Authentication ────────────────────────────────────────────────────────────

static void handleAuth(const uint8_t* payload, int len, int rssi) {
    if (len < 30) return;
    const uint8_t* src  = &payload[10];
    const uint8_t* dst  = &payload[4];
    uint16_t algo   = payload[24] | (payload[25] << 8);
    uint16_t seq    = payload[26] | (payload[27] << 8);
    uint16_t status = payload[28] | (payload[29] << 8);
    char src_str[18], dst_str[18];
    macToStr(src_str, src);
    macToStr(dst_str, dst);

    const char* algo_s = (algo==0) ? "Open" : (algo==1) ? "SharedKey"
                       : (algo==3) ? "SAE(WPA3)" : "?";
    String outcome  = (status == 0) ? "OK" : "FAIL(" + String(status) + ")";
    String info_str = "Auth " + String(algo_s) + " seq=" + String(seq) + " " + outcome;
    addPacketLog("AUTH", outcome, src_str, dst_str, rssi, len, info_str, nullptr, nullptr, payload, len);
}

// ── Deauthentication / Disassociation ─────────────────────────────────────────

static void handleDeauth(uint8_t fc, const uint8_t* payload, int len, int rssi) {
    s_deauthCount++;
    s_rollingDeauthCount++;
    if (len < 26) return;

    bool isProtected = (payload[1] & 0x40) != 0; // Check Protected flag in Frame Control

    const uint8_t* src   = &payload[10];
    const uint8_t* dst   = &payload[4];
    const uint8_t* bssid = &payload[16];
    uint16_t reason = payload[24] | (payload[25] << 8);
    String reason_str = getDeauthReasonStr(reason);

    char src_str[18], dst_str[18], bssid_str[18];
    macToStr(src_str,   src);
    macToStr(dst_str,   dst);
    macToStr(bssid_str, bssid);

    const char* type_s = (fc == 0xC0) ? "DEAUTH" : "DISASSOC";
    String info_str = String(type_s) + " reason:" + reason_str
                    + "  bssid=" + bssid_str;
                    
    if (isProtected) {
        info_str += " [PMF/Encrypted]";
        reason_str = "PMF_" + reason_str;
    } else {
        info_str += " [Unprotected/Spoofable]";
    }

    addPacketLog(type_s, reason_str, src_str, dst_str, rssi, len, info_str, nullptr, nullptr, payload, len);
    Serial.printf("[SNIFFER] [%s] ch=%d rssi=%d %s->%s reason:%s\n",
                  type_s, s_currentChannel, rssi, src_str, dst_str, reason_str.c_str());
}

// ── Main Dispatcher ───────────────────────────────────────────────────────────

void dispatchMgmtFrame(uint8_t fc, const uint8_t* payload, int len, int rssi) {
    switch (fc) {
        case 0x80: // Beacon
        case 0x50: // Probe Response
            handleBeacon(fc, payload, len, rssi);
            break;
        case 0x40: // Probe Request
            handleProbeRequest(payload, len, rssi);
            break;
        case 0x00: // Association Request
        case 0x20: // Reassociation Request
            handleAssocRequest(fc, payload, len, rssi);
            break;
        case 0x10: // Association Response
        case 0x30: // Reassociation Response
            handleAssocResponse(fc, payload, len, rssi);
            break;
        case 0xB0: // Authentication
            handleAuth(payload, len, rssi);
            break;
        case 0xC0: // Deauthentication
        case 0xA0: // Disassociation
            handleDeauth(fc, payload, len, rssi);
            break;
        default:
            s_otherCount++;
            break;
    }
}
