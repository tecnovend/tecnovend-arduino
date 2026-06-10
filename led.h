#pragma once

#include <Arduino.h>

// ============================================================================
// Control del LED RGB de estado.
// ============================================================================

void setLedRgb(uint8_t red, uint8_t green, uint8_t blue);
void setLed(bool on);
void setupLed();
void updateStatusLed();
void blinkLedRgb(uint8_t red, uint8_t green, uint8_t blue, unsigned long durationMs);
void showConfigOkLed();
void showClientWifiOkLed();
