#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHTesp.h"
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <string.h>
#include "secrets.h"

// Tópico publicacion: datos de sensores
const char *TOPIC_SENSORES  = "weather-station/sensors";
// Tópico suscripcion: comandos para LEDs (P1 / P2)
const char *TOPIC_COMANDOS  = "weather-station/comandos";

const int PIN_LED_ROJO   = 32;
const int PIN_LED_VERDE  = 13;
const int PIN_BOTON_1    = 27;
const int PIN_BOTON_2    = 15;
const int PIN_DHT22      = 23;
const int PIN_I2C_SDA    = 21;
const int PIN_I2C_SCL    = 22;

const uint8_t BMP280_ADDR = 0x76;

const unsigned long INTERVALO_SENSORES_MS = 60000;
const unsigned long DURACION_LED_ROJO_MS  = 3000;
const unsigned long DEBOUNCE_MS           = 200;

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHTesp dht;
Adafruit_BMP280 bmp;

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

bool ledVerdeEncendido = false;
bool btn1EsperandoSoltar = false;
bool btn2EsperandoSoltar = false;
unsigned long ledRojoApagarEn = 0;
unsigned long ultimaLecturaSensores = 0;
unsigned long ultimoDebounceP1 = 0;
unsigned long ultimoDebounceP2 = 0;

void recuperarI2C();
void estabilizarPantalla();

void encenderLedRojoPor(unsigned long duracionMs) {
  digitalWrite(PIN_LED_ROJO, HIGH);
  ledRojoApagarEn = millis() + duracionMs;
}

void toggleLedVerde() {
  ledVerdeEncendido = !ledVerdeEncendido;
  digitalWrite(PIN_LED_VERDE, ledVerdeEncendido ? HIGH : LOW);
}

void activarP1(const char *origen) {
  encenderLedRojoPor(DURACION_LED_ROJO_MS);
  Serial.print(origen);
  Serial.println(": LED rojo encendido por 3 segundos");
}

void activarP2(const char *origen) {
  toggleLedVerde();
  Serial.print(origen);
  Serial.println(ledVerdeEncendido ? ": LED verde ON" : ": LED verde OFF");
}

void actualizarLedRojo() {
  if (ledRojoApagarEn != 0 && millis() >= ledRojoApagarEn) {
    digitalWrite(PIN_LED_ROJO, LOW);
    ledRojoApagarEn = 0;
  }
}

