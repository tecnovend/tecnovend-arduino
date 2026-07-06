# CLAUDE.md

Guía para trabajar en este repositorio (firmware ESP32 del proyecto TecnoVend / JavoPoint).

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


### 3. No romper lo que ya anda

- No cambiar pines, IDs ni la URL de la API sin que el usuario lo pida explícitamente.
- Secretos reales (API keys, claves WiFi de clientes) no se commitean: van a `secrets.h`
  (ya ignorado por `.gitignore`).

## Compilar / verificar

No hay `arduino-cli` instalado en el entorno, así que la compilación se valida desde el
Arduino IDE (botón *Verificar*). Si se instala `arduino-cli` + el core ESP32, documentar
aquí el comando exacto de build.
