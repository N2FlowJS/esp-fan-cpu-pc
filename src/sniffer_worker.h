#pragma once
#include <Arduino.h>
#include "wifi_sniffer_common.h"

// Packet capture constants
#define DEFAULT_MAX_QUEUED_PACKETS 250
#define DEFAULT_PACKET_CAPTURE_LEN 128

// Backwards-compatible macro names used elsewhere in codebase
#define MAX_QUEUED_PACKETS DEFAULT_MAX_QUEUED_PACKETS
#define PACKET_CAPTURE_LEN DEFAULT_PACKET_CAPTURE_LEN

struct QueuedPacket {
    uint32_t timestamp;
    int      rssi;
    uint16_t len;
    uint8_t  payload[DEFAULT_PACKET_CAPTURE_LEN];
};

// Create packet queue with given depth. Returns true on success.
bool createPacketQueue(size_t depth);

// Delete packet queue
void deletePacketQueue();

// Start worker task (returns true if started)
bool startSnifferWorker();

// Stop worker task
void stopSnifferWorker();

// Enqueue a packet from ISR context. Returns pdPASS on success.
BaseType_t snifferEnqueueFromISR(const QueuedPacket* pkt, BaseType_t* pxHigherPriorityTaskWoken);

// Check if queue exists
bool isPacketQueueCreated();
