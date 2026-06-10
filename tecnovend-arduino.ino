#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

// WiFi estandar para instalacion con hotspot del celular.
static const char* DEFAULT_WIFI_SSID = "JavoPoint";
static const char* DEFAULT_WIFI_PASS = "12345678";

// API de pulsos
static const char* API_BASE_URL = "https://tecnovend-api-production.up.railway.app";

// ID alfanumerico grabado en el firmware. El servidor lo vincula con la maquina.
static const char* ARDUINO_ID = "ARD-7F3A9C";
static const char* API_KEY = "";
static const char* FW_VERSION = "1.0.5-api-arduino-id";

#define USE_RGB_LED 1
#ifndef LED_PIN
#define LED_PIN 48
#endif

// INHIBIT: libre/abierto por pullup = en servicio. A GND = fuera de servicio.
static const int INHIBIT_PIN = 14;
static const int PULSE_PIN = 13;
static const int WIFI_RESET_PIN = 12;

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

Preferences preferences;
unsigned long lastPollMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastStateHeartbeatAttemptMs = 0;
unsigned long lastConfigAttemptMs = 0;
bool lastReportedInService = true;
bool reportedInService = true;
bool inhibitLowActive = false;
bool bootInhibitGraceActive = false;
unsigned long inhibitLowSinceMs = 0;
unsigned long wifiResetLowSinceMs = 0;
bool wifiResetLatched = false;
bool skipConfigThisBoot = false;
bool configLoadedThisBoot = false;
int pulseValue = 1;
unsigned long pulseHighMs = 175;
unsigned long pulseLowMs = 250;
String machineId = "";
String currentWifiSsid;
String currentWifiPass;
bool usingDefaultWifi = true;

enum CommunicationState {
  COMM_NO_CONNECTION,
  COMM_API_REACHED,
  COMM_WEB_LINK_OK
};

CommunicationState communicationState = COMM_NO_CONNECTION;

struct Pulse {
  String id;
  int channel;
  int count;
};

struct PulseResult {
  bool pending;
  String id;
  int channel;
  int count;
  String status;
  String reason;
  int attempts;
  unsigned long nextAttemptMs;
};

PulseResult resultQueue[RESULT_QUEUE_SIZE];
Pulse observedSalePulse;
bool hasObservedSalePulse = false;
bool saleInhibitSeen = false;
unsigned long observedSaleStartedMs = 0;
String heartbeatReason = "";
String heartbeatAffectedPulseId = "";

