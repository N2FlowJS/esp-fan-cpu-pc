#ifndef RGB_LED_H
#define RGB_LED_H

#include <Arduino.h>

void ledSetup();
void ledSetColor(uint8_t r, uint8_t g, uint8_t b);
void ledLoop(); // for blinking effects without delay

// Predefined colors
extern const uint32_t LED_COLOR_NORMAL;
extern const uint32_t LED_COLOR_SNIFFING;
extern const uint32_t LED_COLOR_JAMMING;
extern const uint32_t LED_COLOR_OFF;

extern bool s_ledIsAuto;
extern uint32_t s_ledManualColor;
extern uint8_t s_ledBrightness;
extern int s_ledPin;

void ledSetMode(bool isAuto);
void ledSetManualColor(uint8_t r, uint8_t g, uint8_t b);
void ledSetStatusColor(uint32_t hexColor, bool blink = false);
void ledSetBrightness(uint8_t brightness);
void ledSetPin(int pin);

#endif // RGB_LED_H
