#pragma once

// ============================================================================
// Configuracion de compilacion (constantes). Editar aqui los valores fijos.
// ============================================================================

// WiFi estandar para instalacion con hotspot del celular.
static const char* DEFAULT_WIFI_SSID = "JavoPoint";
static const char* DEFAULT_WIFI_PASS = "12345678";

// API de pulsos
static const char* API_BASE_URL = "https://tecnovend-api-production.up.railway.app";

// ID alfanumerico grabado en el firmware. El servidor lo vincula con la maquina.
static const char* ARDUINO_ID = "ARD-7F3A9C";
static const char* API_KEY = "";
static const char* FW_VERSION = "1.0.5-api-arduino-id";

// ---- LED ----
#define USE_RGB_LED 1
#ifndef LED_PIN
#define LED_PIN 48
#endif

// ---- Pines ----
// INHIBIT: libre/abierto por pullup = en servicio. A GND = fuera de servicio.
static const int INHIBIT_PIN = 14;
static const int PULSE_PIN = 13;
static const int WIFI_RESET_PIN = 12;

// ---- Tiempos ----
// Para maxima velocidad bajar este valor. 1000 ms es mas rapido que la version original.
// Si el servidor pide estrictamente 3 s, cambiar a 3000.
static const unsigned long POLL_INTERVAL_MS = 1000;
static const unsigned long HEARTBEAT_INTERVAL_MS = 60UL * 60UL * 1000UL;
static const unsigned long HTTP_TIMEOUT_MS = 4000;
static const unsigned long HEARTBEAT_TIMEOUT_MS = 2000;
static const unsigned long RESULT_RETRY_INTERVAL_MS = 10000;
static const unsigned long STATE_HEARTBEAT_RETRY_MS = 5000;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 12000;
static const unsigned long WIFI_LED_FEEDBACK_MS = 10000;
static const unsigned long CONFIG_OK_LED_MS = 3000;
static const unsigned long CLIENT_WIFI_OK_LED_MS = 3000;
static const unsigned long BOOT_SERVICE_GRACE_MS = 2UL * 60UL * 1000UL;
static const unsigned long SALE_INHIBIT_GRACE_MS = 72UL * 1000UL;
static const unsigned long INHIBIT_OUT_OF_SERVICE_DEBOUNCE_MS = 1000;
static const unsigned long SALE_START_WAIT_MS = 10000;
static const unsigned long WIFI_RESET_HOLD_MS = 2000;
static const int WIFI_CONNECT_ATTEMPTS = 5;
static const int SAVED_WIFI_CONNECT_ATTEMPTS = 2;
static const int RESULT_QUEUE_SIZE = 8;
