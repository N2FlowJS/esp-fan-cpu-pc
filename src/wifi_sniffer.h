#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Initialize sniffer state (e.g. load MAC filters).
 */
void snifferSetup();

/**
 * @brief Check if the Sniffer is currently active.
 */
bool snifferIsActive();

/**
 * @brief Check if the Sniffer is in channel hopping mode.
 */
bool snifferIsHopping();

/**
 * @brief Get the current scanning channel.
 */
uint8_t snifferGetChannel();

/**
 * @brief Start Sniffer mode.
 *        @param channel Start channel (1-13). If = 0 and concurrent = false, it will auto-hop channels.
 *        @param concurrent If true, sniff concurrently on the current Wi-Fi channel without interrupting the Web Server.
 */
void snifferStart(uint8_t channel = 0, bool concurrent = false);

/**
 * @brief Stop Sniffer mode.
 */
void snifferStop();

/**
 * @brief Switch Wi-Fi channel (1-13).
 */
void snifferSetChannel(uint8_t channel);

/**
 * @brief Print statistics of scanned packets and detected devices to Serial.
 */
void snifferPrintStats();

/**
 * @brief Fill statistics data into a JSON Document.
 */
void snifferGetStatsJson(JsonDocument& doc, bool includeLogs = true);

/**
 * @brief Get pending device updates as a JSON string.
 */
String snifferGetPendingDevicesJson();

/**
 * @brief Get Whitelist/Blacklist as a JSON object.
 */
void snifferGetFilterConfig(JsonDocument& doc);

/**
 * @brief Update Whitelist/Blacklist.
 * @param whitelist String containing comma or newline separated MACs.
 * @param blacklist String containing comma or newline separated MACs.
 */
void snifferSetFilterConfig(const String& whitelist, const String& blacklist);

/**
 * @brief Add a temporary MAC address to the exclusion list (e.g., MAC of the connecting browser).
 * These MACs will be removed from the log stream to avoid noise.
 * @param mac MAC string in "XX:XX:XX:XX:XX:XX" format.
 */
void snifferAddOwnerMac(const String& mac);

/**
 * @brief Enable/disable Raw Binary PCAP Streaming mode over Serial.
 * When enabled, text logs are disabled to avoid corrupting the PCAP file structure.
 */
void snifferSetPcapSerial(bool enable);

/**
 * @brief Enable/disable JSON data transmission over Serial for database storage.
 */
void snifferSetJsonSerial(bool enable);

/**
 * @brief Check if PCAP serial transmission mode is enabled.
 */
bool snifferIsPcapSerialActive();

/**
 * @brief Call this function in the main loop().
 *        Performs automatic channel hopping if enabled.
 */
void snifferLoop();
