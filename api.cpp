#include "api.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "globals.h"
#include "json_utils.h"
#include "led.h"
#include "wifi_manager.h"
#include "service.h"
#include "pulses.h"
#include "safety.h"
#include <esp_system.h>

void addApiKeyIfNeeded(HTTPClient& http) {
  if (strlen(API_KEY) > 0) {
    http.addHeader("X-Api-Key", API_KEY);
  }
}

const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt_wdt";
    case ESP_RST_TASK_WDT:
      return "task_wdt";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deepsleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      return "unknown";
  }
}

void markNetworkOk() {
  consecutiveNetworkFailures = 0;
  lastNetworkOkMs = millis();
}

void markNetworkFail(const char* operation) {
  consecutiveNetworkFailures++;
  Serial.print("[NET FAIL] ");
  Serial.print(operation);
  Serial.print(" consecutive=");
  Serial.println(consecutiveNetworkFailures);
}

const char* communicationStateText() {
  switch (communicationState) {
    case COMM_WEB_LINK_OK:
      return "web_link_ok";
    case COMM_API_REACHED:
      return "api_reached";
    case COMM_NO_CONNECTION:
    default:
      return "no_connection";
  }
}

int pendingPulseResultCount() {
  int pending = 0;
  for (int i = 0; i < RESULT_QUEUE_SIZE; i++) {
    if (resultQueue[i].pending) {
      pending++;
    }
  }
  return pending;
}

bool httpGet(const String& url, String& response) {
  currentNetworkOperation = "GET begin";
  feedWatchdog();

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout((HTTP_TIMEOUT_MS / 1000) + 1);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("HTTP GET begin fallo");
    communicationState = COMM_NO_CONNECTION;
    markNetworkFail("GET begin");
    currentNetworkOperation = "idle";
    return false;
  }

  addApiKeyIfNeeded(http);

  currentNetworkOperation = "GET request";
  Serial.print("[HTTP] GET ");
  Serial.println(url);
  pauseWatchdog();
  int code = http.GET();
  resumeWatchdog();
  currentNetworkOperation = "GET response";

  if (code <= 0) {
    communicationState = COMM_NO_CONNECTION;
    markNetworkFail("GET request");
  } else if (code == HTTP_CODE_OK) {
    communicationState = COMM_WEB_LINK_OK;
    markNetworkOk();
  } else {
    communicationState = COMM_API_REACHED;
    markNetworkFail("GET http");
  }

  if (code != HTTP_CODE_OK) {
    Serial.print("GET fallo. HTTP ");
    Serial.println(code);
    http.end();
    currentNetworkOperation = "idle";
    return false;
  }

  currentNetworkOperation = "GET body";
  pauseWatchdog();
  response = http.getString();
  resumeWatchdog();
  http.end();
  currentNetworkOperation = "idle";
  return true;
}

bool httpPostPulseResult(const Pulse& pulse, const String& status, const String& reason) {
  currentNetworkOperation = "ACK begin";
  feedWatchdog();

  String url = String(API_BASE_URL) + "/arduino/ack/" + ARDUINO_ID + "/" + urlEncode(pulse.id);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout((HTTP_TIMEOUT_MS / 1000) + 1);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("HTTP ACK begin fallo");
    markNetworkFail("ACK begin");
    currentNetworkOperation = "idle";
    return false;
  }

  addApiKeyIfNeeded(http);

  currentNetworkOperation = "ACK post";
  Serial.print("[HTTP] ACK ");
  Serial.println(pulse.id);
  pauseWatchdog();
  int code = http.POST("");
  String body = http.getString();
  resumeWatchdog();
  http.end();
  currentNetworkOperation = "idle";

  if (code < 200 || code >= 300) {
    markNetworkFail("ACK post");
    Serial.print("Resultado de pulso fallo. HTTP ");
    Serial.print(code);
    Serial.print(" body=");
    Serial.println(body);
    return false;
  }

  markNetworkOk();
  Serial.print("Resultado de pulso OK: ");
  Serial.print(pulse.id);
  Serial.print(" status=");
  Serial.println(status);
  return true;
}

