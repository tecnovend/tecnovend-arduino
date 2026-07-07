#pragma once

// ============================================================================
// Configuracion de compilacion (constantes). Editar aqui los valores fijos.
// ============================================================================

// WiFi estandar para instalacion con hotspot del celular.
static const char* DEFAULT_WIFI_SSID = "JavoPoint";
static const char* DEFAULT_WIFI_PASS = "12345678";

// WiFi propio del ESP para primera configuracion sin app ni internet.
static const char* CONFIG_PORTAL_SSID = "JavoPoint_Config";
static const char* CONFIG_PORTAL_PASS = "12345678";

// API de pulsos
static const char* API_BASE_URL = "https://tecnovend-api-production.up.railway.app";

// ID alfanumerico grabado en el firmware. El servidor lo vincula con la maquina.
static const char* INITIAL_ARDUINO_ID = "ARD-000002";
static const char* API_KEY = "";
static const char* FW_VERSION = "0.0.3";

// ---- LED ----
#define USE_RGB_LED 1
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
#ifndef LED_PIN
#define LED_PIN 48
#endif
static const bool LED_ACTIVE_LOW = true;

// ---- Pines ----
// INHIBIT: a GND/LOW = en servicio. Abierto por pullup/HIGH = fuera de servicio.
static const int INHIBIT_PIN = 14;
static const int PULSE_PIN = 13;
static const int WIFI_RESET_PIN = 12;

// ---- Tiempos ----
// Para maxima velocidad bajar este valor. 1000 ms es mas rapido que la version original.
// Si el servidor pide estrictamente 3 s, cambiar a 3000.
static const unsigned long POLL_INTERVAL_MS = 1000;
static const unsigned long HEARTBEAT_INTERVAL_MS = 60UL * 60UL * 1000UL;
static const unsigned long STARTUP_HEARTBEAT_RETRY_MS = 5000;
static const unsigned long STARTUP_HEARTBEAT_WINDOW_MS = 2UL * 60UL * 1000UL;
static const unsigned long HTTP_TIMEOUT_MS = 4000;
static const unsigned long HEARTBEAT_TIMEOUT_MS = 2000;
static const unsigned long HTTP_CONNECT_TIMEOUT_MS = 2500;
static const unsigned long NETWORK_RECOVERY_COOLDOWN_MS = 60000;
static const unsigned long NETWORK_STALE_LINK_MS = 60UL * 1000UL;
static const unsigned long REMOTE_STATUS_LOG_INTERVAL_MS = 30UL * 60UL * 1000UL;
static const unsigned long RESULT_RETRY_INTERVAL_MS = 10000;
static const unsigned long STATE_HEARTBEAT_RETRY_MS = 5000;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 12000;
static const unsigned long WIFI_LED_FEEDBACK_MS = 10000;
static const unsigned long CONFIG_OK_LED_MS = 3000;
static const unsigned long CLIENT_WIFI_OK_LED_MS = 3000;
static const unsigned long BOOT_SERVICE_GRACE_MS = 3000;
static const unsigned long SALE_INHIBIT_GRACE_MS = 200UL * 1000UL;
static const unsigned long INHIBIT_OUT_OF_SERVICE_DEBOUNCE_MS = 3000;
static const unsigned long SALE_START_WAIT_MS = 5UL * 60UL * 1000UL;
static const unsigned long WIFI_RESET_HOLD_MS = 2000;
static const int INHIBIT_READ_SAMPLES = 7;
static const int INHIBIT_SERVICE_MIN_LOW_SAMPLES = 5;
static const int WIFI_CONNECT_ATTEMPTS = 5;
static const int SAVED_WIFI_CONNECT_ATTEMPTS = 2;
static const int NETWORK_FAILURES_BEFORE_RECOVERY = 8;
static const int RESULT_QUEUE_SIZE = 8;

// ---- Seguridad ----
#define ENABLE_WATCHDOG 1
static const unsigned long WATCHDOG_TIMEOUT_MS = 30000;

// ---- Diagnostico por USB / Monitor Serie ----
#define ENABLE_SERIAL_STATUS_LOG 0
static const unsigned long SERIAL_STATUS_INTERVAL_MS = 5000;

// ---- Diagnostico remoto hacia la web ----
// Endpoint esperado: POST /arduino/status/{arduinoId}
// Si la web todavia no lo tiene, el firmware no usa esa falla para bloquear cobros.
#define ENABLE_REMOTE_STATUS_LOG 1