void setLedRgb(uint8_t red, uint8_t green, uint8_t blue) {
#if USE_RGB_LED
  neopixelWrite(LED_PIN, red, green, blue);
#else
  digitalWrite(LED_PIN, (red > 0 || green > 0 || blue > 0) ? HIGH : LOW);
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

void applyDefaultWifiConfig(bool skipConfigReload) {
  Serial.println("Reset WiFi solicitado. Borrando red guardada y volviendo a JavoPoint.");
  preferences.remove("wifi_ssid");
  preferences.remove("wifi_pass");
  currentWifiSsid = DEFAULT_WIFI_SSID;
  currentWifiPass = DEFAULT_WIFI_PASS;
  usingDefaultWifi = true;
  skipConfigThisBoot = skipConfigReload;
  communicationState = COMM_NO_CONNECTION;
  WiFi.disconnect(false, true);
}

bool rawMachineInService() {
  return digitalRead(INHIBIT_PIN) == HIGH;
}

bool machineInService() {
  return reportedInService;
}

bool canAcceptPulse() {
  return rawMachineInService() && reportedInService && !hasObservedSalePulse;
}

void updateStatusLed() {
  if (!machineInService()) {
    setLedRgb(40, 0, 0);  // Rojo fijo: maquina fuera de servicio.
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

void loadWifiConfig() {
  preferences.begin("javopoint", false);

  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    applyDefaultWifiConfig(true);
  }

  currentWifiSsid = preferences.getString("wifi_ssid", "");
  currentWifiPass = preferences.getString("wifi_pass", "");
  usingDefaultWifi = currentWifiSsid.length() == 0;

  if (usingDefaultWifi) {
    currentWifiSsid = DEFAULT_WIFI_SSID;
    currentWifiPass = DEFAULT_WIFI_PASS;
  }
}

void blinkLedRgb(uint8_t red, uint8_t green, uint8_t blue, unsigned long durationMs) {
  unsigned long startMs = millis();
  while (millis() - startMs < durationMs) {
    setLedRgb(red, green, blue);
    delay(250);
    setLedRgb(0, 0, 0);
    delay(250);
  }
}

void showConfigOkLed() {
  setLedRgb(35, 0, 45);  // Violeta: configuracion cargada OK.
  delay(CONFIG_OK_LED_MS);
  setLedRgb(0, 0, 0);
}

void showClientWifiOkLed() {
  setLedRgb(45, 45, 45);  // Blanco: conectado a red WiFi del cliente.
  delay(CLIENT_WIFI_OK_LED_MS);
  setLedRgb(0, 0, 0);
}

void resetWifiRadio() {
  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_OFF);
  delay(300);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  delay(200);
}

bool connectToSelectedWiFi(bool showLedFeedback) {
  if (WiFi.status() == WL_CONNECTED) {
    String connectedSsid = WiFi.SSID();
    if (connectedSsid == currentWifiSsid) {
      return true;
    }

    Serial.print("WiFi conectado a ");
    Serial.print(connectedSsid);
    Serial.print(". Cambiando a ");
    Serial.println(currentWifiSsid);
    WiFi.disconnect(false, true);
    delay(500);
  }

  Serial.print("Conectando a WiFi: ");
  Serial.println(currentWifiSsid);

  resetWifiRadio();
  WiFi.begin(currentWifiSsid.c_str(), currentWifiPass.c_str());

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    updateStatusLed();

    if (millis() - startMs >= WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      Serial.println("No se pudo conectar a internet/WiFi");
      communicationState = COMM_NO_CONNECTION;
      return false;
    }
  }

  Serial.println();
  Serial.print("WiFi conectado. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  int savedAttempts = usingDefaultWifi ? WIFI_CONNECT_ATTEMPTS : SAVED_WIFI_CONNECT_ATTEMPTS;
  for (int attempt = 1; attempt <= savedAttempts; attempt++) {
    Serial.print("Intento WiFi ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.println(savedAttempts);

    if (connectToSelectedWiFi(false)) {
      if (!usingDefaultWifi && WiFi.SSID() == currentWifiSsid) {
        showClientWifiOkLed();
      }
      return true;
    }

    WiFi.disconnect(false, true);
    delay(500);
  }

  if (!usingDefaultWifi) {
    Serial.println("No conecto a red guardada. Pasando a WiFi estandar.");
    currentWifiSsid = DEFAULT_WIFI_SSID;
    currentWifiPass = DEFAULT_WIFI_PASS;
    usingDefaultWifi = true;
    WiFi.disconnect(false, true);
    delay(500);

    for (int attempt = 1; attempt <= WIFI_CONNECT_ATTEMPTS; attempt++) {
      Serial.print("Intento WiFi estandar ");
      Serial.print(attempt);
      Serial.print("/");
      Serial.println(WIFI_CONNECT_ATTEMPTS);

      if (connectToSelectedWiFi(true)) {
        return true;
      }

      WiFi.disconnect(false, true);
      delay(500);
    }
  }

  communicationState = COMM_NO_CONNECTION;
  return false;
}

void checkWifiResetPin() {
  if (digitalRead(WIFI_RESET_PIN) == HIGH) {
    wifiResetLowSinceMs = 0;
    wifiResetLatched = false;
    return;
  }

  unsigned long now = millis();
  if (wifiResetLowSinceMs == 0) {
    wifiResetLowSinceMs = now;
    return;
  }

  if (!wifiResetLatched && now - wifiResetLowSinceMs >= WIFI_RESET_HOLD_MS) {
    wifiResetLatched = true;
    applyDefaultWifiConfig(true);
    connectWiFi();
  }
}

bool saveWifiConfig(const String& ssid, const String& pass) {
  if (ssid.length() == 0) {
    Serial.println("Config WiFi ignorada: SSID vacio");
    return false;
  }

  if (ssid == currentWifiSsid && pass == currentWifiPass && !usingDefaultWifi &&
      WiFi.status() == WL_CONNECTED && WiFi.SSID() == currentWifiSsid) {
    Serial.println("Config WiFi sin cambios");
    return true;
  }

  String previousSsid = currentWifiSsid;
  String previousPass = currentWifiPass;
  bool previousUsingDefault = usingDefaultWifi;

  currentWifiSsid = ssid;
  currentWifiPass = pass;
  usingDefaultWifi = false;

  WiFi.disconnect(false, true);
  delay(500);

  if (!connectToSelectedWiFi(true)) {
    Serial.println("La red nueva fallo. Volviendo a la anterior.");
    currentWifiSsid = previousSsid;
    currentWifiPass = previousPass;
    usingDefaultWifi = previousUsingDefault;
    WiFi.disconnect(false, true);
    delay(500);
    connectWiFi();
    return false;
  }

  Serial.print("Red WiFi nueva activa: ");
  Serial.println(WiFi.SSID());
  if (WiFi.SSID() == currentWifiSsid && currentWifiSsid != DEFAULT_WIFI_SSID) {
    showClientWifiOkLed();
  }

  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_pass", pass);
  Serial.println("Nueva red WiFi guardada");
  return true;
}

String urlEncode(const String& value) {
  String encoded;
  const char* hex = "0123456789ABCDEF";

  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    bool safe = (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~';

    if (safe) {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }

  return encoded;
}

String jsonEscape(const String& value) {
  String escaped;

  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '"' || c == '\\') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += "\\n";
    } else if (c == '\r') {
      escaped += "\\r";
    } else if (c == '\t') {
      escaped += "\\t";
    } else {
      escaped += c;
    }
  }

  return escaped;
}

