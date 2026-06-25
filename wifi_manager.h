#pragma once

#include <Arduino.h>

// ============================================================================
// Gestion de WiFi: conexion, persistencia de credenciales y reset.
// ============================================================================

void applyDefaultWifiConfig(bool skipConfigReload);
void loadWifiConfig();
void resetWifiRadio();
bool runWifiConfigPortal();
bool connectToSelectedWiFi(bool showLedFeedback);
bool connectWiFi();
void checkWifiResetPin();
bool saveWifiConfig(const String& ssid, const String& pass);
