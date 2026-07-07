#include "ota.h"
#include "safety.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

static bool rollbackFlag = false;
bool otaFailedFlag = false;
String otaFailedError = "";

void setupOta() {
  // Verificar si la tabla de particiones tiene soporte para OTA (evita crash si no hay otadata)
  const esp_partition_t* ota_data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (ota_data == NULL) {
    Serial.println("[OTA] La tabla de particiones no tiene 'otadata'. Funciones OTA desactivadas.");
    return;
  }

  // Verificar si la partición de inicio anterior falló y se aplicó rollback
  const esp_partition_t* invalid = esp_ota_get_last_invalid_partition();
  if (invalid != NULL) {
    Serial.println("[OTA] Ultimo inicio fallo. Rollback automatico detectado.");
    rollbackFlag = true;
  }
}

void confirmFwStable() {
  // Verificar si la tabla de particiones tiene soporte para OTA
  const esp_partition_t* ota_data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (ota_data == NULL) return;

  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running == NULL) {
    Serial.println("[OTA] Warning: running partition is NULL.");
    return;
  }
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
      if (err == ESP_OK) {
        Serial.println("[OTA] Firmware verificado. Rollback cancelado con exito.");
      } else {
        Serial.printf("[OTA] Error al cancelar rollback: %d\n", err);
      }
    }
  }
}

void checkOtaTimeout() {
  // Verificar si la tabla de particiones tiene soporte para OTA
  const esp_partition_t* ota_data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (ota_data == NULL) return;

  if (millis() > 180000) { // 3 minutos de gracia para conexion de API
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running != NULL) {
      esp_ota_img_states_t state;
      if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
          Serial.println("[OTA] Critico: 3 minutos sin conexion de API en nuevo firmware. Reiniciando para rollback...");
          delay(1000);
          ESP.restart();
        }
      }
    }
  }
}

bool hasOtaRollbackOccurred() {
  return rollbackFlag;
}

void clearOtaRollbackFlag() {
  rollbackFlag = false;
}

bool performOta(const String& url, const String& targetVersion) {
  Serial.printf("[OTA] Iniciando actualizacion a la version: %s\n", targetVersion.c_str());
  Serial.printf("[OTA] Descargando desde: %s\n", url.c_str());

  // Pausar el watchdog durante el proceso de OTA (evita reseteos por handshake TLS largo o borrado de Flash)
  pauseWatchdog();

  otaFailedFlag = false;
  otaFailedError = "";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30); // timeout de socket alto para descargas

  HTTPClient http;
  http.setTimeout(45000); // 45s de timeout de lectura HTTP
  http.setConnectTimeout(15000); // 15s de conexion HTTP
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Requerido para seguir redirecciones de GitHub a AWS S3

  if (!http.begin(client, url)) {
    Serial.println("[OTA] Error al iniciar HTTPClient");
    otaFailedFlag = true;
    otaFailedError = "HTTPClient begin failed";
    resumeWatchdog();
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Peticion HTTP GET fallo, codigo: %d\n", httpCode);
    otaFailedFlag = true;
    otaFailedError = "HTTP GET failed: " + String(httpCode);
    http.end();
    resumeWatchdog();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("[OTA] Content-Length invalido o desconocido");
    otaFailedFlag = true;
    otaFailedError = "Invalid size: " + String(contentLength);
    http.end();
    resumeWatchdog();
    return false;
  }

  Serial.printf("[OTA] Tamaño del binario: %d bytes\n", contentLength);

  if (!Update.begin(contentLength, U_FLASH)) {
    Serial.printf("[OTA] Error al inicializar Update. Espacio insuficiente o error de particion. Codigo: %d\n", Update.getError());
    otaFailedFlag = true;
    otaFailedError = "Update.begin failed: code " + String(Update.getError());
    http.end();
    resumeWatchdog();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[1024];

  while (http.connected() && written < contentLength) {
    size_t size = stream->available();
    if (size > 0) {
      int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
      size_t w = Update.write(buff, c);
      if (w != c) {
        Serial.printf("[OTA] Error en escritura. Escrito=%d, Esperado=%d\n", w, c);
        otaFailedFlag = true;
        otaFailedError = "Write error at " + String(written) + "/" + String(contentLength);
        http.end();
        resumeWatchdog();
        return false;
      }
      written += w;
    }
    delay(1);
  }

  http.end();

  if (Update.end(true)) {
    Serial.println("[OTA] Descarga completada e integrada con exito.");
    if (Update.isFinished()) {
      Serial.println("[OTA] Actualizacion finalizada. Reiniciando...");
      delay(1000);
      ESP.restart();
      return true; // No deberia llegar aca
    } else {
      Serial.println("[OTA] Update.isFinished() devolvio false");
      otaFailedFlag = true;
      otaFailedError = "Update not finished";
    }
  } else {
    Serial.printf("[OTA] Error al verificar binario (MD5/SHA256). Codigo: %d\n", Update.getError());
    otaFailedFlag = true;
    otaFailedError = "Verification error (MD5/SHA256): code " + String(Update.getError());
  }

  resumeWatchdog();
  return false;
}
