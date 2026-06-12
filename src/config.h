#pragma once

// ============================================================
//  ESP32-S3 CPU Fan Controller – Central Configuration
//  Edit this file to match your hardware wiring.
// ============================================================

// ------------------------------------------------------------
// WiFi Access Point – ESP32-S3 tự phát WiFi riêng
// ------------------------------------------------------------
#define WIFI_AP_SSID  "ESP32-Fan"   // Tên WiFi phát ra
#define WIFI_AP_PASS  "12345678"    // Mật khẩu (tối thiểu 8 ký tự)
#define WIFI_AP_IP    "192.168.4.1" // Địa chỉ IP khi dùng qua AP
#define WIFI_HOSTNAME "esp32-fan"

// ------------------------------------------------------------
// WiFi Station – Cấu hình qua giao diện Web
// Mật khẩu và SSID được lưu trữ trong NVS (Preferences)
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
// Dùng IDF driver: driver/temperature_sensor.h
// Không cần GPIO hay linh kiện ngoài.
// Đo nhiệt độ chip (±2°C, phản ánh tải xử lý của ESP32-S3).
// ------------------------------------------------------------
#define TEMP_READ_INTERVAL_MS 2000   // Đọc nhiệt độ mỗi 2 giây
// Offset hiệu chỉnh (°C) – chỉnh nếu đọc lệch so với thực tế
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
