#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Initialize filter subsystem (loads whitelist/blacklist from Preferences)
void snifferInitFilters();

// Returns true if the MAC is filtered (blacklisted or not whitelisted)
bool isMacFiltered(const uint8_t* mac);

// Returns true if the MAC is one of the owner/excluded MACs
bool isOwnerMac(const uint8_t* mac);

// Exposed via wifi_sniffer.h: snifferGetFilterConfig / snifferSetFilterConfig / snifferAddOwnerMac
