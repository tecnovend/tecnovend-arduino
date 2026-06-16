# Changelog

Registro de cambios del firmware. Las entradas con cambios **funcionales** deben
acompañarse de una subida de `FW_VERSION` en `config.h` (ver [CLAUDE.md](CLAUDE.md)).

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
