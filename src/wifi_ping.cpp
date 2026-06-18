#include "wifi_ping.h"
#include "web_server.h"
#include <lwip/inet.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <ping/ping_sock.h>
#include <ArduinoJson.h>

static esp_ping_handle_t s_ping_handle = NULL;
static String s_target_host = "";
static bool s_active = false;

// SSE broadcast helper (implemented in web_server.cpp, but we'll use it via a callback or extern)
extern void webServerBroadcast(const char* event, const char* data);

static void s_ping_on_ping_success(esp_ping_handle_t hdl, void *args) {
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, bytes;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &bytes, sizeof(bytes));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));

    JsonDocument doc;
    doc["type"] = "result";
    doc["success"] = true;
    doc["time"] = elapsed_time;
    doc["ttl"] = ttl;
    doc["bytes"] = bytes;
    doc["seq"] = seqno;
    doc["ip"] = ipaddr_ntoa(&target_addr);
    doc["target"] = s_target_host;
    
    String json;
    serializeJson(doc, json);
    webServerBroadcast("ping", json.c_str());
    
    Serial.printf("[PING] %d bytes from %s: icmp_seq=%d ttl=%d time=%d ms\n",
                  bytes, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void s_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args) {
    uint16_t seqno;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    
    JsonDocument doc;
    doc["type"] = "result";
    doc["success"] = false;
    doc["seq"] = seqno;
    doc["target"] = s_target_host;
    
    String json;
    serializeJson(doc, json);
    webServerBroadcast("ping", json.c_str());
    
    Serial.printf("[PING] From %s: icmp_seq=%d timeout\n", s_target_host.c_str(), seqno);
}

static void s_ping_on_ping_end(esp_ping_handle_t hdl, void *args) {
    uint32_t transmitted, received, total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    
    JsonDocument doc;
    doc["type"] = "end";
    doc["transmitted"] = transmitted;
    doc["received"] = received;
    doc["time_ms"] = total_time_ms;
    
    String json;
    serializeJson(doc, json);
    webServerBroadcast("ping", json.c_str());
    
    Serial.printf("[PING] %d packets transmitted, %d received, time %dms\n", transmitted, received, total_time_ms);
    
    s_active = false;
    esp_ping_delete_session(hdl);
    s_ping_handle = NULL;
}

void pingStart(const String& target, int count) {
    if (s_active) pingStop();
    
    s_target_host = target;
    s_active = true;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    
    ip_addr_t target_addr;
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    if (getaddrinfo(target.c_str(), NULL, &hints, &res) != 0) {
        Serial.printf("[PING] DNS lookup failed for %s\n", target.c_str());
        s_active = false;
        return;
    }
    struct sockaddr_in *saddr = (struct sockaddr_in *)res->ai_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &saddr->sin_addr);
    freeaddrinfo(res);

    ping_config.target_addr = target_addr;
    ping_config.count = count;

    esp_ping_callbacks_t cbs = {
        .cb_args = NULL,
        .on_ping_success = s_ping_on_ping_success,
        .on_ping_timeout = s_ping_on_ping_timeout,
        .on_ping_end = s_ping_on_ping_end
    };

    esp_ping_new_session(&ping_config, &cbs, &s_ping_handle);
    esp_ping_start(s_ping_handle);
    
    Serial.printf("[PING] Starting ping to %s (%s)...\n", target.c_str(), ipaddr_ntoa(&target_addr));
}

void pingStop() {
    if (!s_active || !s_ping_handle) return;
    esp_ping_stop(s_ping_handle);
    // on_ping_end will handle cleanup
}

bool pingIsActive() {
    return s_active;
}
