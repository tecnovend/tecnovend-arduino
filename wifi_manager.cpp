#include "wifi_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "globals.h"
#include "led.h"
#include "safety.h"

static const byte DNS_PORT = 53;

void wifiDisconnect() {
  WiFi.disconnect(false, true);
}

String wifiConfigPage(const String& message) {
  String html = "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>VendPoint WiFi</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:24px;max-width:520px}";
  html += "label{display:block;margin-top:14px;font-weight:bold}input{width:100%;padding:10px;font-size:16px}";
  html += "button{margin-top:18px;padding:12px 16px;font-size:16px}small{color:#555}</style></head><body>";
  html += "<h2>Configurar WiFi VendPoint</h2>";
  html += "<p><small>Arduino ID: ";
  html += arduinoId;
  html += "</small></p>";
  if (message.length() > 0) {
    html += "<p><b>";
    html += message;
    html += "</b></p>";
  }
  html += "<form method='POST' action='/save'>";
  html += "<label>Nombre de red WiFi del cliente</label><input name='ssid' required maxlength='63'>";
  html += "<label>Clave WiFi</label><input name='pass' type='password' maxlength='95'>";
  html += "<button type='submit'>Guardar y conectar</button>";
  html += "</form><p><small>Si el celular avisa que esta red no tiene internet, elegir mantener conexion.</small></p>";
  html += "</body></html>";
  return html;
}

void applyDefaultWifiConfig(bool skipConfigReload) {
  Serial.println("Reset WiFi solicitado. Borrando red guardada y volviendo a VendPoint.");
  preferences.remove("wifi_ssid");
  preferences.remove("wifi_pass");
  currentWifiSsid = DEFAULT_WIFI_SSID;
  currentWifiPass = DEFAULT_WIFI_PASS;
  usingDefaultWifi = true;
  skipConfigThisBoot = skipConfigReload;
  communicationState = COMM_NO_CONNECTION;
  wifiDisconnect();
}

void loadWifiConfig() {
  // 1. Intentar abrir el nuevo cajón en modo lectura/escritura
  preferences.begin("vendpoint", false);
  arduinoId = preferences.getString("arduino_id", "");
  preferences.end(); // Lo cerramos temporalmente para operar el cajón viejo

  // 2. Si no hay arduino_id en el cajón nuevo, migrar desde el viejo
  if (arduinoId.length() == 0) {
    Serial.println("[NVS] Cajon 'vendpoint' vacio. Buscando cajon antiguo 'javopoint' para migrar...");
    
    preferences.begin("javopoint", false);
    String oldArduinoId = preferences.getString("arduino_id", "");
    String oldSsid = preferences.getString("wifi_ssid", "");
    String oldPass = preferences.getString("wifi_pass", "");
    preferences.end();
    
    if (oldArduinoId.length() > 0) {
      Serial.println("[NVS] Datos encontrados en 'javopoint'. Migrando a 'vendpoint'...");
      
      // Guardar en el nuevo
      preferences.begin("vendpoint", false);
      preferences.putString("arduino_id", oldArduinoId);
      preferences.putString("wifi_ssid", oldSsid);
      preferences.putString("wifi_pass", oldPass);
      preferences.end();
      
      // Limpiar el viejo
      preferences.begin("javopoint", false);
      preferences.clear();
      preferences.end();
      
      arduinoId = oldArduinoId;
      currentWifiSsid = oldSsid;
      currentWifiPass = oldPass;
      Serial.print("[NVS] Migracion exitosa. arduino_id=");
      Serial.println(arduinoId);
    } else {
      Serial.println("[NVS] Cajon antiguo 'javopoint' tambien vacio. Inicializando nuevo...");
      
      // Placa nueva o reseteada: inicializar directamente en el nuevo
      preferences.begin("vendpoint", false);
      preferences.putString("arduino_id", INITIAL_ARDUINO_ID);
      preferences.end();
      
      arduinoId = INITIAL_ARDUINO_ID;
      currentWifiSsid = "";
      currentWifiPass = "";
    }
  } else {
    // Ya estaba en el nuevo, cargamos los valores de red temporales
    preferences.begin("vendpoint", false);
    currentWifiSsid = preferences.getString("wifi_ssid", "");
    currentWifiPass = preferences.getString("wifi_pass", "");
    preferences.end();
    
    Serial.print("arduinoId cargado desde NVS: ");
    Serial.println(arduinoId);
  }

  // 3. Dejar el cajón 'vendpoint' abierto en lectura/escritura para el resto del programa
  preferences.begin("vendpoint", false);

  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    applyDefaultWifiConfig(true);
  }

  usingDefaultWifi = currentWifiSsid.length() == 0;

  if (usingDefaultWifi) {
    currentWifiSsid = DEFAULT_WIFI_SSID;
    currentWifiPass = DEFAULT_WIFI_PASS;
  }
}

