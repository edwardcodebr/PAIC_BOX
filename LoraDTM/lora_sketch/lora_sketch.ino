#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include "MQ135.h" 
#include <DHTesp.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==========================================================================
// === CONFIGURAÇÕES DO NÓ - MODIFIQUE ESTAS PARA CADA DISPOSITIVO ===
// ==========================================================================
#define NODE_ID "barcoAM01"
const char* AP_SSID = "ESP32_Node_AP";
const char* AP_PASSWORD = "uea12345";

const char* LINUX_SERVER_HOST = "192.168.4.2"; 
uint16_t LINUX_SERVER_PORT = 5000;

// === OLED ===
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 2 // <<< MUDADO PARA EVITAR CONFLITO COM GPS (era 16)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// === Pinos Sensores ===
#define MQ135_PIN 36 
#define MQ7_PIN   39 
#define DHT_PIN   27 // <<< MUDADO PARA EVITAR CONFLITO COM GPS (era 17)

// Pinos para UART2 do ESP32 para o GPS.
#define GPS_RX_ESP32_PIN    17  // Conectar ao TXD do módulo GPS
#define GPS_TX_ESP32_PIN    16  // Conectar ao RXD do módulo GPS

// === Parâmetros ===
#define INITIAL_WARMUP_DELAY_MS (30 * 1000) 
#define DATA_COLLECTION_INTERVAL_MS (1 * 1000) 

MQ135 mq135(MQ135_PIN);
DHTesp dht;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // Usando UART2 para o GPS

unsigned long lastDataCollectionTime = 0;
int local_message_counter = 0;
bool lastSendSuccess = true; 

void displayMessage(String l1, String l2 = "", bool clear = true, int delayMs = 0) {
  if (clear) display.clearDisplay();
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE); 
  display.setCursor(0, 0);
  display.println(l1);
  if (l2 != "") display.println(l2);
  display.display();
  if (delayMs > 0) delay(delayMs);
}

void setupWiFiAP() {
  Serial.println(F("Configurando AP WiFi..."));
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println(F("AP WiFi configurado com sucesso."));
    IPAddress IP = WiFi.softAPIP(); 
    Serial.print(F("IP do ESP32 (AP): ")); Serial.println(IP);
    Serial.print(F("SSID do AP: ")); Serial.println(AP_SSID);
    Serial.println(F("Conecte seu PC/Servidor a esta rede."));
    Serial.print(F("O ESP32 enviará dados para: http://"));
    Serial.print(LINUX_SERVER_HOST); Serial.print(":"); Serial.println(LINUX_SERVER_PORT);

    // Tenta mostrar no display APENAS SE o display.begin() tiver funcionado
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Checa novamente antes de usar
        displayMessage("Modo AP WiFi", "SSID: " + String(AP_SSID).substring(0,12), true, 1000);
        displayMessage("ESP32 IP: " + IP.toString(), "Srv: " + String(LINUX_SERVER_HOST), false, 3000);
    }
  } else {
    Serial.println(F("Falha ao configurar AP WiFi."));
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Checa novamente antes de usar
        displayMessage("Erro WiFi AP!", "", true, 5000);
    }
  }
}

bool sendDataToLinux(const String& dataLine) {
  HTTPClient http;
  String serverURL = String("http://") + LINUX_SERVER_HOST + ":" + LINUX_SERVER_PORT + "/upload";
  
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  http.setConnectTimeout(5000); 
  http.setTimeout(5000);       

  StaticJsonDocument<350> doc; 
  doc["node_id"] = NODE_ID;
  doc["data"] = dataLine;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);
  
  bool success = false;
  if (httpResponseCode > 0) {
    Serial.print(F("HTTP Response code: ")); Serial.println(httpResponseCode);
    if (httpResponseCode == 200) {
        success = true;
    }
  } else {
    Serial.print(F("Erro no envio HTTP: "));
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
  return success;
}

