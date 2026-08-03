# CLAUDE.md

Guía para trabajar en este repositorio (firmware ESP32 del proyecto TecnoVend / VendPoint).

## Qué es esto

Firmware para placa **ESP32** que vincula una máquina expendedora con la API de TecnoVend:
consulta pulsos pendientes, los ejecuta (acreditaciones) y reporta el estado de la máquina.
Se compila con el **Arduino IDE** (el sketch principal es `tecnovend-arduino.ino`).

Estructura y detalle de cada módulo: ver [README.md](README.md).

## Reglas mínimas de trabajo

### 1. Documentar siempre lo mínimo

- Todo módulo (`*.h`/`*.cpp`) lleva en su cabecera un comentario de **una línea** que diga qué hace.
- Toda función no obvia lleva un comentario corto explicando el "por qué", no el "qué".
- Si se agrega una constante de configuración en `config.h`, dejar un comentario al lado.
- Mantener actualizada la tabla de estructura del [README.md](README.md) cuando se agregan o
  quitan archivos.

### 2. Versionado Semántico y Proceso de Release (SemVer)

A partir de ahora, todo cambio funcional debe seguir un esquema estricto de versionado semántico (`MAYOR.MENOR.PARCHE` / `MAJOR.MINOR.PATCH`, ej: `1.1.2`):
- **PATCH (Parche/Fix):** Corrección de bugs, mejoras de estabilidad o refactorizaciones internas sin cambios funcionales nuevos (ej: de `1.1.1` a `1.1.2`).
- **MINOR (Menor):** Funcionalidad nueva compatible (ej: agregar soporte para actualización remota de firmware, de `1.1.1` a `1.2.0`).
- **MAJOR (Mayor):** Cambios incompatibles (ej: cambio de protocolo de API que rompe compatibilidad con el servidor anterior, reconfiguración de pines incompatibles, de `1.1.1` a `2.0.0`).

**Flujo obligatorio ante cualquier cambio en el código:**
1. **Evaluación de Versión:** El asistente evaluará qué tipo de cambio se realizó, propondrá la nueva versión y preguntará explícitamente al usuario: *"¿Incrementamos la versión de firmware a X.Y.Z?"*.
2. **Actualización de Versión:** Tras la aprobación del usuario, se actualizará `FW_VERSION` en `config.h` (solo los tres números estrictos, sin sufijos descriptivos, ej: `"1.1.2"`).
3. **Registro en CHANGELOG.md:** Se documentará el cambio bajo la sección de la versión en `CHANGELOG.md` con la fecha actual.
4. **Git Commit & Push:** Se realiza el commit y push de los archivos modificados.
5. **Creación de Release en Git:** El asistente creará un tag anotado en Git y lo subirá para marcar el release oficial:
   ```bash
   git tag -a vX.Y.Z -m "Release vX.Y.Z"
   git push origin vX.Y.Z
   ```

Si un cambio toca la **API** (rutas, parámetros o formato del body/heartbeat), dejarlo
explícito en el CHANGELOG porque debe coordinarse con el servidor.


### 4. Watchdog, Red y Cuelgues de Librería HTTP (APRENDIZAJES Y REGLAS CLAVE)

- **`safety.cpp` y Reconfiguración de Watchdog (30s):** En ESP-IDF v5 (Arduino Core 3.x), la reconfiguración del Watchdog a 30s DEBE hacerse ÚNICAMENTE llamando a `esp_task_wdt_reconfigure(&watchdogConfig)` como en la v0.0.12. NUNCA llamar a `esp_task_wdt_deinit()` ni `esp_task_wdt_delete(NULL)` en `setupWatchdog()`, porque `deinit()` falla en tiempo de ejecución al haber tareas del sistema suscriptas, anulando los 30s y provocando que el SDK revierta al timeout por defecto de 5s.
- **`pauseWatchdog()` / `resumeWatchdog()` alrededor de HTTP (PROHIBIDO):** NO colocar `pauseWatchdog()` alrededor de llamadas HTTP. Se probó en commit `e54e91f` y **falló**, porque si la librería síncrona de Arduino sufre un cuelgue de socket, el ESP32 queda congelado indefinidamente sin que el Watchdog lo reinicie.
- **HTTPS Keep-Alive & `keepAliveClient.stop()` (OBLIGATORIO en fallas `code <= 0`):** La arquitectura reutiliza el cliente HTTPS (`keepAliveClient` + `keepAliveHttp` con `setReuse(true)`). Sin embargo, ante cualquier error (`code <= 0`, ej. `-11` timeout de lectura o `-1`), **ES OBLIGATORIO ejecutar `keepAliveClient.stop()`** además de `keepAliveHttp.end()`. Sin `keepAliveClient.stop()`, el descriptor de socket TCP y el contexto SSL de `mbedtls` quedan en estado viciado en RAM, causando que la siguiente reconexión en `keepAliveHttp.GET()` se bloquee síncronamente durante 30s hasta que salta el `task_wdt`.
- **Recuperación tras error `-11` (`HTTPC_ERROR_READ_TIMEOUT`):** Al detectar error `-11` o `-1`, la placa debe invocar `forceWifiReconnect()` para limpiar el vínculo de red antes del próximo poll.
- **Filtro de SSIDs Placeholders:** En `fetchConfigOnce()`, ignorar SSIDs `user`, `string`, `ssid`, `placeholder`, `null` devueltos por la API para evitar sobrescribir las credenciales Wi-Fi válidas guardadas en NVS.

## Compilar / verificar

No hay `arduino-cli` instalado en el entorno, así que la compilación se valida desde el
Arduino IDE (botón *Verificar*). Si se instala `arduino-cli` + el core ESP32, documentar
aquí el comando exacto de build.