bool runWifiConfigPortal() {
  Serial.println("Sin WiFi guardado. Iniciando portal local de configuracion.");
  configPortalActive = true;

  WiFi.mode(WIFI_AP);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(CONFIG_PORTAL_SSID, CONFIG_PORTAL_PASS);

  DNSServer dnsServer;
  WebServer server(80);
  bool saved = false;

  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", HTTP_GET, [&]() {
    server.send(200, "text/html", wifiConfigPage(""));
  });

  server.on("/generate_204", HTTP_GET, [&]() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  });

  server.on("/fwlink", HTTP_GET, [&]() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  });

  server.onNotFound([&]() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  });

  server.on("/save", HTTP_POST, [&]() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();

    if (ssid.length() == 0) {
      server.send(400, "text/html", wifiConfigPage("El nombre de red no puede estar vacio."));
      return;
    }

    Serial.print("Validando WiFi del cliente desde portal: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < WIFI_CONNECT_TIMEOUT_MS) {
      feedWatchdog();
      dnsServer.processNextRequest();
      updateStatusLed();
      delay(100);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi del cliente no conecto. Manteniendo portal abierto.");
      WiFi.disconnect();
      watchdogDelay(300);
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(apIP, gateway, subnet);
      WiFi.softAP(CONFIG_PORTAL_SSID, CONFIG_PORTAL_PASS);
      server.send(200, "text/html", wifiConfigPage("No se pudo conectar a esa red. Revisar nombre, clave y que sea WiFi 2.4 GHz."));
      return;
    }

    Serial.print("WiFi del cliente conectado. IP: ");
    Serial.println(WiFi.localIP());

    currentWifiSsid = ssid;
    currentWifiPass = pass;
    usingDefaultWifi = false;
    skipConfigThisBoot = false;
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", pass);

    server.send(200, "text/html", wifiConfigPage("WiFi conectado y guardado. El equipo va a salir del modo configuracion."));
    saved = true;
  });

  server.begin();

  Serial.print("Conectarse a ");
  Serial.print(CONFIG_PORTAL_SSID);
  Serial.print(" y abrir http://");
  Serial.println(apIP);

  while (!saved) {
    feedWatchdog();
    dnsServer.processNextRequest();
    server.handleClient();
    updateStatusLed();
    delay(10);
  }

  watchdogDelay(1200);
  server.stop();
  configPortalActive = false;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  communicationState = COMM_NO_CONNECTION;
  return true;
}

void resetWifiRadio() {
  wifiDisconnect();
  WiFi.mode(WIFI_OFF);
  watchdogDelay(300);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  watchdogDelay(200);
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
    wifiDisconnect();
    watchdogDelay(500);
  }

  Serial.print("Conectando a WiFi: ");
  Serial.println(currentWifiSsid);

  resetWifiRadio();
  WiFi.begin(currentWifiSsid.c_str(), currentWifiPass.c_str());

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED) {
    watchdogDelay(100);
    Serial.print(".");
    updateStatusLed();
    feedWatchdog();

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

    wifiDisconnect();
    watchdogDelay(500);
  }

  if (!usingDefaultWifi) {
    Serial.println("No conecto a red guardada. Pasando a WiFi estandar.");
    currentWifiSsid = DEFAULT_WIFI_SSID;
    currentWifiPass = DEFAULT_WIFI_PASS;
    usingDefaultWifi = true;
    wifiDisconnect();
    watchdogDelay(500);

    for (int attempt = 1; attempt <= WIFI_CONNECT_ATTEMPTS; attempt++) {
      Serial.print("Intento WiFi estandar ");
      Serial.print(attempt);
      Serial.print("/");
      Serial.println(WIFI_CONNECT_ATTEMPTS);

      if (connectToSelectedWiFi(true)) {
        return true;
      }

      wifiDisconnect();
      watchdogDelay(500);
    }
  }

  communicationState = COMM_NO_CONNECTION;
  return false;
}

void forceWifiReconnect(const char* reason) {
  Serial.print("Recuperacion WiFi/API solicitada: ");
  Serial.println(reason);
  communicationState = COMM_NO_CONNECTION;
  wifiDisconnect();
  watchdogDelay(300);
  resetWifiRadio();
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
    runWifiConfigPortal();
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

  wifiDisconnect();
  watchdogDelay(500);

  if (!connectToSelectedWiFi(true)) {
    Serial.println("La red nueva fallo. Volviendo a la anterior.");
    currentWifiSsid = previousSsid;
    currentWifiPass = previousPass;
    usingDefaultWifi = previousUsingDefault;
    wifiDisconnect();
    watchdogDelay(500);
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
