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
  body += "\"";
  // in_service solo se reporta en heartbeats de cambio de estado (los que llevan reason).
  // El keepalive periodico lo omite y el server conserva el ultimo estado conocido.
  if (heartbeatReason.length() > 0) {
    body += ",\"in_service\":";
    body += machineInService() ? "true" : "false";
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