void addApiKeyIfNeeded(HTTPClient& http) {
  if (strlen(API_KEY) > 0) {
    http.addHeader("X-Api-Key", API_KEY);
  }
}

bool httpGet(const String& url, String& response) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("HTTP GET begin fallo");
    communicationState = COMM_NO_CONNECTION;
    return false;
  }

  addApiKeyIfNeeded(http);

  int code = http.GET();
  if (code <= 0) {
    communicationState = COMM_NO_CONNECTION;
  } else if (code == HTTP_CODE_OK) {
    communicationState = COMM_WEB_LINK_OK;
  } else {
    communicationState = COMM_API_REACHED;
  }

  if (code != HTTP_CODE_OK) {
    Serial.print("GET fallo. HTTP ");
    Serial.println(code);
    http.end();
    return false;
  }

  response = http.getString();
  http.end();
  return true;
}

bool httpPostPulseResult(const Pulse& pulse, const String& status, const String& reason) {
  String url = String(API_BASE_URL) + "/arduino/ack/" + ARDUINO_ID + "/" + urlEncode(pulse.id);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("HTTP ACK begin fallo");
    return false;
  }

  addApiKeyIfNeeded(http);

  int code = http.POST("");
  String body = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    Serial.print("Resultado de pulso fallo. HTTP ");
    Serial.print(code);
    Serial.print(" body=");
    Serial.println(body);
    return false;
  }

  Serial.print("Resultado de pulso OK: ");
  Serial.print(pulse.id);
  Serial.print(" status=");
  Serial.println(status);
  return true;
}

void queuePulseResult(const Pulse& pulse, const String& status, const String& reason) {
  int slot = -1;

  for (int i = 0; i < RESULT_QUEUE_SIZE; i++) {
    if (resultQueue[i].pending && resultQueue[i].id == pulse.id) {
      slot = i;
      break;
    }
    if (!resultQueue[i].pending && slot < 0) {
      slot = i;
    }
  }

  if (slot < 0) {
    Serial.println("Cola de resultados llena. No se pudo guardar resultado pendiente.");
    return;
  }

  resultQueue[slot].pending = true;
  resultQueue[slot].id = pulse.id;
  resultQueue[slot].channel = pulse.channel;
  resultQueue[slot].count = pulse.count;
  resultQueue[slot].status = status;
  resultQueue[slot].reason = reason;
  resultQueue[slot].attempts = 0;
  resultQueue[slot].nextAttemptMs = millis() + RESULT_RETRY_INTERVAL_MS;
}

bool reportPulseResultWithRetries(const Pulse& pulse, const String& status, const String& reason) {
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (httpPostPulseResult(pulse, status, reason)) {
      return true;
    }

    Serial.print("Reintentando resultado de pulso ");
    Serial.print(attempt);
    Serial.println("/3");
    delay(250);
  }

  queuePulseResult(pulse, status, reason);
  return false;
}

