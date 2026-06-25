// ============================================================================
// TecnoVend - Firmware JavoPoint (ESP32)
//
// Punto de entrada del sketch. La logica esta repartida en modulos:
//   - config.h       : constantes (WiFi, API, pines, tiempos)
//   - types.h        : enum y structs compartidos
//   - globals.*      : estado mutable compartido
//   - led.*          : LED RGB de estado
//   - wifi_manager.* : conexion y persistencia de WiFi
//   - json_utils.*   : parseo de JSON ligero
//   - api.*          : HTTP, heartbeat, config y poll
//   - pulses.*       : ejecucion de pulsos y cola de resultados
//   - service.*      : estado de servicio (pin INHIBIT) y ventas
// ============================================================================

#include "config.h"
#include "globals.h"
#include "led.h"
#include "wifi_manager.h"
#include "api.h"
#include "pulses.h"
#include "service.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(INHIBIT_PIN, INPUT_PULLUP);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);
  setupLed();
  loadWifiConfig();
  if (usingDefaultWifi) {
    runWifiConfigPortal();
  }
  connectWiFi();
  if (!skipConfigThisBoot) {
    fetchConfigOnce();
  } else {
    Serial.println("Config web omitida por reset WiFi. La placa queda en JavoPoint.");
  }

  reportedInService = true;
  lastReportedInService = reportedInService;
  if (!rawMachineInService()) {
    inhibitLowActive = true;
    bootInhibitGraceActive = true;
    inhibitLowSinceMs = millis();
    Serial.println("Inhibit activo al encender. Esperando gracia inicial antes de reportar fuera de servicio.");
  }

  // Primer poll inmediato. El heartbeat periodico queda espaciado para reducir trafico.
  lastPollMs = millis() - POLL_INTERVAL_MS;
  lastHeartbeatMs = millis();
}

void loop() {
  checkWifiResetPin();

  unsigned long now = millis();
  updateInhibitState();
  updateStatusLed();

  if (!connectWiFi()) {
    updateStatusLed();
    delay(1000);
    return;
  }

  now = millis();
  updateStatusLed();

  if (!startupHeartbeatSent &&
      now - lastStartupHeartbeatAttemptMs >= STARTUP_HEARTBEAT_RETRY_MS) {
    lastStartupHeartbeatAttemptMs = now;
    if (sendServiceHeartbeat("startup", "")) {
      startupHeartbeatSent = true;
      lastHeartbeatMs = millis();
    }
  }

  retryPendingPulseResults();

  bool pollDue = now - lastPollMs >= POLL_INTERVAL_MS;
  if (pollDue) {
    bool ok = pollOnce();
    lastPollMs = millis();

    if (!ok) {
      // No bloquear mucho: el proximo intento vuelve rapido.
      delay(100);
    }
    return;
  }

  bool heartbeatDue = now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS;

  if (heartbeatDue) {
    sendHeartbeat();
    lastHeartbeatMs = millis();
    return;
  }

  delay(20);
}
