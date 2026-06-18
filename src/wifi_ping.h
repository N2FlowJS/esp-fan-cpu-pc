#pragma once
#include <Arduino.h>

struct PingResult {
    bool success;
    float time;      // ms
    int ttl;
    int bytes;
    String ip;
    String target;
    bool finished;
};

/**
 * @brief Start a ping session.
 * @param target Hostname or IP string.
 * @param count Number of pings to send.
 */
void pingStart(const String& target, int count = 4);

/**
 * @brief Stop current ping session.
 */
void pingStop();

/**
 * @brief Check if ping is active.
 */
bool pingIsActive();