void retryPendingPulseResults() {
  unsigned long now = millis();

  for (int i = 0; i < RESULT_QUEUE_SIZE; i++) {
    if (!resultQueue[i].pending || now < resultQueue[i].nextAttemptMs) {
      continue;
    }

    Pulse pulse;
    pulse.id = resultQueue[i].id;
    pulse.channel = resultQueue[i].channel;
    pulse.count = resultQueue[i].count;

    resultQueue[i].attempts++;
    if (httpPostPulseResult(pulse, resultQueue[i].status, resultQueue[i].reason)) {
      resultQueue[i].pending = false;
      continue;
    }

    resultQueue[i].nextAttemptMs = millis() + RESULT_RETRY_INTERVAL_MS;
  }
}

bool sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return false;
  }

  String url = String(API_BASE_URL) + "/arduino/heartbeat/" + ARDUINO_ID;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(HEARTBEAT_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("HTTP heartbeat begin fallo");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  addApiKeyIfNeeded(http);

  String body = "{";
  body += "\"rssi\":";
  body += WiFi.RSSI();
  body += ",\"uptime\":";
  body += millis() / 1000;
  body += ",\"fw\":\"";
  body += FW_VERSION;
  body += "\",\"in_service\":";
  body += machineInService() ? "true" : "false";
  if (heartbeatReason.length() > 0) {
    body += ",\"reason\":\"";
    body += jsonEscape(heartbeatReason);
    body += "\"";
  }
  if (heartbeatAffectedPulseId.length() > 0) {
    body += ",\"affected_pulse_id\":\"";
    body += jsonEscape(heartbeatAffectedPulseId);
    body += "\"";
  }
  body += "}";

  int code = http.POST(body);
  http.end();

  Serial.print("heartbeat -> ");
  Serial.println(code);

  return code >= 200 && code < 300;
}

bool sendServiceHeartbeat(const String& reason, const String& affectedPulseId) {
  heartbeatReason = reason;
  heartbeatAffectedPulseId = affectedPulseId;

  bool ok = sendHeartbeat();

  heartbeatReason = "";
  heartbeatAffectedPulseId = "";

  if (ok) {
    lastReportedInService = reportedInService;
    lastHeartbeatMs = millis();
  }

  return ok;
}

void clearObservedSalePulse() {
  hasObservedSalePulse = false;
  saleInhibitSeen = false;
  observedSalePulse.id = "";
  observedSalePulse.channel = 0;
  observedSalePulse.count = 0;
}

void updateInhibitState() {
  unsigned long now = millis();
  bool rawInService = rawMachineInService();

  if (rawInService) {
    inhibitLowActive = false;
    bootInhibitGraceActive = false;

    if (hasObservedSalePulse && saleInhibitSeen) {
      Serial.print("Venta finalizada normalmente. pulse_id=");
      Serial.println(observedSalePulse.id);
      clearObservedSalePulse();
    }

    if (hasObservedSalePulse && !saleInhibitSeen &&
        now - observedSaleStartedMs >= SALE_START_WAIT_MS) {
      Serial.println("No se detecto ciclo inhibit despues del pulso. Venta observada finalizada.");
      clearObservedSalePulse();
    }

    if (!reportedInService &&
        now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
      reportedInService = true;
      lastStateHeartbeatAttemptMs = now;
      sendServiceHeartbeat("recovered", "");
    }

    if (reportedInService && lastReportedInService != reportedInService &&
        now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
      lastStateHeartbeatAttemptMs = now;
      sendServiceHeartbeat("recovered", "");
    }

    return;
  }

  if (!inhibitLowActive) {
    inhibitLowActive = true;
    inhibitLowSinceMs = now;
  }

  if (hasObservedSalePulse && !saleInhibitSeen) {
    saleInhibitSeen = true;
    inhibitLowSinceMs = now;
    Serial.print("Inhibit activo durante venta. Esperando recuperacion de pulse_id=");
    Serial.println(observedSalePulse.id);
  }

  unsigned long graceMs = INHIBIT_OUT_OF_SERVICE_DEBOUNCE_MS;
  if (saleInhibitSeen) {
    graceMs = SALE_INHIBIT_GRACE_MS;
  } else if (bootInhibitGraceActive) {
    graceMs = BOOT_SERVICE_GRACE_MS;
  }

  if (reportedInService &&
      now - inhibitLowSinceMs >= graceMs &&
      now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
    String reason = saleInhibitSeen ? "sale_timeout" : "out_of_service";
    String affectedPulseId = saleInhibitSeen && hasObservedSalePulse ? observedSalePulse.id : "";

    reportedInService = false;
    lastStateHeartbeatAttemptMs = now;

    if (sendServiceHeartbeat(reason, affectedPulseId) && saleInhibitSeen) {
      Serial.print("Venta afectada informada como fuera de servicio: ");
      Serial.println(affectedPulseId);
      clearObservedSalePulse();
    }
  }

  if (!reportedInService && lastReportedInService != reportedInService &&
      now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
    String reason = saleInhibitSeen ? "sale_timeout" : "out_of_service";
    String affectedPulseId = saleInhibitSeen && hasObservedSalePulse ? observedSalePulse.id : "";
    lastStateHeartbeatAttemptMs = now;
    if (sendServiceHeartbeat(reason, affectedPulseId) && saleInhibitSeen) {
      clearObservedSalePulse();
    }
  }
}

