#include "sniffer_filters.h"
#include <vector>
#include <array>
#include <algorithm>
#include <Preferences.h>
#include "wifi_sniffer.h"

typedef std::array<uint8_t, 6> MacAddr;

static bool s_macsInitialized = false;
static std::vector<MacAddr> s_ownerMacs;
static std::vector<MacAddr> s_whitelist;
static std::vector<MacAddr> s_blacklist;

static MacAddr macStringToBytes(const String& mac) {
    MacAddr bytes = {0,0,0,0,0,0};
    String m = mac;
    m.replace(":", "");
    m.replace("-", "");
    if (m.length() != 12) return bytes;
    for (int i = 0; i < 6; i++) {
        String b = m.substring(i * 2, i * 2 + 2);
        bytes[i] = (uint8_t)strtol(b.c_str(), NULL, 16);
    }
    return bytes;
}

static String bytesToMacString(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

static void loadFilterConfig() {
    Preferences prefs;
    prefs.begin("sniffer", true);
    String wl = prefs.getString("whitelist", "");
    String bl = prefs.getString("blacklist", "");
    prefs.end();

    s_whitelist.clear();
    s_blacklist.clear();

    auto parseMACs = [](const String& input, std::vector<MacAddr>& target) {
        int start = 0;
        int end = input.indexOf(',');
        while (end != -1) {
            String mac = input.substring(start, end);
            mac.trim();
            if (mac.length() > 0) target.push_back(macStringToBytes(mac));
            start = end + 1;
            end = input.indexOf(',', start);
        }
        String mac = input.substring(start);
        mac.trim();
        if (mac.length() > 0) target.push_back(macStringToBytes(mac));
    };

    parseMACs(wl, s_whitelist);
    parseMACs(bl, s_blacklist);
}

bool isMacFiltered(const uint8_t* mac) {
    for (const auto& m : s_blacklist) {
        if (memcmp(m.data(), mac, 6) == 0) return true;
    }

    if (!s_whitelist.empty()) {
        bool found = false;
        for (const auto& m : s_whitelist) {
            if (memcmp(m.data(), mac, 6) == 0) { found = true; break; }
        }
        if (!found) return true;
    }
    return false;
}

void snifferInitFilters() {
    if (s_macsInitialized) return;
    loadFilterConfig();
    s_macsInitialized = true;
}

void snifferGetFilterConfig(JsonDocument& doc) {
    std::vector<String> wl, bl;
    for (const auto& m : s_whitelist) wl.push_back(bytesToMacString(m.data()));
    for (const auto& m : s_blacklist) bl.push_back(bytesToMacString(m.data()));

    JsonArray arrW = doc["whitelist"].to<JsonArray>();
    for (const auto& s : wl) arrW.add(s);
    JsonArray arrB = doc["blacklist"].to<JsonArray>();
    for (const auto& s : bl) arrB.add(s);
}

void snifferSetFilterConfig(const String& whitelist, const String& blacklist) {
    Preferences prefs;
    prefs.begin("sniffer", false);
    prefs.putString("whitelist", whitelist);
    prefs.putString("blacklist", blacklist);
    prefs.end();
    loadFilterConfig();
}

void snifferAddOwnerMac(const String& mac) {
    MacAddr b = macStringToBytes(mac);
    if (memcmp(b.data(), (const uint8_t[]){0,0,0,0,0,0}, 6) == 0) return;
    // prevent duplicates
    for (const auto& m : s_ownerMacs) if (memcmp(m.data(), b.data(), 6) == 0) return;
    s_ownerMacs.push_back(b);
}

bool isOwnerMac(const uint8_t* mac) {
    for (const auto& m : s_ownerMacs) {
        if (memcmp(m.data(), mac, 6) == 0) return true;
    }
    return false;
}
