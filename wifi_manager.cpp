#include "wifi_manager.h"
#include <WiFi.h>
#include "config.h"
#include "globals.h"
#include "led.h"

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
