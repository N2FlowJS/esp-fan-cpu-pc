#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
//  Fan Control Module
//  – PWM output via ESP32 LEDC peripheral (25 kHz, 8-bit)
//  – RPM measurement via tachometer interrupt
//  – Temperature-based auto mode
// ============================================================

enum class FanMode : uint8_t {
    MANUAL = 0,
    AUTO   = 1
};

// --- Public API -----------------------------------------------

/**
 * @brief Initialise PWM output and tachometer interrupt.
 *        Call once in setup().
 */
void fanSetup();

/**
 * @brief Must be called every loop() iteration.
 *        Updates RPM counter and auto-control logic.
 */
void fanLoop();

/**
 * @brief Set fan speed (0-100 %).
 *        Automatically clamps to FAN_MIN_DUTY when > 0.
 */
void fanSetSpeed(uint8_t percent);

/**
 * @brief Set operating mode (MANUAL / AUTO).
 */
void fanSetMode(FanMode mode);

/**
 * @brief Update external host (PC) temperature.
 *        If updated recently, it will take precedence over the internal sensor.
 */
void fanSetHostTemp(float temp);

/**
 * @brief Get the current fan curve points.
 * @param outPoints Pointer to array to fill.
 * @param maxLen Maximum points to read.
 * @return Number of points actually read.
 */
size_t fanGetCurve(TempSpeedPoint* outPoints, size_t maxLen);

/**
 * @brief Set the fan curve points and save to NVS.
 * @param points Array of new points.
 * @param len Number of points.
 */
void fanSetCurve(const TempSpeedPoint* points, size_t len);

// --- Getters --------------------------------------------------

uint8_t   fanGetSpeedPct();   // Current speed setting in %
uint32_t  fanGetRPM();         // Last measured RPM
float     fanGetTemp();         // Last measured temperature (°C)
FanMode   fanGetMode();         // Current mode
bool      fanIsHostTempActive(); // True if host temp is currently driving the fan
bool      fanHasHealthAlert();   // True if a fan stall/failure is detected
