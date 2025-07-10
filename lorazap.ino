#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================================================================
// ================= SELETOR DE DISPOSITIVO ==========================
// DESCOMENTE A LINHA ABAIXO PARA COMPILAR PARA O "DISPOSITIVO A"
#define DEVICE_B 
// DEIXE COMENTADO PARA COMPILAR PARA O "DISPOSITIVO B"
// ===================================================================

// --- Definição da Identidade do Dispositivo ---
#ifdef DEVICE_A
  #define DEVICE_ID "Dispositivo A"
#else
  #define DEVICE_ID "Dispositivo B"
#endif

// --- Pinos do Hardware ---
#define LORA_SCK    5
#define LORA_MISO   19
#define LORA_MOSI   27
#define LORA_NSS    18
#define LORA_RST    14
#define LORA_DIO0   26

#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define PRG_BUTTON  0
#define LED_PIN     17 

// --- Variáveis Globais e Objetos ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
enum State { STATE_IDLE, STATE_WAIT_ACK };
State currentState = STATE_IDLE;
unsigned long txStartTime;
const long ACK_TIMEOUT = 5000;

bool ledState = true; 

void updateDisplayStatus();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(PRG_BUTTON, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  ledState = true;
  digitalWrite(LED_PIN, HIGH);
  
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao iniciar o display SSD1306"));
    while (1); 
  }
  
  Serial.println(String(DEVICE_ID) + " LoRa Controle de LED");
  updateDisplayStatus();

  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(915E6)) {
    Serial.println("Falha ao iniciar LoRa!");
    displayMessage("Falha LoRa!");
    while (1);
  }
  
  LoRa.setSpreadingFactor(7);
  LoRa.setSyncWord(0xF3);
  
  Serial.println("LoRa iniciado! Aguardando...");
  LoRa.receive();
}

void loop() {
  if (currentState == STATE_IDLE && digitalRead(PRG_BUTTON) == LOW) {
    delay(50);
    if(digitalRead(PRG_BUTTON) == LOW) {
      sendCommandToggleLed();
      while(digitalRead(PRG_BUTTON) == LOW);
    }
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    processIncomingPacket(packetSize);
  }

  if (currentState == STATE_WAIT_ACK && (millis() - txStartTime > ACK_TIMEOUT)) {
    Serial.println("Timeout! Nenhuma confirmação (ACK) recebida.");
    displayMessage("Timeout!\nSem resposta.");
    
    currentState = STATE_IDLE;
    LoRa.receive(); 
    
    delay(2000);
    updateDisplayStatus();
  }
}

void sendCommandToggleLed() {
  String message = "CMD_LED:" + String(DEVICE_ID) + ":TOGGLE";
  
  Serial.println("Enviando comando: " + message);
  displayMessage("Enviando comando\npara alternar LED...");
  
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  
  blinkLed(1);

  currentState = STATE_WAIT_ACK;
  txStartTime = millis();
}

void processIncomingPacket(int packetSize) {
  String receivedText = "";
  while (LoRa.available()) {
    receivedText += (char)LoRa.read();
  }
  // O RSSI do pacote recebido é medido aqui
  int rssi = LoRa.packetRssi();
  blinkLed(2);

  Serial.print("Recebido: '");
  Serial.print(receivedText);
  Serial.print("' com RSSI: ");
  Serial.println(rssi);

  // --- Lógica de Protocolo ---
  
  // 1. É um comando para o LED?
  if (receivedText.startsWith("CMD_LED:")) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    
    String currentLedStatusStr = ledState ? "LIGADO" : "DESLIGADO";
    displayMessage("Comando recebido!\nMeu LED agora: " + currentLedStatusStr);

    // MODIFICADO: Inclui o RSSI medido na mensagem de ACK.
    // O novo formato será: "ACK:DEVICE_ID:STATUS_LED:RSSI"
    String ackMessage = "ACK:" + String(DEVICE_ID) + ":" + currentLedStatusStr + ":" + String(rssi);
    
    LoRa.beginPacket();
    LoRa.print(ackMessage);
    LoRa.endPacket();
    Serial.println("Enviando ACK: " + ackMessage);
    
    LoRa.receive();
    
    delay(2000);
    updateDisplayStatus();
    
  // 2. É uma confirmação?
  } else if (receivedText.startsWith("ACK:")) {
    if (currentState == STATE_WAIT_ACK) {
      
      // MODIFICADO: Lógica para extrair o status do LED e o RSSI do payload
      // Formato esperado: ACK:REMETENTE:STATUS:RSSI
      int firstColon = receivedText.indexOf(':');
      int secondColon = receivedText.indexOf(':', firstColon + 1);
      int thirdColon = receivedText.indexOf(':', secondColon + 1);

      // Verifica se o formato está correto antes de tentar extrair os dados
      if (secondColon != -1 && thirdColon != -1) {
          String sender = receivedText.substring(firstColon + 1, secondColon);
          String ledStatusPayload = receivedText.substring(secondColon + 1, thirdColon);
          String rssiPayload = receivedText.substring(thirdColon + 1);
          
          displayMessage("Confirmado! " + sender + "\nLED: " + ledStatusPayload + "\n\nRSSI: " + rssiPayload + " dBm");
      
      } else {
          // Se o formato estiver errado, exibe uma mensagem genérica
          displayMessage("ACK Recebido!\n(formato invalido)");
      }
      
      currentState = STATE_IDLE; // Sucesso, volta ao estado ocioso
      delay(3000); // Aumenta o tempo para dar para ler o RSSI
      updateDisplayStatus(); // Atualiza o display principal
    }
  }
}

