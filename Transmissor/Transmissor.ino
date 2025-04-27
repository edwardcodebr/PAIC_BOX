/* Bibliotecas para o Display OLED */
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <time.h>
#include <unistd.h>

/* Bibliotecas para comunicação LoRa */
#include <LoRa.h>
#include <SPI.h>

/* Biblioteca para acessar o NVS */
#include <Preferences.h>

/* Pinagem para o Display OLED */
#define OLED_SDA 4
#define OLED_SCL 15 
#define OLED_RST 16
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

/* Pinagem LoRa */
#define SCK_LORA           5
#define MISO_LORA          19
#define MOSI_LORA          27
#define RESET_PIN_LORA     14
#define SS_PIN_LORA        18
#define DIO0_LORA          26
#define BAND               915E6

/* Sensor DHT */
#define DHT_PIN 17

/* Objetos */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
Preferences prefs;
DHTesp dht;

void setup() {
  Serial.begin(9600);
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Inicializa display OLED
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha na alocação do display SSD1306"));
    for (;;);
  }

  // Mensagem inicial
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Transmissor LoRa");
  display.display();
  delay(2000);

  // Inicia LoRa
  SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_PIN_LORA);
  LoRa.setPins(SS_PIN_LORA, RESET_PIN_LORA, DIO0_LORA);

  if (!LoRa.begin(BAND)) {
    Serial.println("Falha ao iniciar LoRa");
    while (1);
  }

  LoRa.setSpreadingFactor(8);
  Serial.println("LoRa iniciado com SF8");

  configTime(0, 0, "pool.ntp.org"); // Sincroniza com NTP
  Serial.println("Aguardando sincronização de tempo...");
}

void loop() {
  time_t now = time(nullptr);
  struct tm* pTime = localtime(&now);

  TempAndHumidity dados = dht.getTempAndHumidity();
  String payload = "Temp: " +
                  String(dados.temperature, 1) + " C,\nUmid: " + 
                  String(dados.humidity, 1) + " %";

  display.clearDisplay();

  char horaFormatada[9]; // "HH:MM:SS" + '\0'
  if (pTime != nullptr) {
    sprintf(horaFormatada, "%02d:%02d:%02d", pTime->tm_hour, pTime->tm_min, pTime->tm_sec);
  } else {
    strcpy(horaFormatada, "00:00:00"); // Valor de fallback
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(horaFormatada);
  
  display.setCursor(0, 20);
  display.println("Enviando:");
  
  display.setCursor(0, 35);
  display.println(payload);

  display.display();

  delay(1000);
}