bool sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    markNetworkFail("heartbeat wifi");
    return false;
  }

  currentNetworkOperation = "heartbeat begin";
  feedWatchdog();

  String url = String(API_BASE_URL) + "/arduino/heartbeat/" + ARDUINO_ID;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout((HEARTBEAT_TIMEOUT_MS / 1000) + 1);

  HTTPClient http;
  http.setTimeout(HEARTBEAT_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("HTTP heartbeat begin fallo");
    markNetworkFail("heartbeat begin");
    currentNetworkOperation = "idle";
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
  body += ",\"raw_inhibit\":\"";
  body += rawMachineInService() ? "service" : "out_of_service";
  body += "\"";
  body += ",\"reset_reason\":";
  body += (int)esp_reset_reason();
  body += ",\"reset_reason_text\":\"";
  body += resetReasonText();
  body += "\"";
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

  currentNetworkOperation = "heartbeat post";
  Serial.println("[HTTP] heartbeat");
  pauseWatchdog();
  int code = http.POST(body);
  resumeWatchdog();
  http.end();
  currentNetworkOperation = "idle";

  Serial.print("heartbeat -> ");
  Serial.println(code);

  bool ok = code >= 200 && code < 300;
  if (ok) {
    markNetworkOk();
  } else {
    markNetworkFail("heartbeat post");
  }
  return ok;
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

bool sendRemoteStatusLog() {
#if !ENABLE_REMOTE_STATUS_LOG
  return false;
#else
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  currentNetworkOperation = "status_log begin";
  feedWatchdog();

  String url = String(API_BASE_URL) + "/arduino/status/" + ARDUINO_ID;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout((HEARTBEAT_TIMEOUT_MS / 1000) + 1);

  HTTPClient http;
  http.setTimeout(HEARTBEAT_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    Serial.println("HTTP status log begin fallo");
    currentNetworkOperation = "idle";
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  addApiKeyIfNeeded(http);

  unsigned long now = millis();
  String body = "{";
  body += "\"fw\":\"";
  body += FW_VERSION;
  body += "\",\"uptime\":";
  body += now / 1000;
  body += ",\"rssi\":";
  body += WiFi.RSSI();
  body += ",\"ssid\":\"";
  body += jsonEscape(WiFi.SSID());
  body += "\",\"wifi_status\":";
  body += (int)WiFi.status();
  body += ",\"in_service\":";
  body += machineInService() ? "true" : "false";
  body += ",\"raw_inhibit\":\"";
  body += rawMachineInService() ? "service" : "out_of_service";
  body += "\",\"communication_state\":\"";
  body += communicationStateText();
  body += "\",\"consecutive_network_failures\":";
  body += consecutiveNetworkFailures;
  body += ",\"seconds_since_network_ok\":";
  body += (now >= lastNetworkOkMs) ? ((now - lastNetworkOkMs) / 1000) : 0;
  body += ",\"pending_ack\":";
  body += pendingPulseResultCount();
  body += ",\"config_loaded\":";
  body += configLoadedThisBoot ? "true" : "false";
  body += ",\"startup_sent\":";
  body += startupHeartbeatSent ? "true" : "false";
  body += ",\"free_heap\":";
  body += ESP.getFreeHeap();
  body += ",\"reset_reason\":";
  body += (int)esp_reset_reason();
  body += ",\"reset_reason_text\":\"";
  body += resetReasonText();
  body += "\",\"netop\":\"";
  body += jsonEscape(currentNetworkOperation);
  body += "\"}";

  currentNetworkOperation = "status_log post";
  Serial.println("[HTTP] status log");
  pauseWatchdog();
  int code = http.POST(body);
  resumeWatchdog();
  http.end();
  currentNetworkOperation = "idle";

  Serial.print("status log -> ");
  Serial.println(code);

  if (code >= 200 && code < 300) {
    markNetworkOk();
    return true;
  }

  return false;
#endif
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
