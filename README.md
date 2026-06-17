# ESP32 CPU Fan Controller

> Control Intel-standard 4-pin PC CPU fans using an ESP32 via 25 kHz PWM, read RPM via Tach, measure temperature from the **ESP32 internal chip sensor**, and monitor via a dark-mode web dashboard.

---

## Table of Contents

1. [Project Structure](#project-structure)
2. [Wiring Diagram](#wiring-diagram)
3. [Setup & Build](#setup--build)
4. [REST API](#rest-api)
5. [Fan Curve – Auto Mode](#fan-curve--auto-mode)
6. [Libraries Used](#libraries-used)
7. [Technical Notes](#technical-notes)

---

## Project Structure

```
esp32-fan-cpu/
├── platformio.ini          ← PlatformIO configuration (board, libs, filesystem)
├── scripts/
│   └── build_flags.py      ← Injects git commit hash into the build
├── src/
│   ├── config.h            ← All configurations (WiFi, GPIO, fan curve)
│   ├── main.cpp            ← Entry point (setup + loop)
│   ├── fan_control.h/cpp   ← PWM + RPM (ISR) + internal temperature + Auto mode
│   └── web_server.h/cpp    ← ESPAsyncWebServer + REST API
├── data/
│   ├── index.html          ← Web dashboard (dark glassmorphism)
│   └── style.css           ← Stylesheet
└── README.md
```

---

## Wiring Diagram

### Identifying the 4-pin Fan Connector

```
╔═══════════════════════╗
║  [1] [2] [3] [4]      ║  ← Latch side
╚═══════════════════════╝
   │    │    │    │
  GND  12V  RPM  PWM
 (Black)(Yellow)(Green)(White/Green)
```

> ⚠️ Wire colors may vary by manufacturer — **pin numbering** is the most reliable method.

---

### Connection Table

| Fan Pin | Name | Common Color | Connect to |
|:-------:|------|:------------:|------------|
| **Pin 1** | GND | Black | ESP32-S3 GND **AND** 12V Power GND (Must share GND) |
| **Pin 2** | +12V | Yellow | **External 12V Power** — PC PSU or 12V Adapter |
| **Pin 3** | Tach (RPM) | Green / Blue | **GPIO 5** + 10 kΩ pull-up resistor to 3.3V |
| **Pin 4** | PWM | White / Green | **GPIO 4** |

> ✅ **No external temperature sensor required** — uses the ESP32's internal chip temperature sensor.

---

### Detailed Diagram

```
                     ┌─────────────────────────┐
  12V Power (+) ─────┤ Fan Pin 2  (+12V)       │
                     │                         │
  12V Power (–) ──┬──┤ Fan Pin 1  (GND)        │  PC FAN
                  │  │                         │  4-pin
                  │  │ Fan Pin 3  (Tach) ───┐  │
                  │  │ Fan Pin 4  (PWM)  ───┼─┐│
                  │  └──────────────────────┼─┼┘
                  │                         │ │
                  │  ┌──────────────────┐   │ │
                  └──┤ GND              │   │ │   ESP32-S3 DevKit
                     │                  │   │ │
                     │ 3.3V ── 10kΩ ────┼───┘ │   (Pull-up Tach)
                     │                  │     │
                     │ GPIO  5 ◄────────┼─────┘   Tach / RPM Input
                     │                  │
                     │ GPIO  4 ─────────┼─────────► Fan Pin 4 (PWM Out)
                     │                  │
                     │ [Internal Temp]  │  ← No GPIO / external wires needed
                     └──────────────────┘
```

---

### Pull-up Resistor (Mandatory)

| Pin | Resistor | Connect from | To |
|-----|:--------:|--------------|-----|
| Tach – GPIO **5** | **10 kΩ** | 3.3V ESP32-S3 | Fan Pin 3 |

> Missing pull-up resistor → RPM will read 0 or be completely incorrect.

---

### Fan Power Supply

CPU fans require **12V / 0.1–0.5A**. **Do not** use the ESP32's 5V pin.

**Option 1 – Molex from PC PSU (Recommended):**
```
Molex 4-pin:
  Yellow (+12V) ──→ Fan Pin 2
  Black  (GND)   ──→ Fan Pin 1  +  ESP32 GND
```

**Option 2 – 12V DC Adapter:**
```
DC barrel adapter:
  (+) ──→ Fan Pin 2
  (–) ──→ Fan Pin 1  +  ESP32 GND
```

---

### Pre-power Checklist

- [ ] 12V power GND **is connected** to ESP32 GND.
- [ ] Fan Pin 2 is receiving correct **12V** (not 5V / 3.3V).
- [ ] GPIO 5 has a **10 kΩ** pull-up resistor to 3.3V.
- [ ] ESP32 is powered via USB or a separate 5V source.

---

## Setup & Build

### 1. WiFi Configuration

Open [`src/config.h`](src/config.h) and edit:

```c
#define WIFI_AP_SSID  "ESP32-Fan"
#define WIFI_AP_PASS  "12345678"
```

### 2. Check GPIOs

All default pins are declared in `config.h`:

| Function | GPIO | Notes |
|-----------|:----:|----------|
| PWM Output (Fan Pin 4) | **4** | Direct connection, no resistor needed |
| Tach / RPM (Fan Pin 3) | **5** | Requires 10 kΩ pull-up to 3.3V |
| Temperature | – | Internal ESP32-S3 sensor, no GPIO needed |

### 3. Build & Flash

> **Note**: PlatformIO stores the ESP32 framework (~500 MB) in `C:\Users\<user>\.platformio\`.
> At least **1.5 GB** of free space on the C: drive is required for the first build.
> If the C: drive is full, set the environment variable `PLATFORMIO_CORE_DIR=E:\.platformio` to move it to another drive.

```powershell
# Build firmware
pio run

# Upload SPIFFS (Upload web dashboard to ESP32 flash)
pio run --target uploadfs

# Flash firmware
pio run --target upload

# Open Serial Monitor
pio device monitor --baud 115200
```

### 4. Accessing the Dashboard

After flashing, open Serial Monitor → check the IP address:
```
[WIFI] Dashboard → http://192.168.1.xxx
```
Open your browser → enter the IP → the Dashboard will appear.

---

## REST API

| Method | Endpoint | Body | Description |
|:------:|----------|------|-------------|
| `GET` | `/api/status` | – | Get current status |
| `POST` | `/api/speed` | `{"speed": 75}` | Set speed 0–100% (switches to Manual mode) |
| `POST` | `/api/mode` | `{"mode": "auto"}` | Switch mode `auto` / `manual` |

**Example `/api/status` response:**
```json
{
  "rpm":   1250,
  "temp":  52.3,
  "speed": 65,
  "mode":  "auto"
}
```

---

## Fan Curve – Auto Mode

In **Auto** mode, fan speed is automatically adjusted based on the ESP32 chip temperature:

| Chip Temperature | Fan Speed |
|:----------------:|:---------:|
| < 30°C | 20% |
| 40°C | 35% |
| 50°C | 55% |
| 60°C | 75% |
| 70°C | 90% |
| ≥ 80°C | 100% |

Intermediate points are linearly interpolated. Customize this in `src/config.h` → `FAN_CURVE[]` array.

If the temperature reading differs from reality, adjust the `TEMP_OFFSET_C` constant in `config.h`.

---

## Libraries Used

| Library | Author | Function |
|----------|---------|-----------|
| ESPAsyncWebServer | ESP Async Team | Asynchronous Web Server |
| AsyncTCP | ESP Async Team | TCP layer |
| ArduinoJson v7 | Benoit Blanchon | JSON serialization / parsing |

> No temperature sensor library required — uses the internal `temprature_sens_read()` API available in the ESP32 Arduino core.

---

## Technical Notes

- **25 kHz PWM** – Intel 4-pin standard. ESP32 LEDC supports this natively without external hardware.
- **Tach ISR** – Uses `IRAM_ATTR` so the ISR runs in IRAM, avoiding blocks from flash cache access.
- **RPM Formula** – `RPM = (pulses / PPR) × (60000 / interval_ms)`, PPR = 2 (standard).
- **Internal Temperature** – The `temprature_sens_read()` API returns °F; the firmware converts it to °C. Accuracy is ±5°C, reflecting the ESP32's CPU load.
- **Web server** – `ESPAsyncWebServer` is non-blocking and does not hog the `loop()`.
- **SPIFFS** – The web dashboard (HTML/CSS) is stored on the ESP32 flash; upload separately via `uploadfs`.