void displaySensorData(const String& nodeId, int counter, bool sendStatus,
                       float temp, float hum, float co2, float co,
                       const String& lat, const String& lon, const String& dateTime) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print(nodeId.substring(0,9)); 
    display.print(F(" C:")); display.print(counter);
    display.setCursor(SCREEN_WIDTH - 18, 0); 
    display.print(sendStatus ? F("OK") : F("FL"));

    display.setCursor(0, 8);
    display.print(F("T:")); 
    display.print(isnan(temp) ? "N/A" : String(temp, 1)); display.print(F("C"));
    display.print(F(" H:")); 
    display.print(isnan(hum) ? "N/A" : String(hum, 0)); display.print(F("%"));

    display.setCursor(0, 16);
    display.print(F("CO2:")); display.print(String(co2, 0)); 
    display.print(F(" CO:")); display.print(String(co, 0));  
    
    display.setCursor(0, 24);
    display.print(F("Lat: ")); display.print(lat.substring(0,10)); 

    display.setCursor(0, 32);
    display.print(F("Lon: ")); display.print(lon.substring(0,10)); 

    display.setCursor(0, 40);
    display.print(F("UTC: "));
    if (dateTime != "N/A" && dateTime.length() >= 23) { 
        display.print(dateTime.substring(11, 23)); 
    } else {
        display.print("N/A");
    }

    display.setCursor(0, 48);
    display.print(F("AP: ")); display.print(String(AP_SSID).substring(0,15));

    display.setCursor(0, 56);
    display.print(F("IP: ")); display.print(WiFi.softAPIP().toString());
    
    display.display();
}

bool oledInitialized = false; // Flag para saber se o OLED iniciou

void setup() {
  Serial.begin(115200);
  while(!Serial && millis() < 3000); 
  Serial.println(F("\nIniciando Setup do ESP32..."));

  Serial.println(F("Configurando pino OLED_RST..."));
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW); delay(20); digitalWrite(OLED_RST, HIGH); delay(100); // Aumentar delay após RST HIGH

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Tenta o endereço 0x3C
    Serial.println(F("Falha ao iniciar SSD1306 com 0x3C. Tentando 0x3D..."));
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Tenta o endereço 0x3D
        Serial.println(F("Falha ao iniciar SSD1306 com 0x3D também. Verifique conexões. OLED DESATIVADO."));
        oledInitialized = false;
        // Não para o programa, apenas o OLED não funcionará
    } else {
        Serial.println(F("OLED OK com 0x3D."));
        oledInitialized = true;
    }
  } else {
    Serial.println(F("OLED OK com 0x3C."));
    oledInitialized = true;
  }
  
  if (oledInitialized) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    displayMessage("OLED OK", "", true, 1000);
  }

  Serial.println(F("Pinos dos Sensores e GPS:"));
  Serial.print(F("  OLED_RST: ")); Serial.println(OLED_RST);
  Serial.print(F("  DHT_PIN:  ")); Serial.println(DHT_PIN);
  Serial.print(F("  MQ135_PIN:")); Serial.println(MQ135_PIN);
  Serial.print(F("  MQ7_PIN:  ")); Serial.println(MQ7_PIN);
  Serial.print(F("  GPS_RX_ESP32_PIN (UART2 RX): ")); Serial.println(GPS_RX_ESP32_PIN);
  Serial.print(F("  GPS_TX_ESP32_PIN (UART2 TX): ")); Serial.println(GPS_TX_ESP32_PIN);

  if (oledInitialized) {
    displayMessage("Aquecendo...", String(INITIAL_WARMUP_DELAY_MS / 1000) + "s", true, 500);
  }
  Serial.print(F("Aquecendo sensores por ")); Serial.print(INITIAL_WARMUP_DELAY_MS / 1000); Serial.println(F(" segundos..."));
  unsigned long warmupStartTime = millis();
  while(millis() - warmupStartTime < INITIAL_WARMUP_DELAY_MS){
    if (oledInitialized) {
        display.clearDisplay(); 
        display.setCursor(0,0);
        display.println(F("Aquecendo..."));
        display.setCursor(0,10); 
        display.print(String((INITIAL_WARMUP_DELAY_MS - (millis() - warmupStartTime))/1000) + "s restantes");
        display.display();
    }
    delay(1000); 
  }
  Serial.println(F("Aquecimento concluído."));

  setupWiFiAP(); // As chamadas displayMessage dentro de setupWiFiAP já checam oledInitialized internamente na versão que postei

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_ESP32_PIN, GPS_TX_ESP32_PIN);
  Serial.print(F("Serial do GPS (UART2) inicializada nos pinos RX_ESP="));
  Serial.print(GPS_RX_ESP32_PIN); Serial.print(F(", TX_ESP=")); Serial.println(GPS_TX_ESP32_PIN);
  
  Serial.println(F("Testando recepcao de dados do GPS por 5 segundos..."));
  unsigned long gpsTestStart = millis();
  bool gpsDataReceived = false;
  while(millis() - gpsTestStart < 5000) { 
    if (gpsSerial.available()) {
      Serial.write(gpsSerial.read()); 
      gpsDataReceived = true;
    }
  }
  if (gpsDataReceived) {
    Serial.println(F("\nDados recebidos do GPS."));
  } else {
    Serial.println(F("\nNENHUM dado recebido do GPS. Verifique fiação, alimentação do GPS e baud rate."));
  }
  
  dht.setup(DHT_PIN, DHTesp::DHT11); 
  Serial.print(F("Sensor DHT inicializado no pino: ")); Serial.println(DHT_PIN);

  if (oledInitialized) {
    displayMessage("Sistema Pronto", "ID: " + String(NODE_ID).substring(0,12), false, 2000);
  }
  Serial.println(F("Sistema pronto. Iniciando coleta de dados..."));
  lastDataCollectionTime = millis() - DATA_COLLECTION_INTERVAL_MS; 
}

