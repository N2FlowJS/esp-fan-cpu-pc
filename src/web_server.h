#pragma once

#include <Arduino.h>

// ============================================================
//  Web Server Module
//  – Serves dashboard from SPIFFS (/data/index.html)
//  – REST API for status, speed, mode
// ============================================================

/**
 * @brief Initialise ESPAsyncWebServer and mount SPIFFS.
 *        Call once in setup() after WiFi is connected.
 */
void webServerSetup();

/**
 * @brief No-op (async server runs in background).
 *        Kept for symmetry with fanLoop().
 */
void webServerLoop();

void webServerBroadcast(const char* event, const char* data);
void webServerBroadcastLog(const String& proto, const String& subtype, const String& src, const String& dst, int rssi, int len, const String& info, uint8_t channel, const String& srcMac = "", const String& dstMac = "", const String& rawHex = "", int ttl = -1, int srcPort = -1, int dstPort = -1);
void webServerBroadcastDevice(const uint8_t* mac, bool isAP, int rssi, const String& ssid, uint8_t channel, const String& security, const String& wifiGen, int clients, int util, const String& vendor, uint32_t packetCount, uint32_t lastSeen);
void webServerInvalidateSession();

