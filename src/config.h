#pragma once

// ============================================================
//  ESP32-S3 CPU Fan Controller – Central Configuration
//  Edit this file to match your hardware wiring.
// ============================================================

// ------------------------------------------------------------
// WiFi Access Point – ESP32-S3 creates its own hotspot
// ------------------------------------------------------------
#define WIFI_AP_SSID  "ESP32-Fan"   // Broadcasted SSID
#define WIFI_AP_PASS  "12345678"    // Password (min 8 characters)
#define WIFI_AP_IP    "192.168.4.1" // IP address when using AP
#define WIFI_HOSTNAME "esp32-fan"

// ------------------------------------------------------------
// WiFi Station – Configured via Web interface
// Password and SSID are stored in NVS (Preferences)
// ------------------------------------------------------------

// ------------------------------------------------------------
// Fan PWM Output (4-pin PC fan – Intel spec: 25 kHz)
// ------------------------------------------------------------
#define FAN_PWM_PIN        4      // GPIO pin for PWM signal (Fan Pin 4) – ESP32-S3
#define FAN_PWM_FREQ_HZ    25000  // 25 kHz – Intel 4-pin standard
#define FAN_PWM_RESOLUTION 8      // 8-bit → duty 0-255
#define FAN_MIN_DUTY       30     // Minimum duty % to keep fan alive (0 = fully off)

// ------------------------------------------------------------
// Fan Tachometer Input (open-drain, needs 10k pull-up to 3.3V)
// Fan Pin 3 — 2 pulses per revolution
// ------------------------------------------------------------
#define FAN_TACH_PIN       5      // GPIO pin (interrupt-capable) – ESP32-S3
#define FAN_TACH_PPR       2      // Pulses per revolution (standard PC fan)
#define RPM_UPDATE_INTERVAL_MS 1000  // Recalculate RPM every 1 second

// ------------------------------------------------------------
// Internal ESP32-S3 Temperature Sensor
// Uses IDF driver: driver/temperature_sensor.h
// No GPIO or external components required.
// Measures chip temperature (±2°C, reflects ESP32-S3 processing load).
// ------------------------------------------------------------
#define TEMP_READ_INTERVAL_MS 2000   // Read temperature every 2 seconds
// Calibration offset (°C) – adjust if reading differs from reality
#define TEMP_OFFSET_C      0.0f

// ------------------------------------------------------------
// Auto Fan Curve (Temperature → Speed %)
// Lookup table – linear interpolation between points
// ------------------------------------------------------------
struct TempSpeedPoint {
    float  tempC;
    uint8_t speedPct;
};

// Fan curve: [temp °C] → [speed %]
constexpr TempSpeedPoint FAN_CURVE[] = {
    {30.0f,  20},   // Below 30°C → 20% (silent idle)
    {40.0f,  35},
    {50.0f,  55},
    {60.0f,  75},
    {70.0f,  90},
    {80.0f, 100},   // Above 80°C → 100%
};
constexpr size_t FAN_CURVE_LEN = sizeof(FAN_CURVE) / sizeof(FAN_CURVE[0]);

// ------------------------------------------------------------
// Web Server
// ------------------------------------------------------------
#define WEB_SERVER_PORT    80
