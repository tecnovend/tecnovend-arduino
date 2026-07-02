#pragma once

#include <Arduino.h>

// ============================================================================
// Seguridad: watchdog de tarea para reiniciar si el firmware se queda trabado.
// ============================================================================

void setupWatchdog();
void feedWatchdog();
void pauseWatchdog();
void resumeWatchdog();
void watchdogDelay(unsigned long durationMs);
