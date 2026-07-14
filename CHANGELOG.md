# Changelog

Registro de cambios del firmware. Las entradas con cambios **funcionales** deben
acompañarse de una subida de `FW_VERSION` en `config.h` (ver [CLAUDE.md](CLAUDE.md)).

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
