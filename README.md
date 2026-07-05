# Basic Weather Station

Estación meteorológica basada en **ESP32** que lee temperatura y humedad (DHT22), presión atmosférica (BMP/BME280), muestra los datos en un **LCD I2C 16×2** y los publica por **MQTT** a un broker **HiveMQ Cloud**. También permite controlar dos **LEDs** de forma local (pulsadores) o remota (MQTT).

Proyecto desarrollado como trabajo práctico de IoT.

---

## Tabla de contenidos

- [Funcionalidades](#funcionalidades)
- [Componentes de hardware](#componentes-de-hardware)
- [Conexiones (cableado)](#conexiones-cableado)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Requisitos de software](#requisitos-de-software)
- [Configuración de credenciales (.env)](#configuración-de-credenciales-env)
- [Flujo de trabajo para compilar y subir](#flujo-de-trabajo-para-compilar-y-subir)
- [Configuración de Arduino IDE](#configuración-de-arduino-ide)
- [Uso del dispositivo](#uso-del-dispositivo)
- [Comunicación MQTT (HiveMQ Cloud)](#comunicación-mqtt-hivemq-cloud)
- [Solución de problemas](#solución-de-problemas)

---

## Funcionalidades

| Área | Descripción |
|------|-------------|
| **Sensores** | Lectura de temperatura/humedad (DHT22) y temperatura/presión (BMP280) cada **60 segundos** |
| **Display** | LCD I2C muestra `T`, `H` y `P` (presión en hPa) |
| **Actuadores locales** | P1 enciende LED rojo 3 s; P2 hace toggle del LED verde |
| **MQTT publish** | Publica JSON con lecturas en `weather-station/sensors` |
| **MQTT subscribe** | Recibe comandos en `weather-station/comandos` para controlar los LEDs |
| **Serial** | Monitor serial a 115200 baud con logs de sensores, LCD y MQTT |

---

## Componentes de hardware

| Componente | Notas |
|------------|-------|
| ESP32 DevKit (clásico) | Placa principal |
| DHT22 | Sensor de temperatura y humedad |
| GY-BME280 / BMP280 | Sensor de presión (I2C, dirección `0x76`) |
| LCD 16×2 con módulo I2C (PCF8574) | Dirección I2C `0x27` |
| 2× LED (rojo y verde) | Con resistencias limitadoras (~220 Ω) |
| 2× pulsadores (P1, P2) | Con resistencias pull-up externas de 10 kΩ (o `INPUT_PULLUP` interno) |
| Cables jumper | Verificar que estén en buen estado |
| Fuente USB | Alimentación del ESP32 |

---

## Conexiones (cableado)

### GPIO del ESP32

| Señal | GPIO ESP32 | Destino |
|-------|------------|---------|
| LED rojo | **32** | Ánodo del LED (cátodo → GND vía resistencia) |
| LED verde | **13** | Ánodo del LED (cátodo → GND vía resistencia) |
| Pulsador P1 | **27** | Un terminal a GPIO, otro a GND (pull-up activo en LOW) |
| Pulsador P2 | **15** | Idem P1 |
| DHT22 data | **23** | Pin data del DHT22 |
| I2C SDA | **21** | SDA del LCD y del BMP280 |
| I2C SCL | **22** | SCL del LCD y del BMP280 |

### Alimentación y bus I2C compartido

- **DHT22:** `VCC` → 3.3 V, `GND` → GND, `DATA` → GPIO 23.
- **BMP/BME280:** `VCC` → 3.3 V, `GND` → GND, `SDA` → GPIO 21, `SCL` → GPIO 22.
  - Para dirección I2C `0x76`: `CSB` → 3.3 V, `SDO` → GND.
- **LCD I2C:** `VCC` → 5 V o 3.3 V (según módulo), `GND` → GND, `SDA`/`SCL` → GPIO 21/22.

> El LCD y el sensor de presión comparten el mismo bus I2C. Mantener cables cortos y firmes reduce errores de lectura/display.

### Esquema simplificado

```
ESP32 DevKit
├── GPIO 32 ──[220Ω]── LED rojo ── GND
├── GPIO 13 ──[220Ω]── LED verde ── GND
├── GPIO 27 ── P1 ── GND
├── GPIO 15 ── P2 ── GND
├── GPIO 23 ── DHT22 (data)
├── GPIO 21 ──┬── LCD SDA
│             └── BMP280 SDA
└── GPIO 22 ──┬── LCD SCL
              └── BMP280 SCL
```

---

## Estructura del repositorio

```
basic-weather-station/
├── basic-weather-station.ino   # Sketch principal
├── .env.example                # Plantilla de credenciales
├── .env                        # Crear este archivo para tus credenciales reales
├── secrets.h                   # Generado por script
├── .gitignore
├── scripts/
│   └── generate_secrets.py     # Convierte .env → secrets.h
└── README.md
```

### ¿Por qué `.env` y no credenciales en el `.ino`?

En proyectos web/backend, `.env` se lee en runtime. En **Arduino/ESP32**, las credenciales se **incrustan al compilar** el firmware. El flujo de este proyecto replica la experiencia de `.env` en tu PC:

```
.env  →  generate_secrets.py  →  secrets.h  →  compilación  →  ESP32
```

Así las claves no se suben al repositorio, pero sí quedan dentro del binario flasheado en la placa (limitación normal de IoT embebido).

---

## Requisitos de software

### Arduino IDE

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x
- Core **esp32** by Espressif (Board Manager)
- **Python 3** (para generar `secrets.h`)

### Librerías Arduino (Library Manager)

| Librería | Autor / uso |
|----------|-------------|
| **LiquidCrystal I2C** | Frank de Brabander |
| **DHTesp** | beegee-tokyo |
| **Adafruit BMP280 Library** | Adafruit |
| **Adafruit Unified Sensor** | Adafruit (dependencia) |
| **Adafruit BusIO** | Adafruit (dependencia) |
| **PubSubClient** | Nick O'Leary |

`WiFi` y `WiFiClientSecure` vienen incluidas con el core ESP32.

---

## Configuración de credenciales (.env)

### 1. Crear el archivo `.env`

```bash
cp .env.example .env
```

### 2. Completar variables en `.env`

```env
WIFI_SSID=nombre_de_tu_red
WIFI_PASSWORD=password_wifi

MQTT_BROKER=tu-cluster.s1.eu.hivemq.cloud
MQTT_PORT=8883
MQTT_USER=usuario_de_hivemq_credentials
MQTT_PASSWORD=password_generada_por_hivemq
MQTT_CLIENT_ID=basic-weather-station-esp32
```

#### Dónde obtener cada valor de HiveMQ Cloud

| Variable | Origen en HiveMQ Cloud |
|----------|------------------------|
| `MQTT_BROKER` | Cluster → **Cluster Details** → URL/host (sin `mqtts://`) |
| `MQTT_PORT` | **8883** (TLS) |
| `MQTT_USER` | **Access Management** → **Credentials** |
| `MQTT_PASSWORD` | **Access Management** → **Credentials** |
| `MQTT_CLIENT_ID` | Lo elegís vos (único por dispositivo; no viene del panel) |

> Si la password contiene `#`, `=` o espacios, encerrala entre comillas:
> `MQTT_PASSWORD="mi-clave#rara="`

### 3. Generar `secrets.h`

```bash
python3 scripts/generate_secrets.py
```

Salida esperada:

```
OK: .../secrets.h generado desde .../.env
```

**Importante:** cada vez que modifiques `.env`, volvé a ejecutar el script **antes** de compilar/subir.

---

## Flujo de trabajo para compilar y subir

Orden recomendado en cada sesión de desarrollo:

```
1. Editar .env (si cambiaron credenciales)
        ↓
2. python3 scripts/generate_secrets.py
        ↓
3. Abrir basic-weather-station.ino en Arduino IDE
        ↓
4. Verificar placa, puerto y librerías
        ↓
5. Compilar y subir (Upload)
        ↓
6. Abrir Serial Monitor (115200 baud)
        ↓
7. (Opcional) Probar MQTT desde HiveMQ Web Client
```

### Checklist rápido antes del upload

- [ ] Existe `secrets.h` (generado recientemente)
- [ ] Placa: **ESP32 Dev Module**
- [ ] Puerto correcto (ej. `/dev/ttyUSB0` en Linux)
- [ ] ESP32 conectado por USB
- [ ] Red WiFi de 2.4 GHz accesible (ESP32 no usa 5 GHz)

---

## Configuración de Arduino IDE

| Opción | Valor |
|--------|-------|
| Board | **ESP32 Dev Module** |
| Upload Speed | 921600 (o 115200 si falla) |
| CPU Frequency | 240 MHz |
| Flash Size | Según tu placa (ej. 4 MB) |
| Port | `/dev/ttyUSB0` (Linux) / `COMx` (Windows) |
| Monitor Speed | **115200** |
| Core Debug Level | None |

> Al arrancar, el ESP32 puede imprimir caracteres basura en Serial (bootloader a 74880 baud). Ignorarlos hasta ver `===== Basic Weather Station =====`.

---

## Uso del dispositivo

### Pantalla LCD

Se actualiza cada 60 segundos con formato similar a:

```
T:21.8 H:55%
P:1033 hPa
```

### Pulsadores

| Botón | Acción |
|-------|--------|
| **P1** | Enciende el **LED rojo** durante **3 segundos** |
| **P2** | Alterna el **LED verde** (ON ↔ OFF) |

Los pulsadores están configurados como **activos en LOW** (`INPUT_PULLUP`).

### Monitor Serial

Muestra:

- Estado de WiFi y MQTT
- Lecturas de sensores
- Contenido enviado al LCD
- Mensajes MQTT recibidos y publicados

---

## Comunicación MQTT (HiveMQ Cloud)

### Tópicos

| Tópico | Dirección | Contenido |
|--------|-----------|-----------|
| `weather-station/sensors` | ESP32 → broker | JSON con lecturas de sensores |
| `weather-station/comandos` | broker → ESP32 | Comandos de control de LEDs |

### Payload de sensores (publicación)

Publicado cada 60 s (retained):

```json
{
  "temp_dht": 21.8,
  "humidity": 54.9,
  "temp_bmp": 20.7,
  "pressure": 1033.1
}
```

Si un sensor falla, su campo aparece como `null`.

### Comandos para controlar LEDs (suscripción)

Publicar en **`weather-station/comandos`** con mensaje de **texto plano** (sin comillas JSON):

| Mensaje | Acción |
|---------|--------|
| `P1` o `LED1` | LED rojo encendido 3 s |
| `P2` o `LED2` | Toggle LED verde |
| `LED2_ON` | Enciende LED verde |
| `LED2_OFF` | Apaga LED verde |

### Uso con HiveMQ Web Client

1. Entrar al cluster en [HiveMQ Cloud](https://www.hivemq.com/mqtt-cloud/).
2. Abrir **Web Client**.
3. Conectar con las mismas credenciales del `.env` (TLS, puerto **8883**).
4. **Ver sensores:** Subscribe → `weather-station/sensors`
5. **Controlar LEDs:** Publish → topic `weather-station/comandos`, message `P1` (o `P2`, etc.)

### Verificación en Serial

Al publicar un comando deberías ver:

```
MQTT recibido [weather-station/comandos]: P1
MQTT: LED rojo encendido por 3 segundos
```

---

## Solución de problemas

### MQTT falla con `rc=5`

Significa **no autorizado**: usuario o password incorrectos.

- Verificar credenciales en HiveMQ → **Access Management** → **Credentials**
- No usar el email de login de HiveMQ como `MQTT_USER`
- Regenerar `secrets.h` después de corregir `.env`
- Revisar en Serial la línea `MQTT password length` vs la longitud real de la clave

### WiFi no conecta

- Confirmar red **2.4 GHz**
- Revisar `WIFI_SSID` y `WIFI_PASSWORD` en `.env`
- Regenerar `secrets.h` y volver a subir

### LCD muestra símbolos raros

Suele ocurrir por interferencia del WiFi sobre el bus I2C compartido.

**Sin reiniciar el ESP32:**

1. Esperar la próxima lectura (60 s) — a veces se corrige solo.
2. **Resetear solo el LCD:** desconectar su VCC 1 segundo y reconectar (el ESP32 sigue con MQTT activo).

### BMP280 no detectado

- Revisar cableado SDA/SCL (GPIO 21/22)
- Confirmar dirección I2C `0x76` (CSB→3.3 V, SDO→GND)
- Probar con scanner I2C si persiste el error

### DHT22 devuelve error

- Esperar ~2 s tras el arranque (el sketch ya incluye delay inicial)
- Revisar conexión en GPIO 23
- Evitar cables muy largos sin pull-up adecuado

### Upload falla / puerto no encontrado

- Linux: probar `/dev/ttyUSB0` (CP210x) vs `/dev/ttyACM0`
- Mantener presionado **BOOT** al iniciar upload si la placa lo requiere
- Verificar permisos USB (`dialout` en Linux)

---

## Licencia

Proyecto académico / educativo. Ajustar licencia según criterio del autor del repositorio.