String extractStringField(const String& object, const char* fieldName) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = object.indexOf(key);
  if (keyPos < 0) return "";

  int colon = object.indexOf(':', keyPos + key.length());
  int firstQuote = object.indexOf('"', colon + 1);
  int secondQuote = object.indexOf('"', firstQuote + 1);
  if (colon < 0 || firstQuote < 0 || secondQuote < 0) return "";

  return object.substring(firstQuote + 1, secondQuote);
}

int extractIntField(const String& object, const char* fieldName, int fallback) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = object.indexOf(key);
  if (keyPos < 0) return fallback;

  int colon = object.indexOf(':', keyPos + key.length());
  if (colon < 0) return fallback;

  int start = colon + 1;
  while (start < object.length() && isspace(object[start])) start++;

  int end = start;
  while (end < object.length() && isdigit(object[end])) end++;

  if (end == start) return fallback;
  return object.substring(start, end).toInt();
}

String extractObjectField(const String& json, const char* fieldName) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = json.indexOf(key);
  if (keyPos < 0) return "";

  int colon = json.indexOf(':', keyPos + key.length());
  int objStart = json.indexOf('{', colon + 1);
  if (colon < 0 || objStart < 0) return "";

  int depth = 0;
  for (int i = objStart; i < json.length(); i++) {
    if (json[i] == '{') depth++;
    if (json[i] == '}') depth--;
    if (depth == 0) {
      return json.substring(objStart, i + 1);
    }
  }

  return "";
}

bool fetchConfigOnce() {
  String url = String(API_BASE_URL) + "/arduino/config/" + ARDUINO_ID;
  String response;

  if (!httpGet(url, response)) {
    return false;
  }

  String wifiObject = extractObjectField(response, "wifi");
  String configObject = extractObjectField(response, "config");

  machineId = extractStringField(response, "machine_id");

  String ssid = extractStringField(wifiObject.length() > 0 ? wifiObject : response, "ssid");
  String pass = extractStringField(wifiObject.length() > 0 ? wifiObject : response, "password");

  if (ssid.length() == 0) {
    ssid = extractStringField(response, "wifi_ssid");
  }
  if (pass.length() == 0) {
    pass = extractStringField(response, "wifi_pass");
  }

  if (ssid.length() > 0) {
    saveWifiConfig(ssid, pass);
  }

  pulseValue = extractIntField(configObject.length() > 0 ? configObject : response, "pulse_value", pulseValue);
  if (pulseValue <= 0) {
    pulseValue = 1;
  }

  pulseHighMs = extractIntField(configObject.length() > 0 ? configObject : response, "pulse_duration_ms", pulseHighMs);
  pulseHighMs = extractIntField(configObject.length() > 0 ? configObject : response, "pulse_high_ms", pulseHighMs);
  if (pulseHighMs <= 0) {
    pulseHighMs = 175;
  }

  pulseLowMs = extractIntField(configObject.length() > 0 ? configObject : response, "pulse_gap_ms", pulseLowMs);
  pulseLowMs = extractIntField(configObject.length() > 0 ? configObject : response, "pulse_low_ms", pulseLowMs);
  if (pulseLowMs <= 0) {
    pulseLowMs = 250;
  }

  configLoadedThisBoot = true;
  Serial.print("Config OK. pulse_value=");
  Serial.print(pulseValue);
  Serial.print(" duration_ms=");
  Serial.print(pulseHighMs);
  Serial.print(" gap_ms=");
  Serial.println(pulseLowMs);
  showConfigOkLed();
  return true;
}

