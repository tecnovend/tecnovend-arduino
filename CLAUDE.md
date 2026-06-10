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

### 2. Dejar constancia de cualquier cambio funcional

Un **cambio funcional** es cualquier cosa que altere el comportamiento del firmware:
nuevos endpoints, cambios en pines, tiempos, lógica de pulsos/servicio, estados del LED,
manejo de WiFi, formato de mensajes a la API, etc. (No cuenta refactor puro, formato o comentarios.)

Ante un cambio funcional:

1. **Subir `FW_VERSION`** en `config.h` (formato `MAYOR.MENOR.PARCHE-descripcion`).
2. **Anotar el cambio en `CHANGELOG.md`** (crear el archivo si no existe), bajo una entrada
   con la nueva versión y fecha, describiendo el cambio en una o dos líneas.
3. **Mensaje de commit claro**, en español, describiendo el cambio de comportamiento.

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
