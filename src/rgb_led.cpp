#include "rgb_led.h"
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

#define NUM_LEDS 1

int s_ledPin = 38;
Adafruit_NeoPixel strip(NUM_LEDS, s_ledPin, NEO_GRB + NEO_KHZ800);

const uint32_t LED_COLOR_NORMAL = 0x00FF00;   // Green
const uint32_t LED_COLOR_SNIFFING = 0x0000FF; // Blue
const uint32_t LED_COLOR_JAMMING = 0xFF0000;  // Red
const uint32_t LED_COLOR_OFF = 0x000000;

static uint32_t s_currentColor = LED_COLOR_OFF;
static bool s_isBlinking = false;
static uint32_t s_lastBlinkTime = 0;
static bool s_blinkState = false;

bool s_ledIsAuto = true;
uint32_t s_ledManualColor = LED_COLOR_OFF;
uint8_t s_ledBrightness = 128; // Default 50%

void ledSetMode(bool isAuto) {
    if (s_ledIsAuto == isAuto) return;
    s_ledIsAuto = isAuto;
    
    Preferences prefs;
    prefs.begin("sys", false);
    prefs.putBool("ledauto", s_ledIsAuto);
    prefs.end();

    if (!isAuto) {
        uint8_t r = (s_ledManualColor >> 16) & 0xFF;
        uint8_t g = (s_ledManualColor >> 8) & 0xFF;
        uint8_t b = s_ledManualColor & 0xFF;
        ledSetColor(r, g, b);
    } else {
        // Re-apply auto color immediately
        uint8_t r = (s_currentColor >> 16) & 0xFF;
        uint8_t g = (s_currentColor >> 8) & 0xFF;
        uint8_t b = s_currentColor & 0xFF;
        ledSetColor(r, g, b);
    }
}

void ledSetManualColor(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t newColor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    if (s_ledManualColor == newColor) return;
    s_ledManualColor = newColor;

    Preferences prefs;
    prefs.begin("sys", false);
    prefs.putUInt("ledcolor", s_ledManualColor);
    prefs.end();

    if (!s_ledIsAuto) {
        ledSetColor(r, g, b);
    }
}

void ledSetBrightness(uint8_t brightness) {
    if (s_ledBrightness == brightness) return;
    s_ledBrightness = brightness;

    Preferences prefs;
    prefs.begin("sys", false);
    prefs.putUChar("ledbright", s_ledBrightness);
    prefs.end();

    strip.setBrightness(s_ledBrightness);
    strip.show();
}

void ledSetPin(int pin) {
    if (s_ledPin == pin) return;
    s_ledPin = pin;
    
    Preferences prefs;
    prefs.begin("sys", false);
    prefs.putInt("ledpin", s_ledPin);
    prefs.end();

    strip.setPin(s_ledPin);
    strip.begin();
    strip.setBrightness(s_ledBrightness);
    
    // Force a color update
    uint8_t r = (s_currentColor >> 16) & 0xFF;
    uint8_t g = (s_currentColor >> 8) & 0xFF;
    uint8_t b = s_currentColor & 0xFF;
    ledSetColor(r, g, b);
}

void ledSetup() {
    Preferences prefs;
    prefs.begin("sys", true);
    s_ledPin = prefs.getInt("ledpin", 48);
    s_ledIsAuto = prefs.getBool("ledauto", true);
    s_ledBrightness = prefs.getUChar("ledbright", 128);
    s_ledManualColor = prefs.getUInt("ledcolor", 0x00FF00); // Default to green if not set
    prefs.end();

    strip.setPin(s_ledPin);
    strip.begin();
    strip.setBrightness(s_ledBrightness);
    
    if (s_ledIsAuto) {
        ledSetStatusColor(LED_COLOR_NORMAL, false);
    } else {
        uint8_t r = (s_ledManualColor >> 16) & 0xFF;
        uint8_t g = (s_ledManualColor >> 8) & 0xFF;
        uint8_t b = s_ledManualColor & 0xFF;
        ledSetColor(r, g, b);
    }
}

void ledSetColor(uint8_t r, uint8_t g, uint8_t b) {
    strip.setPixelColor(0, strip.Color(r, g, b));
    strip.show();
}

void ledSetStatusColor(uint32_t hexColor, bool blink) {
    s_currentColor = hexColor;
    s_isBlinking = blink;
    s_blinkState = true;
    
    if (s_ledIsAuto) {
        uint8_t r = (hexColor >> 16) & 0xFF;
        uint8_t g = (hexColor >> 8) & 0xFF;
        uint8_t b = hexColor & 0xFF;
        ledSetColor(r, g, b);
    }
}

void ledLoop() {
    if (!s_ledIsAuto) return;
    if (!s_isBlinking) return;
    
    uint32_t now = millis();
    if (now - s_lastBlinkTime >= 250) { // Blink cycle 250ms (faster for clear warning)
        s_lastBlinkTime = now;
        s_blinkState = !s_blinkState;
        
        if (s_blinkState) {
            uint8_t r = (s_currentColor >> 16) & 0xFF;
            uint8_t g = (s_currentColor >> 8) & 0xFF;
            uint8_t b = s_currentColor & 0xFF;
            ledSetColor(r, g, b);
        } else {
            ledSetColor(0, 0, 0);
        }
    }
}
