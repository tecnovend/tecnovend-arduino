#include "safety.h"
#include "config.h"

#if ENABLE_WATCHDOG
#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>
#endif

void setupWatchdog() {
#if ENABLE_WATCHDOG
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t watchdogConfig = {};
  watchdogConfig.timeout_ms = WATCHDOG_TIMEOUT_MS;
  watchdogConfig.idle_core_mask = 0; // No monitorear tareas Idle de los cores para evitar falsos positivos por carga de WiFi/red
  watchdogConfig.trigger_panic = true;

  // En ESP-IDF v5, si el watchdog ya está inicializado por Arduino al arrancar, se debe usar reconfigure.
  esp_err_t initResult = esp_task_wdt_reconfigure(&watchdogConfig);
  if (initResult != ESP_OK) {
    // Si falla reconfigure, intentamos init
    initResult = esp_task_wdt_init(&watchdogConfig);
  }
  if (initResult != ESP_OK) {
    Serial.print("Watchdog init/reconfigure fallo: ");
    Serial.println((int)initResult);
  }
#else
  esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true);
#endif

  esp_err_t addResult = esp_task_wdt_add(NULL);
  if (addResult != ESP_OK && addResult != ESP_ERR_INVALID_STATE) {
    Serial.print("Watchdog add fallo: ");
    Serial.println((int)addResult);
  } else {
    Serial.print("Watchdog activo (Loop task). Timeout ms=");
    Serial.println(WATCHDOG_TIMEOUT_MS);
  }
#endif
}

void feedWatchdog() {
#if ENABLE_WATCHDOG
  esp_task_wdt_reset();
#endif
}

void pauseWatchdog() {
#if ENABLE_WATCHDOG
  esp_task_wdt_delete(NULL);
#endif
}

void resumeWatchdog() {
#if ENABLE_WATCHDOG
  esp_task_wdt_add(NULL);
  feedWatchdog();
#endif
}

void watchdogDelay(unsigned long durationMs) {
  unsigned long startMs = millis();
  while (millis() - startMs < durationMs) {
    feedWatchdog();
    unsigned long remainingMs = durationMs - (millis() - startMs);
    delay(remainingMs > 100 ? 100 : remainingMs);
  }
  feedWatchdog();
}