void manejarBotones() {
  bool btn1 = digitalRead(PIN_BOTON_1);
  bool btn2 = digitalRead(PIN_BOTON_2);
  unsigned long ahora = millis();

  if (btn1 == HIGH) {
    btn1EsperandoSoltar = false;
  } else if (!btn1EsperandoSoltar && (ahora - ultimoDebounceP1 >= DEBOUNCE_MS)) {
    btn1EsperandoSoltar = true;
    ultimoDebounceP1 = ahora;
    activarP1("P1");
  }

  if (btn2 == HIGH) {
    btn2EsperandoSoltar = false;
  } else if (!btn2EsperandoSoltar && (ahora - ultimoDebounceP2 >= DEBOUNCE_MS)) {
    btn2EsperandoSoltar = true;
    ultimoDebounceP2 = ahora;
    activarP2("P2");
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  char mensaje[32];
  unsigned int copiar = length;

  if (copiar >= sizeof(mensaje)) {
    copiar = sizeof(mensaje) - 1;
  }

  memcpy(mensaje, payload, copiar);
  mensaje[copiar] = '\0';

  Serial.print("MQTT recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensaje);

  if (strcmp(mensaje, "P1") == 0 || strcmp(mensaje, "LED1") == 0) {
    activarP1("MQTT");
  } else if (strcmp(mensaje, "P2") == 0 || strcmp(mensaje, "LED2") == 0) {
    activarP2("MQTT");
  } else if (strcmp(mensaje, "LED2_ON") == 0) {
    ledVerdeEncendido = true;
    digitalWrite(PIN_LED_VERDE, HIGH);
    Serial.println("MQTT: LED verde ON");
  } else if (strcmp(mensaje, "LED2_OFF") == 0) {
    ledVerdeEncendido = false;
    digitalWrite(PIN_LED_VERDE, LOW);
    Serial.println("MQTT: LED verde OFF");
  } else {
    Serial.println("MQTT: comando no reconocido");
  }
}

void describirErrorMQTT(int rc) {
  Serial.print(" fallo (rc=");
  Serial.print(rc);
  Serial.print(" = ");

  switch (rc) {
    case -4:
      Serial.print("timeout");
      break;
    case -2:
      Serial.print("fallo de red/TLS");
      break;
    case 1:
      Serial.print("protocolo incorrecto");
      break;
    case 2:
      Serial.print("client ID invalido");
      break;
    case 3:
      Serial.print("broker no disponible");
      break;
    case 4:
      Serial.print("usuario o password incorrectos");
      break;
    case 5:
      Serial.print("no autorizado (revisar MQTT_USER y MQTT_PASSWORD)");
      break;
    default:
      Serial.print("error desconocido");
      break;
  }

  Serial.println(")");
}

void imprimirConfigMQTT() {
  Serial.print("MQTT broker: ");
  Serial.println(MQTT_BROKER);
  Serial.print("MQTT puerto: ");
  Serial.println(MQTT_PORT);
  Serial.print("MQTT usuario: ");
  Serial.println(MQTT_USER);
  Serial.print("MQTT password length: ");
  Serial.println(strlen(MQTT_PASSWORD));
  Serial.print("MQTT client id: ");
  Serial.println(MQTT_CLIENT_ID);
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Conectando WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK - IP: ");
    Serial.println(WiFi.localIP());
    WiFi.setSleep(WIFI_PS_NONE);
    estabilizarPantalla();
  } else {
    Serial.println("WiFi: sin conexion (reintento en loop)");
  }
}

void conectarMQTT() {
  if (mqttClient.connected()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  Serial.print("Conectando MQTT...");

  int intentos = 0;
  while (!mqttClient.connected() && intentos < 5) {
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(" OK");
      mqttClient.subscribe(TOPIC_COMANDOS);
      Serial.print("Suscrito a: ");
      Serial.println(TOPIC_COMANDOS);
      return;
    }

    describirErrorMQTT(mqttClient.state());
    Serial.println(" reintento en 5 s");
    delay(5000);
    intentos++;
  }

  if (!mqttClient.connected()) {
    Serial.println("MQTT: sin conexion (reintento en loop)");
  }
}

void mantenerMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();
}

void publicarSensoresMQTT(bool dhtOk, float tempDht, float humedad, bool bmpOk,
                          float tempBmp, float presionHpa) {
  if (!mqttClient.connected()) {
    Serial.println("MQTT: sin conexion, no se publican datos");
    return;
  }

  char payload[160];
  char tempDhtStr[12];
  char humStr[12];
  char tempBmpStr[12];
  char presStr[12];

  if (dhtOk) {
    snprintf(tempDhtStr, sizeof(tempDhtStr), "%.1f", tempDht);
    snprintf(humStr, sizeof(humStr), "%.1f", humedad);
  }
  if (bmpOk) {
    snprintf(tempBmpStr, sizeof(tempBmpStr), "%.1f", tempBmp);
    snprintf(presStr, sizeof(presStr), "%.1f", presionHpa);
  }

  snprintf(
    payload,
    sizeof(payload),
    "{\"temp_dht\":%s,\"humidity\":%s,\"temp_bmp\":%s,\"pressure\":%s}",
    dhtOk ? tempDhtStr : "null",
    dhtOk ? humStr : "null",
    bmpOk ? tempBmpStr : "null",
    bmpOk ? presStr : "null"
  );

  bool ok = mqttClient.publish(TOPIC_SENSORES, payload, true);

  Serial.print("MQTT publicado [");
  Serial.print(TOPIC_SENSORES);
  Serial.print("]: ");
  Serial.println(payload);
  Serial.println(ok ? "Publicacion OK" : "Error al publicar");
}

void esperarBootEsp32() {
  delay(800);
}

void recuperarI2C() {
  Wire.end();
  delay(10);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(50000);
  delay(10);
}

void estabilizarPantalla() {
  recuperarI2C();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  delay(100);
}

void initPantalla() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void rellenarLinea(char destino[17], const char *texto) {
  memset(destino, ' ', 16);
  destino[16] = '\0';

  size_t largo = strlen(texto);
  if (largo > 16) {
    largo = 16;
  }
  memcpy(destino, texto, largo);
}

void escribirPantalla(const char *linea0, const char *linea1) {
  lcd.clear();
  delay(20);

  lcd.setCursor(0, 0);
  lcd.print(linea0);

  lcd.setCursor(0, 1);
  lcd.print(linea1);
}