// --- Funções Auxiliares ---

void updateDisplayStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(DEVICE_ID);
  display.setCursor(0, 16);
  display.print("Status LED: ");
  display.println(ledState ? "LIGADO" : "DESLIGADO");
  display.setCursor(0, 40);
  display.print("Pressione o botao\npara alternar o outro");
  display.display();
}

void displayMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg); 
  display.display();
}

void blinkLed(int times) {
  bool previousState = digitalRead(LED_PIN);
  for (int i=0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) {
      delay(50);
    }
  }
  digitalWrite(LED_PIN, previousState);
}
/*
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===================================================================
// ================= SELETOR DE DISPOSITIVO ==========================
//                                                                    
// DESCOMENTE A LINHA ABAIXO PARA COMPILAR PARA O "DISPOSITIVO A"
// DEIXE COMENTADO PARA COMPILAR PARA O "DISPOSITIVO B"
//                                                                    
#define DEVICE_B 
//                                                                    
// ===================================================================


// --- Definição da Identidade do Dispositivo ---
#ifdef DEVICE_A
  #define DEVICE_ID "Dispositivo A"
#else
  #define DEVICE_ID "Dispositivo B"
#endif


// --- Pinos do Hardware (iguais para ambos os dispositivos) ---
#define LORA_SCK    5
#define LORA_MISO   19
#define LORA_MOSI   27
#define LORA_NSS    18
#define LORA_RST    14
#define LORA_DIO0   26

#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define PRG_BUTTON  0
#define LED_PIN     17 // LED_BUILTIN para algumas placas, ou um pino específico

// --- Variáveis Globais e Objetos ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
enum State { STATE_IDLE, STATE_WAIT_ACK };
State currentState = STATE_IDLE;
unsigned long txStartTime;
const long ACK_TIMEOUT = 5000; // 5 segundos para esperar por uma confirmação
int packetCounter = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(PRG_BUTTON, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // =================== INICIALIZAÇÃO CORRETA DO DISPLAY ===================
  // 1. Reset manual do OLED
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  // 2. Inicia o barramento I2C com os pinos corretos ANTES de iniciar o display
  Wire.begin(OLED_SDA, OLED_SCL);

  // 3. Tenta iniciar o display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao iniciar o display SSD1306"));
    while (1); 
  }
  // ========================================================================
  
  displayMessage(String(DEVICE_ID) + "\nPronto.\nPressione PRG");
  Serial.println(String(DEVICE_ID) + " LoRa Bidirecional");

  // --- Inicialização do LoRa ---
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(915E6)) { // Certifique-se que a frequência é permitida na sua região
    Serial.println("Falha ao iniciar LoRa!");
    displayMessage("Falha LoRa!");
    while (1);
  }
  
  // Parâmetros de comunicação (devem ser iguais em ambos os dispositivos)
  LoRa.setSpreadingFactor(7);
  LoRa.setSyncWord(0xF3);
  
  Serial.println("LoRa iniciado! Aguardando...");
  LoRa.receive(); // Coloca o rádio em modo de recebimento
}


void loop() {
  // --- Bloco de Envio ---
  // Se o dispositivo estiver ocioso e o botão for pressionado...
  if (currentState == STATE_IDLE && digitalRead(PRG_BUTTON) == LOW) {
    delay(50); // Debounce do botão
    if(digitalRead(PRG_BUTTON) == LOW) {
      sendNewMessage();
      while(digitalRead(PRG_BUTTON) == LOW); // Espera o botão ser solto
    }
  }

  // --- Bloco de Recebimento ---
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    processIncomingPacket(packetSize);
  }

  // --- Bloco de Timeout ---
  // Se estivermos esperando por um ACK e o tempo esgotou...
  if (currentState == STATE_WAIT_ACK && (millis() - txStartTime > ACK_TIMEOUT)) {
    Serial.println("Timeout! Nenhuma confirmação (ACK) recebida.");
    displayMessage("Timeout!\nSem resposta.");
    
    // Volta ao estado inicial
    currentState = STATE_IDLE;
    LoRa.receive(); // Coloca o rádio de volta em modo de escuta
    
    delay(2000);
    displayMessage(String(DEVICE_ID) + "\nPronto.\nPressione PRG");
  }
}

void sendNewMessage() {
  packetCounter++;
  String message = "MSG:" + String(DEVICE_ID) + ":oi #" + String(packetCounter);
  
  Serial.println("Enviando: " + message);
  displayMessage("Enviando...\n" + message);
  
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  
  blinkLed(1); // Pisca 1 vez para indicar envio

  // Muda o estado para esperar pela confirmação
  currentState = STATE_WAIT_ACK;
  txStartTime = millis(); // Marca o tempo de início do envio
}

void processIncomingPacket(int packetSize) {
  String receivedText = "";
  while (LoRa.available()) {
    receivedText += (char)LoRa.read();
  }
  int rssi = LoRa.packetRssi();

  blinkLed(2); // Pisca 2 vezes para indicar recebimento

  Serial.print("Recebido: '");
  Serial.print(receivedText);
  Serial.print("' com RSSI: ");
  Serial.println(rssi);

  // --- Lógica de Protocolo ---
  
  // 1. É uma mensagem nova? (Começa com "MSG:")
  if (receivedText.startsWith("MSG:")) {
    // Extrai quem enviou e a mensagem
    String sender = receivedText.substring(4, receivedText.indexOf(':', 4));
    String payload = receivedText.substring(receivedText.indexOf(':', 4) + 1);
    
    displayMessage("De: " + sender + "\n" + payload + "\nRSSI: " + String(rssi));
    
    // Envia uma mensagem de confirmação (ACK) de volta
    String ackMessage = "ACK:" + String(DEVICE_ID) + ":RSSI " + String(rssi);
    
    // Importante: LoRa não pode enviar e receber ao mesmo tempo.
    // O envio do ACK interrompe o modo de recebimento, então precisamos reativá-lo depois.
    LoRa.beginPacket();
    LoRa.print(ackMessage);
    LoRa.endPacket();
    Serial.println("Enviando ACK: " + ackMessage);
    
    // Após enviar o ACK, volta imediatamente para o modo de recebimento
    LoRa.receive(); 
    
  // 2. É uma confirmação? (Começa com "ACK:")
  } else if (receivedText.startsWith("ACK:")) {
    // Só processa o ACK se estivéssemos esperando por um
    if (currentState == STATE_WAIT_ACK) {
      String sender = receivedText.substring(4, receivedText.indexOf(':', 4));
      String payload = receivedText.substring(receivedText.indexOf(':', 4) + 1);
      
      displayMessage("ACK de: " + sender + "\n" + payload);
      
      // Sucesso! Volta ao estado ocioso.
      currentState = STATE_IDLE;
    }
  }

  // Se após processar o pacote, o dispositivo ficou ocioso, atualiza o display
  // para a mensagem de "Pronto" após um tempo.
  if (currentState == STATE_IDLE) {
    delay(2000);
    displayMessage(String(DEVICE_ID) + "\nPronto.\nPressione PRG");
  }
}

// --- Funções Auxiliares ---

void displayMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg); 
  display.display();
}

void blinkLed(int times) {
  for (int i=0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) {
      delay(100);
    }
  }
}
*/