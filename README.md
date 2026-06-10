# TecnoVend — Firmware JavoPoint (ESP32)

Firmware para placa **ESP32** que conecta una máquina expendedora con la API de TecnoVend.
La placa consulta periódicamente al servidor, ejecuta los **pulsos** pendientes (acreditaciones)
y reporta el estado de la máquina (en servicio / fuera de servicio).

## Estructura del proyecto

```
tecnovend-arduino/
├── tecnovend-arduino.ino   # Sketch principal
├── .gitignore
└── README.md
```

> En el Arduino IDE el sketch principal debe llamarse igual que la carpeta que lo
> contiene. Por eso el archivo es `tecnovend-arduino.ino` dentro de `tecnovend-arduino/`.
> Abrí esa carpeta (o el `.ino` directamente) para compilar y subir.

## Requisitos

- **Arduino IDE** 2.x
- Soporte para placas ESP32 (Boards Manager → *esp32 by Espressif Systems*)
- Placa: una ESP32 con LED RGB integrado (NeoPixel en `LED_PIN 48`, típico en ESP32-S3)

Librerías usadas (incluidas con el core ESP32):
`WiFi`, `HTTPClient`, `WiFiClientSecure`, `Preferences`.

## Configuración

Los parámetros principales están al inicio de `JavoPoint/JavoPoint.ino`:

| Constante            | Descripción                                              |
|----------------------|----------------------------------------------------------|
| `DEFAULT_WIFI_SSID`  | WiFi por defecto (hotspot de instalación: `JavoPoint`)   |
| `DEFAULT_WIFI_PASS`  | Clave del WiFi por defecto                                |
| `API_BASE_URL`       | URL base de la API de pulsos                              |
| `ARDUINO_ID`         | ID único de la placa, vinculado a la máquina en el server |
| `API_KEY`            | API key opcional (header `X-Api-Key`)                    |
| `FW_VERSION`         | Versión de firmware reportada en el heartbeat            |

La red WiFi del cliente se puede guardar de forma persistente (`Preferences`) y se obtiene
desde el endpoint de config del servidor. Manteniendo el pin de reset (`WIFI_RESET_PIN`)
se borra la red guardada y se vuelve al WiFi `JavoPoint`.

## Pines

| Pin              | Uso                                                              |
|------------------|------------------------------------------------------------------|
| `INHIBIT_PIN` 14 | Entrada: libre/pull-up = en servicio; a GND = fuera de servicio  |
| `PULSE_PIN` 13   | Salida: pulso hacia la máquina (acreditación)                    |
| `WIFI_RESET_PIN` 12 | Entrada: mantener para resetear WiFi a valores por defecto    |
| `LED_PIN` 48     | LED RGB de estado                                                |

## LED de estado

| Color                 | Significado                                  |
|-----------------------|----------------------------------------------|
| Rojo fijo             | Máquina fuera de servicio                    |
| Rojo titilando        | Sin conexión                                 |
| Naranja fijo          | Hay internet/API pero sin vínculo OK         |
| Azul titilando        | Vínculo web OK                               |
| Violeta (3 s)         | Configuración cargada correctamente          |
| Blanco (3 s)          | Conectado al WiFi del cliente                |

## Cómo compilar y subir

1. Abrí `tecnovend-arduino.ino` en el Arduino IDE.
2. Seleccioná tu placa ESP32 y el puerto correcto.
3. Compilá y subí (botón *Upload*).
4. Abrí el *Monitor Serie* a `115200` baudios para ver los logs.
