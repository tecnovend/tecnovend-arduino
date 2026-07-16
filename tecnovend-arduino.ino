// ============================================================================
// TecnoVend - Firmware VendPoint (ESP32)
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
//   - serial_status.*: diagnostico por Monitor Serie
// ============================================================================

#include "config.h"
#include "globals.h"
#include "led.h"
#include "wifi_manager.h"
#include "api.h"
#include "pulses.h"
#include "service.h"
#include "safety.h"
#include "ota.h"
#include "serial_status.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  printSerialBootStatus();
  initBreadcrumbs();
  setupWatchdog();
  setupOta();
  feedWatchdog();

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
    Serial.println("Config web omitida por reset WiFi. La placa queda en VendPoint.");
  }

  // Reportar log de estado inicial para diagnóstico remoto inmediato
  sendRemoteStatusLog();

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
  lastNetworkOkMs = millis();
  lastRemoteStatusLogMs = millis();
}

void loop() {
  feedWatchdog();
  setBreadcrumb("loop: start");
  checkOtaTimeout();
  printSerialStatusIfDue();
  checkWifiResetPin();

  unsigned long now = millis();

  // Si lleva mas de 30 minutos sin conexion exitosa a la API, reiniciamos el chip
  unsigned long tiempoSinInternet = (lastNetworkOkMs > 0) ? (now - lastNetworkOkMs) : now;
  if (tiempoSinInternet >= 30UL * 60UL * 1000UL) {
    setBreadcrumb("reboot: wifi stale");
    Serial.println("[SYSTEM] Alerta: Sin conexion por 30 minutos. Reiniciando chip...");
    delay(500);
    ESP.restart();
  }

  updateInhibitState();
  updateStatusLed();

  setBreadcrumb("loop: connect_wifi");
  if (!connectWiFi()) {
    updateStatusLed();
    watchdogDelay(1000);
    return;
  }

  now = millis();
  updateStatusLed();

  if (!startupHeartbeatSent &&
      now - lastStartupHeartbeatAttemptMs >= STARTUP_HEARTBEAT_RETRY_MS) {
    lastStartupHeartbeatAttemptMs = now;
    setBreadcrumb("loop: startup_heartbeat");
    if (now <= STARTUP_HEARTBEAT_WINDOW_MS && sendServiceHeartbeat("startup", "")) {
      startupHeartbeatSent = true;
      lastHeartbeatMs = millis();
    } else if (now > STARTUP_HEARTBEAT_WINDOW_MS) {
      sendHeartbeat();
      startupHeartbeatSent = true;
      lastHeartbeatMs = millis();
    }
  }

  retryPendingPulseResults();

  bool tooManyNetworkFailures = consecutiveNetworkFailures >= NETWORK_FAILURES_BEFORE_RECOVERY;
  bool staleNetworkLink = communicationState == COMM_NO_CONNECTION &&
                          now - lastNetworkOkMs >= NETWORK_STALE_LINK_MS;
  if ((tooManyNetworkFailures || staleNetworkLink) &&
      now - lastNetworkRecoveryMs >= NETWORK_RECOVERY_COOLDOWN_MS) {
    lastNetworkRecoveryMs = now;
    consecutiveNetworkFailures = 0;
    setBreadcrumb("loop: wifi_recovery");
    forceWifiReconnect(tooManyNetworkFailures ? "fallas consecutivas API" : "vinculo web vencido");
    watchdogDelay(1000);
    return;
  }

#if ENABLE_REMOTE_STATUS_LOG
  bool statusLogDue = now - lastRemoteStatusLogMs >= REMOTE_STATUS_LOG_INTERVAL_MS;
  if (statusLogDue) {
    setBreadcrumb("loop: status_log");
    sendRemoteStatusLog();
    lastRemoteStatusLogMs = millis();
    return;
  }
#endif

  bool pollDue = now - lastPollMs >= POLL_INTERVAL_MS;
  if (pollDue) {
    setBreadcrumb("loop: poll");
    bool ok = pollOnce();
    lastPollMs = millis();

    if (!ok) {
      // No bloquear mucho: el proximo intento vuelve rapido.
      watchdogDelay(100);
    }
    return;
  }

  bool heartbeatDue = now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS;

  if (heartbeatDue) {
    setBreadcrumb("loop: regular_heartbeat");
    sendHeartbeat();
    lastHeartbeatMs = millis();
    return;
  }

  setBreadcrumb("loop: idle");
  watchdogDelay(20);
}
