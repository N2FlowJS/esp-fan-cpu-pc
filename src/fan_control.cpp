#include "fan_control.h"
#include "driver/temperature_sensor.h"
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
static float             s_hostTemp    = -1.0f;
static uint32_t          s_lastHostTempTime = 0;
static bool              s_fanHealthAlert   = false;
static uint32_t          s_stallStartTime   = 0;
static FanMode           s_mode        = FanMode::MANUAL;
static uint32_t          s_lastTempTime = 0;
static temperature_sensor_handle_t s_tempHandle = NULL;

// Fan curve mutable storage
static TempSpeedPoint s_fanCurve[MAX_FAN_CURVE_POINTS];
static size_t         s_fanCurveLen = 0;

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
    if (s_fanCurveLen == 0) return 50; // Fallback if no curve defined

    // Below first point
    if (tempC <= s_fanCurve[0].tempC) return s_fanCurve[0].speedPct;

    for (size_t i = 1; i < s_fanCurveLen; i++) {
        if (tempC <= s_fanCurve[i].tempC) {
            // Linear interpolation between i-1 and i
            float t = (tempC - s_fanCurve[i-1].tempC) /
                      (s_fanCurve[i].tempC - s_fanCurve[i-1].tempC);
            float speed = s_fanCurve[i-1].speedPct +
                          t * (s_fanCurve[i].speedPct - s_fanCurve[i-1].speedPct);
            return static_cast<uint8_t>(speed);
        }
    }
    // Above last point → max speed
    return s_fanCurve[s_fanCurveLen - 1].speedPct;
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
    
    // Load fan curve from NVS
    size_t len = prefs.getBytes("curve", s_fanCurve, sizeof(s_fanCurve));
    if (len > 0 && (len % sizeof(TempSpeedPoint) == 0)) {
        s_fanCurveLen = len / sizeof(TempSpeedPoint);
        Serial.printf("[FAN] Loaded %d curve points from NVS\n", s_fanCurveLen);
    } else {
        // Use defaults from config.h
        s_fanCurveLen = (FAN_CURVE_LEN > MAX_FAN_CURVE_POINTS) ? MAX_FAN_CURVE_POINTS : FAN_CURVE_LEN;
        memcpy(s_fanCurve, FAN_CURVE, s_fanCurveLen * sizeof(TempSpeedPoint));
        Serial.println("[FAN] Using default curve from config.h");
    }
    
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

    // --- Internal temperature sensor (Modern ESP-IDF driver) ---
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    temperature_sensor_install(&temp_sensor_config, &s_tempHandle);
    temperature_sensor_enable(s_tempHandle);
    temperature_sensor_get_celsius(s_tempHandle, &s_tempC);
    s_tempC += TEMP_OFFSET_C;
    s_lastTempTime = millis();
    s_lastRpmTime  = millis();

    Serial.printf("[FAN] Internal temp: %.1f°C\n", s_tempC);
    Serial.println("[FAN] Ready.");
}

void fanLoop() {
    uint32_t now = millis();

    // --- Tachometer update ---
    if (now - s_lastRpmTime >= RPM_UPDATE_INTERVAL_MS) {
        uint32_t elapsed = now - s_lastRpmTime;
        s_lastRpmTime    = now;

        // Use ESP32 spinlock critical section to safely read/write s_tachCount across cores
        portENTER_CRITICAL(&s_tachMux);
        uint32_t count = s_tachCount;
        s_tachCount = 0;
        portEXIT_CRITICAL(&s_tachMux);

        // RPM = (pulses / PPR) * (60000 / elapsed_ms)
        if (count > 0) {
            s_rpm = (count * 60000UL) / (FAN_TACH_PPR * elapsed);
        } else {
            s_rpm = 0;
        }

        // --- Fan Health Monitoring (Stall Detection) ---
        // If speed > 0 (meaning duty > 0) and RPM is 0 for > 5 seconds, alert.
        if (s_speedPct > 0 && s_rpm == 0) {
            if (s_stallStartTime == 0) {
                s_stallStartTime = now;
            } else if (now - s_stallStartTime > 5000) {
                if (!s_fanHealthAlert) {
                    Serial.println("[ERR] FAN STALL DETECTED!");
                    s_fanHealthAlert = true;
                }
            }
        } else {
            s_stallStartTime = 0;
            if (s_fanHealthAlert) {
                Serial.println("[OK] Fan recovered.");
                s_fanHealthAlert = false;
            }
        }

#if USE_LEDC_V3
        Serial.printf("[FAN] RPM: %lu (Pulses: %lu) | Temp: %.2f°C | Speed: %d%%\n",
                      s_rpm, count, s_tempC, s_speedPct);
#else
        Serial.printf("[FAN] RPM: %lu (Pulses: %lu) | Temp: %.2f°C | Speed: %d%% | Duty: %lu\n",
                      s_rpm, count, s_tempC, s_speedPct, ledcRead(FAN_PWM_CHANNEL));
#endif
    }

    // --- Temperature update ---
    if (now - s_lastTempTime >= TEMP_READ_INTERVAL_MS) {
        s_lastTempTime = now;

        // Read ESP32-S3 internal temperature sensor (Modern driver)
        float raw = 0.0f;
        temperature_sensor_get_celsius(s_tempHandle, &raw);
        s_tempC = raw + TEMP_OFFSET_C;

        // --- Auto mode ---
        if (s_mode == FanMode::AUTO) {
            float activeTemp = fanGetTemp(); // Uses priority logic
            uint8_t target = tempToSpeed(activeTemp);
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

void fanSetHostTemp(float temp) {
    s_hostTemp = temp;
    s_lastHostTempTime = millis();
}

size_t fanGetCurve(TempSpeedPoint* outPoints, size_t maxLen) {
    size_t count = (s_fanCurveLen > maxLen) ? maxLen : s_fanCurveLen;
    if (outPoints && count > 0) {
        memcpy(outPoints, s_fanCurve, count * sizeof(TempSpeedPoint));
    }
    return count;
}

void fanSetCurve(const TempSpeedPoint* points, size_t len) {
    if (!points || len == 0) return;
    s_fanCurveLen = (len > MAX_FAN_CURVE_POINTS) ? MAX_FAN_CURVE_POINTS : len;
    memcpy(s_fanCurve, points, s_fanCurveLen * sizeof(TempSpeedPoint));
    
    Preferences prefs;
    prefs.begin("fan_settings", false);
    prefs.putBytes("curve", s_fanCurve, s_fanCurveLen * sizeof(TempSpeedPoint));
    prefs.end();
    
    Serial.printf("[FAN] New curve saved (%d points)\n", s_fanCurveLen);
}

uint8_t   fanGetSpeedPct() { return s_speedPct; }
uint32_t  fanGetRPM()      { return s_rpm; }

float     fanGetTemp() {
    // If host temperature was updated in the last 10 seconds, use it.
    if (s_lastHostTempTime > 0 && (millis() - s_lastHostTempTime < 10000)) {
        return s_hostTemp;
    }
    return s_tempC; 
}

FanMode   fanGetMode()         { return s_mode; }
bool      fanIsHostTempActive() { return (s_lastHostTempTime > 0 && (millis() - s_lastHostTempTime < 10000)); }
bool      fanHasHealthAlert()   { return s_fanHealthAlert; }
