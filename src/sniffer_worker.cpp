#include "sniffer_worker.h"
#include "wifi_sniffer_common.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdio.h>

static QueueHandle_t s_packetQueue = NULL;
static TaskHandle_t  s_snifferTask = NULL;

static void snifferWorkerTask(void* pvParameters) {
    QueuedPacket pkt;
    static uint32_t processCount = 0;
    static uint32_t lastHealthCheck = millis();

    if (!s_packetQueue) {
        Serial.println("[WORKER] ERROR: Queue is NULL! Task cannot run. Deleting task.");
        vTaskDelete(NULL);
        return;
    }

    Serial.println("[WORKER] Worker task started successfully, waiting for packets...");

    while (true) {
        if (millis() - lastHealthCheck > 10000) {
            lastHealthCheck = millis();
            Serial.printf("[WORKER] Health check: processed %lu packets so far\n", processCount);
        }

        if (xQueueReceive(s_packetQueue, &pkt, portMAX_DELAY) == pdPASS) {
            uint8_t frame_control = pkt.payload[0];
            uint8_t type_val = frame_control & 0x0C;

            processCount++;
            if (processCount <= 10 || processCount % 50 == 0) {
                Serial.printf("[WORKER] Packet #%lu: fc=0x%02X type=0x%02X len=%d rssi=%d\n",
                    processCount, frame_control, type_val, pkt.len, pkt.rssi);
            }

            if (type_val == 0x00) {
                if (processCount <= 10) Serial.println("[WORKER] → Dispatching MGMT frame");
                dispatchMgmtFrame(frame_control, pkt.payload, pkt.len, pkt.rssi);
            } else if (type_val == 0x04) {
                if (processCount <= 10) Serial.printf("[WORKER] → Control frame (not currently handled)\n");
            } else if (type_val == 0x08) {
                if (processCount <= 10) Serial.println("[WORKER] → Dispatching DATA frame");
                dispatchDataFrame(frame_control, pkt.payload, pkt.len, pkt.rssi);
            } else if (type_val == 0x0C) {
                if (processCount <= 10) Serial.printf("[WORKER] → Extension frame (not currently handled)\n");
            } else {
                Serial.printf("[WORKER] ⚠ Unknown frame type: 0x%02X (fc=0x%02X)\n", type_val, frame_control);
            }

            if (processCount % 10 == 0) taskYIELD();
        }
    }
}

bool createPacketQueue(size_t depth) {
    if (s_packetQueue) return true;
    s_packetQueue = xQueueCreate(depth, sizeof(QueuedPacket));
    Serial.printf("[SNIFFER] Queue creation attempt: %p (size: %d packets)\n", s_packetQueue, (int)depth);
    return s_packetQueue != NULL;
}

void deletePacketQueue() {
    if (s_packetQueue) {
        vQueueDelete(s_packetQueue);
        s_packetQueue = NULL;
        Serial.println("[SNIFFER] Packet queue deleted.");
    }
}

bool startSnifferWorker() {
    if (s_snifferTask) return true;
    BaseType_t result = xTaskCreatePinnedToCore(snifferWorkerTask, "sniffer_worker", 8 * 1024, NULL, 1, &s_snifferTask, 0);
    Serial.printf("[SNIFFER] Task creation result: %d, handle: %p\n", result, s_snifferTask);
    if (result != pdPASS) {
        s_snifferTask = NULL;
        return false;
    }
    return true;
}

void stopSnifferWorker() {
    if (s_snifferTask) {
        vTaskDelete(s_snifferTask);
        s_snifferTask = NULL;
        Serial.println("[SNIFFER] Worker task deleted.");
    }
}

BaseType_t snifferEnqueueFromISR(const QueuedPacket* pkt, BaseType_t* pxHigherPriorityTaskWoken) {
    if (!s_packetQueue) return errQUEUE_FULL;
    return xQueueSendToBackFromISR(s_packetQueue, pkt, pxHigherPriorityTaskWoken);
}

bool isPacketQueueCreated() {
    return s_packetQueue != NULL;
}
