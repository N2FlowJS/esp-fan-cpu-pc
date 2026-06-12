#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Kiểm tra xem Sniffer có đang hoạt động hay không.
 */
bool snifferIsActive();

/**
 * @brief Kiểm tra xem Sniffer có đang ở chế độ nhảy kênh hay không.
 */
bool snifferIsHopping();

/**
 * @brief Lấy kênh đang quét hiện tại.
 */
uint8_t snifferGetChannel();

/**
 * @brief Bắt đầu chế độ Sniffer.
 *        @param channel Kênh bắt đầu (1-13). Nếu = 0 và concurrent = false, sẽ tự động nhảy kênh.
 *        @param concurrent Nếu true, sẽ quét đồng thời trên kênh Wi-Fi hiện tại mà không ngắt Web Server.
 */
void snifferStart(uint8_t channel = 0, bool concurrent = false);

/**
 * @brief Dừng chế độ Sniffer.
 */
void snifferStop();

/**
 * @brief Chuyển kênh Wi-Fi (1-13).
 */
void snifferSetChannel(uint8_t channel);

/**
 * @brief In thống kê các gói tin đã quét và các thiết bị phát hiện được ra Serial.
 */
void snifferPrintStats();

/**
 * @brief Điền dữ liệu thống kê và danh sách thiết bị quét được vào JSON Document.
 */
void snifferGetStatsJson(JsonDocument& doc);

/**
 * @brief Cần gọi hàm này trong vòng lặp loop() chính.
 *        Thực hiện nhảy kênh (Channel Hopping) tự động nếu được kích hoạt.
 */
void snifferLoop();