void loop() {
  unsigned long currentTime = millis();

  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (gps.encode(c)) {
        // Opcional: log quando uma sentença GPS completa é recebida
    }
  }

  if (currentTime - lastDataCollectionTime >= DATA_COLLECTION_INTERVAL_MS) {
    lastDataCollectionTime = currentTime;
    local_message_counter++;
    
    float ppmCO2 = mq135.getPPM(); 
    float ppmCO  = map(analogRead(MQ7_PIN), 0, 4095, 10, 1000); 
    TempAndHumidity dhtData = dht.getTempAndHumidity();

    String latStr = "N/A";
    String lonStr = "N/A";
    if (gps.location.isValid()) {
        latStr = String(gps.location.lat(), 6);
        lonStr = String(gps.location.lng(), 6);
    }

    String dateTimeStr = "N/A";
    if (gps.date.isValid() && gps.time.isValid()) {
      char sz[35]; 
      sprintf(sz, "%04d-%02d-%02dT%02d:%02d:%02d.%02dZ",
              gps.date.year(), gps.date.month(), gps.date.day(),
              gps.time.hour(), gps.time.minute(), gps.time.second(),
              gps.time.centisecond()); 
      dateTimeStr = String(sz);
    } else if (gps.time.isValid()){ 
        char sz[20];
        sprintf(sz, "%02d:%02d:%02d.%02d",
              gps.time.hour(), gps.time.minute(), gps.time.second(),
              gps.time.centisecond());
        dateTimeStr = String(sz);
    }

    String localDataCsv = String(NODE_ID) + "," +
                          String(local_message_counter) + ",0," + 
                          dateTimeStr + "," +
                          latStr + "," + lonStr + "," +
                          String(ppmCO2, 1) + "," + String(ppmCO, 1) + "," +
                          (isnan(dhtData.temperature) ? "N/A" : String(dhtData.temperature, 1)) + "," +
                          (isnan(dhtData.humidity) ? "N/A" : String(dhtData.humidity, 1));

    Serial.println("CSV: " + localDataCsv); 

    if (WiFi.softAPgetStationNum() > 0) { 
        lastSendSuccess = sendDataToLinux(localDataCsv);
        if (lastSendSuccess) {
            // Serial.println(F("-> Dados enviados OK para o servidor.")); // Reduzir logs
        } else {
            Serial.println(F("-> FALHA ao enviar dados para o servidor."));
        }
    } else {
        // Serial.println(F("-> Nenhum cliente conectado ao AP. Dados não enviados.")); // Reduzir logs
        lastSendSuccess = false; 
    }

    if (oledInitialized) { // Só tenta atualizar o display se ele foi inicializado
        displaySensorData(NODE_ID, local_message_counter, lastSendSuccess,
                          dhtData.temperature, dhtData.humidity, ppmCO2, ppmCO,
                          latStr, lonStr, dateTimeStr);
    }
  }
}