void mostrarEnLcd(bool dhtOk, float tempDht, float humedad, bool bmpOk, float presionHpa) {
  char linea0[17];
  char linea1[17];
  char tmp[17];

  if (dhtOk) {
    snprintf(tmp, sizeof(tmp), "T:%.1f H:%.0f%%", tempDht, humedad);
  } else {
    snprintf(tmp, sizeof(tmp), "DHT: error");
  }
  rellenarLinea(linea0, tmp);

  if (bmpOk) {
    snprintf(tmp, sizeof(tmp), "P:%.0f hPa", presionHpa);
  } else {
    snprintf(tmp, sizeof(tmp), "P: error");
  }
  rellenarLinea(linea1, tmp);

  Serial.print("LCD > ");
  Serial.print(linea0);
  Serial.print(" | ");
  Serial.println(linea1);

  recuperarI2C();
  escribirPantalla(linea0, linea1);
}

bool leerPresionBmp280(float &temperaturaBmp, float &presionHpa) {
  temperaturaBmp = bmp.readTemperature();
  float presionPa = bmp.readPressure();

  if (presionPa <= 0.0f || isnan(presionPa)) {
    return false;
  }

  presionHpa = presionPa / 100.0f;
  return true;
}

TempAndHumidity leerDht22ConReintentos() {
  TempAndHumidity datos;

  for (int intento = 0; intento < 2; intento++) {
    datos = dht.getTempAndHumidity();
    if (!isnan(datos.temperature) && !isnan(datos.humidity)) {
      return datos;
    }
    delay(300);
  }

  return datos;
}

void leerYMostrarSensores() {
  TempAndHumidity datosDht = leerDht22ConReintentos();
  bool dhtOk = !isnan(datosDht.temperature) && !isnan(datosDht.humidity);

  float tempBmp = 0.0f;
  float presionHpa = 0.0f;
  bool bmpOk = leerPresionBmp280(tempBmp, presionHpa);

  Serial.println("--- Lectura de sensores ---");

  if (dhtOk) {
    Serial.print("DHT22 - Temperatura: ");
    Serial.print(datosDht.temperature, 1);
    Serial.println(" C");
    Serial.print("DHT22 - Humedad: ");
    Serial.print(datosDht.humidity, 1);
    Serial.println(" %");
  } else {
    Serial.print("Error DHT22: ");
    Serial.println(dht.getStatusString());
  }

  if (bmpOk) {
    Serial.print("BMP280 - Temperatura: ");
    Serial.print(tempBmp, 1);
    Serial.println(" C");
    Serial.print("BMP280 - Presion: ");
    Serial.print(presionHpa, 1);
    Serial.println(" hPa");
  } else {
    Serial.println("Error: no se pudo leer el BMP280");
  }

  mostrarEnLcd(dhtOk, datosDht.temperature, datosDht.humidity, bmpOk, presionHpa);
  publicarSensoresMQTT(dhtOk, datosDht.temperature, datosDht.humidity, bmpOk, tempBmp, presionHpa);
}

void setup() {
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_BOTON_1, INPUT_PULLUP);
  pinMode(PIN_BOTON_2, INPUT_PULLUP);

  digitalWrite(PIN_LED_ROJO, LOW);
  digitalWrite(PIN_LED_VERDE, LOW);

  initPantalla();

  Serial.begin(115200);
  esperarBootEsp32();

  Serial.println();
  Serial.println("===== Basic Weather Station =====");

  dht.setup(PIN_DHT22, DHTesp::DHT22);
  delay(2000);

  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("BMP280 no detectado en I2C");
  } else {
    Serial.println("Sensores OK");
  }

  wifiClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);

  conectarWiFi();
  imprimirConfigMQTT();

  Serial.println("P1: LED rojo 3 s | P2: toggle LED verde");
  Serial.println("MQTT comandos: P1, P2, LED1, LED2, LED2_ON, LED2_OFF");

  leerYMostrarSensores();
  ultimaLecturaSensores = millis();

  conectarMQTT();
}

void loop() {
  mantenerMQTT();
  manejarBotones();
  actualizarLedRojo();

  unsigned long ahora = millis();
  if (ahora - ultimaLecturaSensores >= INTERVALO_SENSORES_MS) {
    ultimaLecturaSensores = ahora;
    leerYMostrarSensores();
  }
}
