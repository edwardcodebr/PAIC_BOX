#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHTesp.h"
#include <TinyGPSPlus.h>

// Definições de pinos para o Display OLED
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Definições para o DHT11
#define DHT_PIN 17
DHTesp dht;

// Definições para o GPS (usando Serial2 do ESP32)
#define GPS_RX_PIN 1   // RX do GPS no GPIO16
#define GPS_TX_PIN 3   // TX do GPS no GPIO17
HardwareSerial mySerial(2);  // Usando a Serial2 (pode ser Serial1 ou Serial2)

// Instância do GPS
TinyGPSPlus gps;

// Inicialização do display OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// Função para configurar os pinos do GPS e a comunicação serial
void setup() {
  Serial.begin(9600);  // Inicia a comunicação com o Serial Monitor (para debugar)
  mySerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Inicia a comunicação com o GPS

  // Inicializa o Display OLED
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha na inicialização do display SSD1306"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();
  delay(1000);

  // Inicializa o sensor DHT11
  dht.setup(DHT_PIN, DHTesp::DHT11);
  Serial.println("Sistema iniciado!");
}

void loop() {
  // Leitura do DHT11
  TempAndHumidity data = dht.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;

  // Processa os dados do GPS
  while (mySerial.available() > 0) {
    gps.encode(mySerial.read());  // Lê os dados recebidos do GPS
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("DHT11 + GPS");

  // Exibe a temperatura no display
  display.setCursor(0, 15);
  if (!isnan(temperature)) {
    display.print("Temp: ");
    display.print(temperature, 1);
    display.cp437(true);
    display.write(167); // Símbolo de grau
    display.print("C");
  } else {
    display.print("Temp: Erro");
  }

  // Exibe a umidade no display
  display.setCursor(0, 30);
  if (!isnan(humidity)) {
    display.print("Umid: ");
    display.print(humidity, 1);
    display.print(" %");
  } else {
    display.print("Umid: Erro");
  }

  // Exibe Latitude/Longitude no display
  display.setCursor(0, 45);
  if (gps.location.isValid()) {
    double lat1 = gps.location.lat();
    double lon1 = gps.location.lng();

    // Defina uma segunda coordenada para calcular a distância (por exemplo, uma coordenada fixa ou outra leitura de GPS)
    double lat2 = -23.550520;  // Exemplo: coordenada fixa de São Paulo
    double lon2 = -46.633308;  // Exemplo: coordenada fixa de São Paulo

    // Cálculo da distância
    double distance = haversine(lat1, lon1, lat2, lon2);

    display.print("Distancia: ");
    display.print(distance, 2);
    display.print(" km");
  } else {
    display.print("GPS: Sem sinal");
  }

  display.display();

  // Exibe no Serial Monitor
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print("C, Umid: ");
  Serial.print(humidity);
  Serial.print("%");

  if (gps.location.isValid()) {
    Serial.print(", Lat: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", Lon: ");
    Serial.print(gps.location.lng(), 6);

    // Exibe a distância
    double lat1 = gps.location.lat();
    double lon1 = gps.location.lng();
    double lat2 = -23.550520;  // Coordenada fixa de São Paulo
    double lon2 = -46.633308;

    double distance = haversine(lat1, lon1, lat2, lon2);
    Serial.print(", Distancia: ");
    Serial.print(distance, 2);
    Serial.println(" km");
  } else {
    Serial.println(", GPS: Sem sinal");
  }

  delay(2000);  // Aguarda 2 segundos antes de fazer outra leitura
}

// Função para calcular a distância usando a fórmula de Haversine
double haversine(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0; // Raio da Terra em km
  double phi1 = radians(lat1);
  double phi2 = radians(lat2);
  double deltaPhi = radians(lat2 - lat1);
  double deltaLambda = radians(lon2 - lon1);

  double a = sin(deltaPhi / 2) * sin(deltaPhi / 2) + cos(phi1) * cos(phi2) * sin(deltaLambda / 2) * sin(deltaLambda / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c; // Distância em km
}

// Função para converter graus para radianos
double toRadians(double degree) {
    return degree * DEG_TO_RAD;
}

