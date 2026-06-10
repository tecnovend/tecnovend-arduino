# Changelog

Registro de cambios del firmware. Las entradas con cambios **funcionales** deben
acompañarse de una subida de `FW_VERSION` en `config.h` (ver [CLAUDE.md](CLAUDE.md)).

## [1.0.5-api-arduino-id] - 2026-06-10

- Modularización del firmware: el sketch monolítico se separó en
  `config / types / globals / led / wifi_manager / json_utils / api / pulses / service`.
  Sin cambios funcionales (mismo comportamiento, solo reorganización del código).
