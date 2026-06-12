# ESP32 CPU Fan Controller

> Điều khiển quạt CPU PC 4-pin chuẩn Intel bằng ESP32 qua PWM 25 kHz, đọc RPM qua Tach, nhiệt độ từ **cảm biến nội bộ chip ESP32** và dashboard web dark-mode.

---

## Mục lục

1. [Cấu trúc project](#cấu-trúc-project)
2. [Sơ đồ kết nối dây](#sơ-đồ-kết-nối-dây)
3. [Cài đặt & Build](#cài-đặt--build)
4. [REST API](#rest-api)
5. [Fan Curve – Chế độ Auto](#fan-curve--chế-độ-auto)
6. [Thư viện dùng](#thư-viện-dùng)
7. [Ghi chú kỹ thuật](#ghi-chú-kỹ-thuật)

---

## Cấu trúc project

```
esp32-fan-cpu/
├── platformio.ini          ← Cấu hình PlatformIO (board, libs, filesystem)
├── scripts/
│   └── build_flags.py      ← Inject git commit hash vào build
├── src/
│   ├── config.h            ← Tất cả cấu hình (WiFi, GPIO, fan curve)
│   ├── main.cpp            ← Entry point (setup + loop)
│   ├── fan_control.h/cpp   ← PWM + RPM (ISR) + nhiệt độ nội bộ + Auto mode
│   └── web_server.h/cpp    ← ESPAsyncWebServer + REST API
├── data/
│   ├── index.html          ← Dashboard web (dark glassmorphism)
│   └── style.css           ← Stylesheet
└── README.md
```

---

## Sơ đồ kết nối dây

### Nhận biết connector quạt 4-pin

```
╔═══════════════════════╗
║  [1] [2] [3] [4]      ║  ← Mặt có khóa (latch)
╚═══════════════════════╝
   │    │    │    │
  GND  12V  RPM  PWM
(Đen)(Vàng)(Xanh)(Trắng/Xanh lá)
```

> ⚠️ Màu dây tuỳ hãng sản xuất có thể khác nhau — **đếm số chân** là chuẩn nhất.

---

### Bảng kết nối

| Fan Pin | Tên | Màu thường gặp | Kết nối đến |
|:-------:|-----|:--------------:|-------------|
| **Pin 1** | GND | Đen | GND của ESP32-S3 **VÀ** GND nguồn 12V (bắt buộc chung GND) |
| **Pin 2** | +12V | Vàng | **Nguồn 12V ngoài** — PSU máy tính hoặc adapter 12V |
| **Pin 3** | Tach (RPM) | Xanh lá / Xanh dương | **GPIO 5** + điện trở 10 kΩ kéo lên 3.3V |
| **Pin 4** | PWM | Trắng / Xanh lá | **GPIO 4** |

> ✅ **Không cần cảm biến nhiệt độ ngoài** — dùng cảm biến nhiệt nội bộ của chip ESP32.

---

### Sơ đồ chi tiết

```
                     ┌─────────────────────────┐
  Nguồn 12V (+) ─────┤ Fan Pin 2  (+12V)       │
                     │                         │
  Nguồn 12V (–) ──┬──┤ Fan Pin 1  (GND)        │  PC FAN
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
                     │ [Nhiệt độ nội bộ]│  ← Không cần GPIO / dây ngoài
                     └──────────────────┘
```

---

### Điện trở kéo lên (bắt buộc)

| Chân | Điện trở | Nối từ | Đến |
|------|:--------:|--------|-----|
| Tach – GPIO **5** | **10 kΩ** | 3.3V ESP32-S3 | Fan Pin 3 |

> Thiếu điện trở pull-up → RPM đọc bằng 0 hoặc sai hoàn toàn.

---

### Nguồn cấp cho quạt

Quạt CPU cần **12V / 0.1–0.5A**. **Không** dùng 5V của ESP32.

**Cách 1 – Molex từ PSU máy tính (khuyến nghị):**
```
Molex 4-pin:
  Vàng (+12V) ──→ Fan Pin 2
  Đen   (GND) ──→ Fan Pin 1  +  GND ESP32
```

**Cách 2 – Adapter 12V DC:**
```
Adapter DC barrel:
  (+) ──→ Fan Pin 2
  (–) ──→ Fan Pin 1  +  GND ESP32
```

---

### Checklist trước khi cấp điện

- [ ] GND nguồn 12V **nối chung** với GND ESP32
- [ ] Fan Pin 2 nhận đúng **12V** (không phải 5V / 3.3V)
- [ ] GPIO 5 có điện trở **10 kΩ** pull-up lên 3.3V
- [ ] ESP32 cấp nguồn qua cáp USB hoặc nguồn 5V riêng

---

## Cài đặt & Build

### 1. Cấu hình WiFi

Mở [`src/config.h`](src/config.h) và sửa:

```c
#define WIFI_SSID  "TênMạngWiFi"
#define WIFI_PASS  "MatKhauWiFi"
```

### 2. Kiểm tra GPIO

Tất cả chân mặc định khai báo trong `config.h`:

| Chức năng | GPIO | Ghi chú |
|-----------|:----:|----------|
| PWM Output (Fan Pin 4) | **4** | Trực tiếp, không cần điện trở |
| Tach / RPM (Fan Pin 3) | **5** | Cần 10 kΩ pull-up lên 3.3V |
| Nhiệt độ | – | Cảm biến nội bộ chip ESP32-S3, không cần GPIO |

### 3. Build & Flash

> **Lưu ý**: PlatformIO lưu framework ESP32 (~500 MB) vào `C:\Users\<user>\.platformio\`.
> Cần ít nhất **1.5 GB** trống trên ổ C: trước khi build lần đầu.
> Nếu ổ C: đầy, đặt biến môi trường `PLATFORMIO_CORE_DIR=E:\.platformio` để chuyển sang ổ khác.

```powershell
# Build firmware
pio run

# Upload SPIFFS (web dashboard lên flash ESP32)
pio run --target uploadfs

# Flash firmware
pio run --target upload

# Xem Serial Monitor
pio device monitor --baud 115200
```

### 4. Truy cập Dashboard

Sau khi flash, mở Serial Monitor → thấy IP:
```
[WIFI] Dashboard → http://192.168.1.xxx
```
Mở trình duyệt → nhập IP → Dashboard hiện ra.

---

## REST API

| Method | Endpoint | Body | Mô tả |
|:------:|----------|------|-------|
| `GET` | `/api/status` | – | Lấy trạng thái hiện tại |
| `POST` | `/api/speed` | `{"speed": 75}` | Đặt tốc độ 0–100% (chuyển sang Manual) |
| `POST` | `/api/mode` | `{"mode": "auto"}` | Chuyển chế độ `auto` / `manual` |

**Response mẫu `/api/status`:**
```json
{
  "rpm":   1250,
  "temp":  52.3,
  "speed": 65,
  "mode":  "auto"
}
```

---

## Fan Curve – Chế độ Auto

Ở chế độ **Auto**, tốc độ quạt tự động điều chỉnh theo nhiệt độ chip ESP32:

| Nhiệt độ chip | Tốc độ quạt |
|:-------------:|:-----------:|
| < 30°C | 20% |
| 40°C | 35% |
| 50°C | 55% |
| 60°C | 75% |
| 70°C | 90% |
| ≥ 80°C | 100% |

Các điểm trung gian được nội suy tuyến tính. Tùy chỉnh trong `src/config.h` → mảng `FAN_CURVE[]`.

Nếu nhiệt độ đọc lệch so với thực tế, chỉnh hằng số `TEMP_OFFSET_C` trong `config.h`.

---

## Thư viện dùng

| Thư viện | Tác giả | Chức năng |
|----------|---------|-----------|
| ESPAsyncWebServer | ESP Async Team | Web server bất đồng bộ |
| AsyncTCP | ESP Async Team | TCP layer |
| ArduinoJson v7 | Benoit Blanchon | JSON serialize / parse |

> Không cần thư viện cảm biến nhiệt độ — dùng API nội bộ `temprature_sens_read()` có sẵn trong ESP32 Arduino core.

---

## Ghi chú kỹ thuật

- **PWM 25 kHz** – Chuẩn Intel 4-pin. ESP32 LEDC hỗ trợ native, không cần phần cứng ngoài.
- **Tach ISR** – Dùng `IRAM_ATTR` để ISR chạy trong IRAM, tránh bị block bởi flash cache.
- **Công thức RPM** – `RPM = (pulses / PPR) × (60000 / interval_ms)`, PPR = 2 (chuẩn).
- **Nhiệt độ nội bộ** – API `temprature_sens_read()` trả về °F, firmware tự quy đổi sang °C. Độ chính xác ±5°C, phản ánh tải CPU của chip ESP32.
- **Web server** – `ESPAsyncWebServer` non-blocking, không chiếm `loop()`.
- **SPIFFS** – Web dashboard (HTML/CSS) lưu trên flash ESP32, upload riêng bằng `uploadfs`.
