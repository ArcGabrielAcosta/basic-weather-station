#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHTesp.h"
#include <Adafruit_BMP280.h>
#include <stdio.h>
#include <string.h>

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

bool ledVerdeEncendido = false;
bool btn1EsperandoSoltar = false;
bool btn2EsperandoSoltar = false;
unsigned long ledRojoApagarEn = 0;
unsigned long ultimaLecturaSensores = 0;
unsigned long ultimoDebounceP1 = 0;
unsigned long ultimoDebounceP2 = 0;

void encenderLedRojoPor(unsigned long duracionMs) {
  digitalWrite(PIN_LED_ROJO, HIGH);
  ledRojoApagarEn = millis() + duracionMs;
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
    encenderLedRojoPor(DURACION_LED_ROJO_MS);
    Serial.println("P1: LED rojo encendido por 3 segundos");
  }

  if (btn2 == HIGH) {
    btn2EsperandoSoltar = false;
  } else if (!btn2EsperandoSoltar && (ahora - ultimoDebounceP2 >= DEBOUNCE_MS)) {
    btn2EsperandoSoltar = true;
    ultimoDebounceP2 = ahora;
    ledVerdeEncendido = !ledVerdeEncendido;
    digitalWrite(PIN_LED_VERDE, ledVerdeEncendido ? HIGH : LOW);
    Serial.println(ledVerdeEncendido ? "P2: LED verde ON" : "P2: LED verde OFF");
  }
}

void initPantalla() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void esperarBootEsp32() {
  // El bootloader de mi ESP32 envia texto a otra velocidad.
  // Con esto esperamos un momento antes de usar Serial para evitar mezclar basura.
  delay(800);
}

void recuperarI2C() {
  Wire.end();
  delay(10);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(50000);
  delay(10);
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
  recuperarI2C();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  delay(30);

  lcd.setCursor(0, 0);
  lcd.print(linea0);

  delay(50);

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
  Serial.println("===== TP Parcial 2 =====");

  dht.setup(PIN_DHT22, DHTesp::DHT22);
  delay(2000);

  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("BMP280 no detectado en I2C");
  } else {
    Serial.println("Sistema listo");
    Serial.println("P1: LED rojo 3 s | P2: toggle LED verde");
  }

  leerYMostrarSensores();
  ultimaLecturaSensores = millis();
}

void loop() {
  manejarBotones();
  actualizarLedRojo();

  unsigned long ahora = millis();
  if (ahora - ultimaLecturaSensores >= INTERVALO_SENSORES_MS) {
    ultimaLecturaSensores = ahora;
    leerYMostrarSensores();
  }
}
