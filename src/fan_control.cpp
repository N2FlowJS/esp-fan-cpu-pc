#include "fan_control.h"
#include "driver/temp_sensor.h"
#include <esp_arduino_version.h>
#include <Preferences.h>

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  #define USE_LEDC_V3 1
#else
  #define USE_LEDC_V3 0
  #define FAN_PWM_CHANNEL 0
#endif

// ============================================================
//  Fan Control Implementation
// ============================================================

// --- Internal state ------------------------------------------
static volatile uint32_t s_tachCount   = 0;   // ISR pulse counter
static uint32_t          s_lastRpmTime = 0;
static uint32_t          s_rpm         = 0;
static uint8_t           s_speedPct    = 0;
static float             s_tempC       = 25.0f;
static FanMode           s_mode        = FanMode::MANUAL;
static uint32_t          s_lastTempTime = 0;

// portMUX spinlock to protect tachCount access across cores
static portMUX_TYPE      s_tachMux = portMUX_INITIALIZER_UNLOCKED;


// ============================================================
//  Tachometer ISR  (IRAM-safe)
// ============================================================
static void IRAM_ATTR tachISR() {
    // Use explicit assignment to avoid C++20 volatile ++ deprecation warning
    s_tachCount = s_tachCount + 1;
}

// ============================================================
//  Helpers
// ============================================================

/**
 * @brief Convert speed percent (0-100) to LEDC duty (0-255).
 *        Enforces minimum duty when speed > 0 so the fan
 *        doesn't stall at very low duty cycles.
 */
static uint32_t pctToDuty(uint8_t pct) {
    if (pct == 0) return 0;
    // Map [1, 100] → [FAN_MIN_DUTY, 100] then scale to 255
    float mapped = FAN_MIN_DUTY + (100.0f - FAN_MIN_DUTY) * (pct / 100.0f);
    return static_cast<uint32_t>((mapped / 100.0f) * 255.0f);
}

/**
 * @brief Interpolate fan curve to get target speed % for a given temperature.
 */
static uint8_t tempToSpeed(float tempC) {
    // Below first point
    if (tempC <= FAN_CURVE[0].tempC) return FAN_CURVE[0].speedPct;

    for (size_t i = 1; i < FAN_CURVE_LEN; i++) {
        if (tempC <= FAN_CURVE[i].tempC) {
            // Linear interpolation between i-1 and i
            float t = (tempC - FAN_CURVE[i-1].tempC) /
                      (FAN_CURVE[i].tempC - FAN_CURVE[i-1].tempC);
            float speed = FAN_CURVE[i-1].speedPct +
                          t * (FAN_CURVE[i].speedPct - FAN_CURVE[i-1].speedPct);
            return static_cast<uint8_t>(speed);
        }
    }
    // Above last point → max speed
    return FAN_CURVE[FAN_CURVE_LEN - 1].speedPct;
}

// ============================================================
//  Public API
// ============================================================

void fanSetup() {
    Serial.println("[FAN] Initialising...");

    // --- Load persistent settings ---
    Preferences prefs;
    prefs.begin("fan_settings", false); // read-write (creates if not exists)
    uint8_t savedMode = prefs.getUChar("mode", static_cast<uint8_t>(FanMode::AUTO)); // default to AUTO for safety
    uint8_t savedSpeed = prefs.getUChar("speed", 30);
    prefs.end();
    
    s_mode = static_cast<FanMode>(savedMode);
    s_speedPct = savedSpeed;
    
    Serial.printf("[FAN] Loaded settings - Mode: %s, Speed: %d%%\n", 
                  s_mode == FanMode::AUTO ? "AUTO" : "MANUAL", s_speedPct);

    // --- PWM output (Compatible with both Arduino Core 2.x and 3.x) ---
#if USE_LEDC_V3
    ledcAttach(FAN_PWM_PIN, FAN_PWM_FREQ_HZ, FAN_PWM_RESOLUTION);
    ledcWrite(FAN_PWM_PIN, pctToDuty(s_speedPct));
#else
    ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ_HZ, FAN_PWM_RESOLUTION);
    ledcAttachPin(FAN_PWM_PIN, FAN_PWM_CHANNEL);
    ledcWrite(FAN_PWM_CHANNEL, pctToDuty(s_speedPct));
