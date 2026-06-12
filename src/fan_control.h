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

// --- Getters --------------------------------------------------

uint8_t   fanGetSpeedPct();   // Current speed setting in %
uint32_t  fanGetRPM();         // Last measured RPM
float     fanGetTemp();         // Last measured temperature (°C)
FanMode   fanGetMode();         // Current mode
