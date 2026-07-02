#include "led.h"
#include "config.h"
#include "globals.h"
#include "service.h"
#include "safety.h"

void setLedRgb(uint8_t red, uint8_t green, uint8_t blue) {
#if USE_RGB_LED
  neopixelWrite(LED_PIN, red, green, blue);
#else
  bool on = red > 0 || green > 0 || blue > 0;
  digitalWrite(LED_PIN, LED_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
#endif
}

void setLed(bool on) {
  setLedRgb(0, on ? 40 : 0, 0);
}

void setupLed() {
#if USE_RGB_LED
  setLed(false);
#else
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
#endif
}

void updateStatusLed() {
  if (configPortalActive) {
    setLedRgb(45, 45, 0);  // Amarillo fijo: red propia activa, falta configurar WiFi.
    return;
  }

  if (!rawMachineInService()) {
    setLedRgb(40, 0, 0);  // Rojo fijo: pin INHIBIT abierto, maquina fuera de servicio.
    return;
  }

  unsigned long now = millis();

  if (communicationState == COMM_NO_CONNECTION) {
    bool on = ((now / 500) % 2) == 0;
    setLedRgb(on ? 40 : 0, 0, 0);  // Rojo titilando: sin conexion.
  } else if (communicationState == COMM_API_REACHED) {
    setLedRgb(45, 20, 0);  // Naranja fijo: hay internet/API, pero no vinculo OK.
  } else {
    bool on = ((now / 1000) % 2) == 0;
    setLedRgb(0, 0, on ? 40 : 0);  // Azul titilando: vinculo web OK.
  }
}

void blinkLedRgb(uint8_t red, uint8_t green, uint8_t blue, unsigned long durationMs) {
  unsigned long startMs = millis();
  while (millis() - startMs < durationMs) {
    feedWatchdog();
    setLedRgb(red, green, blue);
    watchdogDelay(250);
    setLedRgb(0, 0, 0);
    watchdogDelay(250);
  }
}

void showConfigOkLed() {
  setLedRgb(35, 0, 45);  // Violeta: configuracion cargada OK.
  watchdogDelay(CONFIG_OK_LED_MS);
  setLedRgb(0, 0, 0);
}

void showClientWifiOkLed() {
  setLedRgb(45, 45, 45);  // Blanco: conectado a red WiFi del cliente.
  watchdogDelay(CLIENT_WIFI_OK_LED_MS);
  setLedRgb(0, 0, 0);
}