int parsePulses(const String& json, Pulse* pulses, int maxPulses) {
  int arrayKey = json.indexOf("\"pending_pulses\"");
  if (arrayKey < 0) return 0;

  int arrayStart = json.indexOf('[', arrayKey);
  int arrayEnd = json.indexOf(']', arrayStart);
  if (arrayStart < 0 || arrayEnd < 0 || arrayEnd <= arrayStart) return 0;

  int count = 0;
  int pos = arrayStart;

  while (count < maxPulses) {
    int objStart = json.indexOf('{', pos);
    if (objStart < 0 || objStart > arrayEnd) break;

    int objEnd = json.indexOf('}', objStart);
    if (objEnd < 0 || objEnd > arrayEnd) break;

    String object = json.substring(objStart, objEnd + 1);

    Pulse pulse;
    pulse.id = extractStringField(object, "pulse_id");
    pulse.channel = extractIntField(object, "channel", 1);
    pulse.count = extractIntField(object, "count", 0);

    if (pulse.id.length() > 0 && pulse.channel > 0 && pulse.count > 0) {
      pulses[count++] = pulse;
    }

    pos = objEnd + 1;
  }

  return count;
}

bool parseBoolField(const String& object, const char* fieldName, bool fallback) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = object.indexOf(key);
  if (keyPos < 0) return fallback;

  int colon = object.indexOf(':', keyPos + key.length());
  if (colon < 0) return fallback;

  int start = colon + 1;
  while (start < object.length() && isspace(object[start])) start++;

  if (object.substring(start, start + 4) == "true") return true;
  if (object.substring(start, start + 5) == "false") return false;
  return fallback;
}

void executePulse(const Pulse& pulse) {
  Serial.print("Ejecutando pulse_id=");
  Serial.print(pulse.id);
  Serial.print(" channel=");
  Serial.print(pulse.channel);
  Serial.print(" count=");
  Serial.println(pulse.count);

  for (int i = 0; i < pulse.count; i++) {
    setLed(true);
    digitalWrite(PULSE_PIN, HIGH);
    delay(pulseHighMs);
    digitalWrite(PULSE_PIN, LOW);
    setLed(false);

    if (i + 1 < pulse.count) {
      delay(pulseLowMs);
    }
  }
}

bool pollOnce() {
  String url = String(API_BASE_URL) + "/arduino/poll/" + ARDUINO_ID;
  String response;

  if (!httpGet(url, response)) {
    return false;
  }

  Pulse pulses[8];
  int pulseCount = parsePulses(response, pulses, 8);

  if (pulseCount == 0) {
    Serial.println("Sin pulsos pendientes");
    return true;
  }

  Serial.print("Pulsos recibidos: ");
  Serial.println(pulseCount);

  for (int i = 0; i < pulseCount; i++) {
    if (!canAcceptPulse()) {
      Serial.print("Pulso recibido pero no ejecutado: maquina ocupada/no disponible: ");
      Serial.println(pulses[i].id);
      continue;
    }

    executePulse(pulses[i]);
    reportPulseResultWithRetries(pulses[i], "executed", "ok");
    observedSalePulse = pulses[i];
    hasObservedSalePulse = true;
    saleInhibitSeen = false;
    observedSaleStartedMs = millis();
    Serial.print("ACK enviado. Observando recuperacion de la maquina para pulse_id=");
    Serial.println(pulses[i].id);
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(INHIBIT_PIN, INPUT_PULLUP);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);
  setupLed();
  loadWifiConfig();
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

  if (!connectWiFi()) {
    updateStatusLed();
    delay(1000);
    return;
  }

  unsigned long now = millis();
  updateInhibitState();
  updateStatusLed();

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