#endif

    // --- Tachometer input ---
    pinMode(FAN_TACH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), tachISR, FALLING);

    // --- Internal temperature sensor (ESP32-S3 legacy IDF driver) ---
    temp_sensor_config_t tempCfg = TSENS_CONFIG_DEFAULT();
    temp_sensor_set_config(tempCfg);
    temp_sensor_start();
    temp_sensor_read_celsius(&s_tempC);
    s_tempC += TEMP_OFFSET_C;
    s_lastTempTime = millis();
    s_lastRpmTime  = millis();

    Serial.printf("[FAN] Internal temp: %.1f°C\n", s_tempC);
    Serial.println("[FAN] Ready.");
}

void fanLoop() {
    uint32_t now = millis();

    // --- RPM update ---
    if (now - s_lastRpmTime >= RPM_UPDATE_INTERVAL_MS) {
        uint32_t elapsed = now - s_lastRpmTime;
        s_lastRpmTime    = now;

        // Use ESP32 spinlock critical section to safely read/write s_tachCount across cores
        portENTER_CRITICAL(&s_tachMux);
        uint32_t count = s_tachCount;
        s_tachCount = 0;
        portEXIT_CRITICAL(&s_tachMux);

        // RPM = (pulses / PPR) * (60000 / elapsed_ms)
        s_rpm = (count * 60000UL) / (FAN_TACH_PPR * elapsed);

#if USE_LEDC_V3
        Serial.printf("[FAN] RPM: %lu | Temp: %.2f°C | Speed: %d%%\n",
                      s_rpm, s_tempC, s_speedPct);
#else
        Serial.printf("[FAN] RPM: %lu | Temp: %.2f°C | Speed: %d%% | LEDC Freq: %lu Hz | LEDC Duty: %lu\n",
                      s_rpm, s_tempC, s_speedPct, ledcReadFreq(FAN_PWM_CHANNEL), ledcRead(FAN_PWM_CHANNEL));
#endif
    }

    // --- Temperature update ---
    if (now - s_lastTempTime >= TEMP_READ_INTERVAL_MS) {
        s_lastTempTime = now;

        // Read ESP32-S3 internal temperature sensor (legacy driver)
        float raw = 0.0f;
        temp_sensor_read_celsius(&raw);
        s_tempC = raw + TEMP_OFFSET_C;

        // --- Auto mode ---
        if (s_mode == FanMode::AUTO) {
            uint8_t target = tempToSpeed(s_tempC);
            if (target != s_speedPct) {
                fanSetSpeed(target);
            }
        }
    }
}

void fanSetSpeed(uint8_t percent) {
    s_speedPct = constrain(percent, 0, 100);
    
    // Only save manual speed to Flash when in MANUAL mode
    if (s_mode == FanMode::MANUAL) {
        Preferences prefs;
        prefs.begin("fan_settings", false); // read-write
        prefs.putUChar("speed", s_speedPct);
        prefs.end();
    }

#if USE_LEDC_V3
    ledcWrite(FAN_PWM_PIN, pctToDuty(s_speedPct));  // core 3.x: use pin, not channel
#else
    ledcWrite(FAN_PWM_CHANNEL, pctToDuty(s_speedPct));
#endif
}

void fanSetMode(FanMode mode) {
    if (s_mode != mode) {
        s_mode = mode;
        Preferences prefs;
        prefs.begin("fan_settings", false); // read-write
        prefs.putUChar("mode", static_cast<uint8_t>(s_mode));
        prefs.end();
        Serial.printf("[FAN] Mode saved: %s\n", s_mode == FanMode::AUTO ? "AUTO" : "MANUAL");
    }
}

uint8_t   fanGetSpeedPct() { return s_speedPct; }
uint32_t  fanGetRPM()      { return s_rpm; }
float     fanGetTemp()     { return s_tempC; }
FanMode   fanGetMode()     { return s_mode; }
