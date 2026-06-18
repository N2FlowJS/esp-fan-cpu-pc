#pragma once
#include <Arduino.h>

enum StressTestType {
    STRESS_TYPE_DEAUTH,
    STRESS_TYPE_BEACON_FLOOD,
    STRESS_TYPE_PROBE_FLOOD
};

struct StressTestConfig {
    StressTestType type;
    String targetMac;   // Target BSSID for Deauth
    String clientMac;   // Target Client for Deauth (FF:FF:FF:FF:FF:FF for all)
    uint8_t channel;
    uint16_t rate;      // Packets per second
    bool active;
};

/**
 * @brief Initialize stress test module.
 */
void stressTestSetup();

/**
 * @brief Start a stress test.
 */
void stressTestStart(StressTestConfig config);

/**
 * @brief Stop current stress test.
 */
void stressTestStop();

/**
 * @brief Get current status.
 */
bool stressTestIsActive();

/**
 * @brief Task loop for stress testing.
 */
void stressTestLoop();
