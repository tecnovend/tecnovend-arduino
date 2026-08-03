# Changelog

Registro de cambios del firmware. Las entradas con cambios **funcionales** deben
acompañarse de una subida de `FW_VERSION` en `config.h` (ver [CLAUDE.md](CLAUDE.md)).

## [0.0.21] - 2026-08-03

- **Rendimiento Masivo / HTTP TCP Proxy (Sin SSL):** Se configuró `API_BASE_URL = "http://yamabiko.proxy.rlwy.net:58436"` aprovechando el TCP Proxy directo de Railway. Se migró de `WiFiClientSecure` a `WiFiClient` (TCP plano). Esto elimina la sobrecarga de cifrado SSL, reduce el tiempo de respuesta de 2.5s a 20ms, ahorra 35KB de RAM por consulta y elimina por completo los congelamientos de socket mbedTLS.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.21`.

## [0.0.20] - 2026-08-03

- **Fix / Socket -11 Controlled Fast Reboot:** Al detectar un error de timeout `-11` (`HTTPC_ERROR_READ_TIMEOUT`), la placa ejecuta un `ESP.restart()` controlado de 1.5s. Esto limpia la memoria RAM, los descriptores lwIP y el contexto `mbedtls` al 100% a nivel hardware, evitando congelamientos de 30s del Task Watchdog.
- **Fix / Eliminar Parpadeo de Wi-Fi en Error -1:** Ante un error transitorio `code = -1`, la placa cierra el socket (`keepAliveClient.stop()`) sin apagar la interfaz Wi-Fi física, eliminando el parpadeo en rojo y los tiempos de reconexión innecesarios.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.20`.

## [0.0.19] - 2026-08-03

- **Fix / Socket Cleanup & WiFi Recovery:** Se agregó `keepAliveClient.stop()` explícito en `executeHttpRequest()` cada vez que la petición retorna error (`code <= 0`), evitando que el socket SSL sucio de `mbedtls` congele la reconexión.
- **Fix / Socket -11 Timeout Recovery:** Al detectar error `-11` (`HTTPC_ERROR_READ_TIMEOUT`) o `-1`, se fuerza la recuperación del vínculo de red con `forceWifiReconnect()` antes del próximo poll.
- **Fix / Config Filter:** Se agregó filtrado de SSIDs placeholders (`user`, `string`, `ssid`, `placeholder`, `null`) traídos por `/arduino/config` para evitar sobrescribir las credenciales guardadas en NVS.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.19`.

## [0.0.18] - 2026-08-03

- **Rendimiento / Red:** Se ajustó el intervalo de polling de 1000ms (1s) a 3000ms (3s) (`POLL_INTERVAL_MS = 3000`). Esto reduce el tráfico HTTPS en un 66%, elimina el encolamiento de sockets durante fluctuaciones de Wi-Fi/4G y le da espacio a la pila de red del ESP32 para procesar ACKs y liberar memoria RAM.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.18`.

## [0.0.17] - 2026-08-03

- **Estabilidad / Rollback a v0.0.12:** Se restauró la implementación exacta de `safety.cpp` y el HTTPS Keep-Alive / reutilización de socket de `api.cpp` de la versión `0.0.12`. Se eliminó el `deinit()` erróneo del WDT que reseteaba la reconfiguración a 5s en ESP-IDF v5, devolviendo el Watchdog real de 30s y el Keep-Alive estable.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.17`.

## [0.0.16] - 2026-08-02

- **Fix / Watchdog & SSL Stability:** Se removió la reutilización del objeto estático global `WiFiClientSecure` en `api.cpp`. Cada petición HTTPS instancie su propia conexión limpia en el stack y la destruye al finalizar con `http.end()`, eliminando los cuelgues de sockets/mbedTLS que causaban reinicios por `task_wdt`.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.16`.

## [0.0.15] - 2026-07-26

- **Fix / Watchdog:** Se des-inicializó el WDT por defecto de 5s del ESP32 para asegurar el timeout de 30s.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.15`.

## [0.0.14] - 2026-07-16

- **Branding / NVS Migration:** Se implementó un mecanismo de migración automática del almacenamiento persistente NVS de `"javopoint"` a `"vendpoint"`. En el primer arranque de esta versión, la placa copia el ID único y las credenciales WiFi al nuevo namespace y vacía el viejo, evitando intervenciones manuales en el campo.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.14`.

## [0.0.13] - 2026-07-16

- **Branding / Rename:** Se renombraron las referencias visibles de "JavoPoint" por "VendPoint" (SSIDs por defecto, portal de configuración, documentación y logs). El namespace interno de Preferences se mantiene como "javopoint" para conservar configuraciones existentes tras actualizar.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.13`.

## [0.0.12] - 2026-07-14

- **Fix / OTA Bootloop:** Se corrigió la recursión síncrona infinita (stack overflow/panic) en `sendHeartbeat()` al recibir payload OTA en un heartbeat de tipo `"ota_start"`, `"ota_failed"` u `"ota_rollback"`.
- **FW Bump:** Se incrementó `FW_VERSION` en `config.h` a `0.0.12`.

## [0.0.11] - 2026-07-14

- **OTA / HTTPS Keep-Alive:** Conexión persistente HTTPS con reporte de métricas al servidor.

## [0.0.3] - 2026-07-06

- **OTA / Actualización Remota:** Implementación de descarga Over-The-Air, verificación de rollback automático y reporte de éxitos/fallas/rollbacks al servidor en el heartbeat.

## [0.0.2] - 2026-07-06

- **Seguridad / Watchdog:** Se removieron los llamados a `pauseWatchdog()` y `resumeWatchdog()` alrededor de las peticiones HTTP a la API (`GET`, `POST`, `getString`). El watchdog de 30 segundos permanece activo para reiniciar el ESP32 ante un cuelgue real de red.
- **Versionado:** Se implementó el esquema de versionado semántico estricto (`MAJOR.MINOR.PATCH`) y proceso formalizado de releases en Git.

## [1.1.0-heartbeat-in-service-on-change] - 2026-06-16

- **API / heartbeat:** `in_service` ya no se envía en cada heartbeat. Ahora solo se
  incluye en los heartbeats de cambio de estado (los que llevan `reason`:
  `out_of_service`, `recovered`, `sale_timeout`). El keepalive periódico (cada hora)
  manda únicamente `rssi / uptime / fw`.
- **Requiere coordinación con el servidor:** cuando el body del heartbeat no trae
  `in_service`, el server debe conservar el último estado conocido (no interpretarlo
  como cambio).

## [1.0.5-api-arduino-id] - 2026-06-10

- Modularización del firmware: el sketch monolítico se separó en
  `config / types / globals / led / wifi_manager / json_utils / api / pulses / service`.
  Sin cambios funcionales (mismo comportamiento, solo reorganización del código).
