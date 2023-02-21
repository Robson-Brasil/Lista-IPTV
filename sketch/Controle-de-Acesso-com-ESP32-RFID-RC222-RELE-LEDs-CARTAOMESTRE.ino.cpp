#include <Arduino.h>
#line 1 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
/*
IoT - Automação Residencial
Controle de Acesso por RFID
Autor : Robson Brasil
Dispositivo : ESP32 WROOM32
Preferences--> Aditional boards Manager URLs: 
                                  http://arduino.esp8266.com/stable/package_esp8266com_index.json,https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
Download Board ESP32 (2.0.5):
Módulo RFID RC-522
LCD I2C
Cadastramento por Cartão Mestre
Versão : 5 - Alfa
Última Modificação : 15/22/2023
   --------------------------------------------------------------------------------------------------------------------
   Example sketch/program showing An Arduino Door Access Control featuring RFID, EEPROM, Rele
   --------------------------------------------------------------------------------------------------------------------
   This is a MFRC522 library example; for further details and other examples see: https://github.com/miguelbalboa/rfid
   This example showing a complete Door Access Control System
  Simple Work Flow (not limited to) :
                                     +---------+
  +----------------------------------->READ TAGS+^------------------------------------------+
  |                              +--------------------+                                     |
  |                              |                    |                                     |
  |                              |                    |                                     |
  |                         +----v-----+        +-----v----+                                |
  |                         |MASTER TAG|        |OTHER TAGS|                                |
  |                         +--+-------+        ++-------------+                            |
  |                            |                 |             |                            |
  |                            |                 |             |                            |
  |                      +-----v---+        +----v----+   +----v------+                     |
  |         +------------+READ TAGS+---+    |KNOWN TAG|   |UNKNOWN TAG|                     |
  |         |            +-+-------+   |    +-----------+ +------------------+              |
  |         |              |           |                |                    |              |
  |    +----v-----+   +----v----+   +--v--------+     +-v----------+  +------v----+         |
  |    |MASTER TAG|   |KNOWN TAG|   |UNKNOWN TAG|     |GRANT ACCESS|  |DENY ACCESS|         |
  |    +----------+   +---+-----+   +-----+-----+     +-----+------+  +-----+-----+         |
  |                       |               |                 |               |               |
  |       +----+     +----v------+     +--v---+             |               +--------------->
  +-------+EXIT|     |DELETE FROM|     |ADD TO|             |                               |
        +----+     |  EEPROM   |     |EEPROM|             |                               |
                   +-----------+     +------+             +-------------------------------+
Use um cartão mestre que funcionará como programador, então você poderá escolher quem terá acesso ou não.

Interface de usuário fácil
Apenas um tag RFID é necessário para adicionar ou remover tags. Você pode escolher usar LEDs para saída ou um módulo LCD serial para informar os usuários.
Armazena informações na EEPROM
As informações são armazenadas na memória EEPROM não volátil do Arduino para preservar os tags do usuário e o cartão mestre. Nenhuma informação é perdida se a energia for desligada. A EEPROM tem um ciclo de leitura ilimitado, mas um ciclo de gravação limitado de cerca de 100.000.
Segurança
Para simplificar, usaremos os IDs exclusivos dos tags. É simples, mas não é à prova de hackers.
@license Liberado para o domínio público.
Layout típico de pinos usados:
   ---------------------------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino     ESP32
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   ---------------------------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST         2
   SPI SS      SDA(SS)      10            53        D10        10               10          5
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16          23
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14          19
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15          18
*/

#include <EEPROM.h>   // Nós vamos ler e escrever UIDs do PICC na EEPROM.
#include <SPI.h>      // O módulo RC522 utiliza o protocolo SPI.
#include <MFRC522.h>  // Biblioteca para Dispositivos Mifare RC522
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

/*
   Em vez de um relé, você pode querer usar um servo. Os servos também podem bloquear e desbloquear fechaduras de portas
   O relé será usado por padrão
*/

// #include <Servo.h>

/*
  Para visualizar o que está acontecendo no hardware, precisamos de alguns leds e para controlar a trava da porta, um relé e um botão de limpeza
  (ou algum outro hardware) Led de ânodo comum usado, digitalWriting HIGH desliga o led Lembre-se de que, se você estiver indo
  para usar led de cátodo comum ou apenas leds separados, simplesmente comente #define COMMON_ANODE.
*/

#define PortaAberta 4  //Sensor de fim de curso, o RFID só lerá outro cartão, quando a porta for fechada

#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

// Definir as credenciais da rede WiFi
const char* ssid = "RVR 2,4GHz";
const char* password = "RodrigoValRobson2021";

// Definir as credenciais do servidor MQTT
const char* mqttServer = "192.168.15.10";
const int mqttPort = 1883;
const char* mqttUser = "RobsonBrasil";
const char* mqttPassword = "loboalfa";

// Definir as credenciais no servidor MQTT
const char* subtopic = "LEITURA";
const char* subtopicnegado = "NEGADO";
const char* subtopicautorizado = "AUTORIZADO";
const char* pubtopicnegado = "NEGADO";
const char* pubtopicautorizado = "AUTORIZADO";

// Inicializar o cliente WiFi
WiFiClient wifiClient;

// Inicializar o cliente MQTT
PubSubClient client(mqttServer, mqttPort, wifiClient);

constexpr uint8_t LedVermelho = 27;  // Set Led Pins
constexpr uint8_t LedVerde = 26;
constexpr uint8_t LedAzul = 25;

constexpr uint8_t Rele = 13;       // GPIO do Relé
constexpr uint8_t BotaoWipe = 33;  // Pino do botão para o modo de limpeza

boolean match = false;        // Inicializar a correspondência do cartão como falso
boolean programMode = false;  // Inicializar modo de programação como falso
boolean replaceMaster = false;

uint8_t successRead;  // Variável inteira para manter se tivemos uma leitura bem-sucedida do leitor.

byte storedCard[4];  // Armazena um ID lido da EEPROM.
byte readCard[4];    // Armazena o ID escaneado lido do módulo RFID.
byte masterCard[4];  // Armazena o ID do cartão master lido da EEPROM.

// Cria uma instância da classe MFRC522.
constexpr uint8_t RST_PIN = 2;  // Configurável, veja o layout típico de pinos acima.
constexpr uint8_t SS_PIN = 5;   // Configurável, veja o layout típico de pinos acima.

MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- Variáveis Globais ---
char st[20];

// Definir o número de colunas e linhas do display LCD
int ColunasLCD = 20;
int LinhasLCD = 4;

// Definir o endereço do display LCD, número de colunas e linhas
// Se você não souber o endereço do seu display, execute um sketch de scanner I2C.
LiquidCrystal_I2C lcd(0x27, ColunasLCD, LinhasLCD);

String messageStatic0 = "POR FAVOR";
String messageStatic1 = "APROXIME SEU CARTAO!";

// Função para rolar o texto
// A função aceita os seguintes argumentos:
// row: Número da linha onde o texto será exibido
// message: Mensagem para rolar
// delayTime: Atraso entre cada deslocamento de caractere
// ColunasLCD: Número de colunas do seu LCD
#line 163 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void scrollText(int row, String message, int delayTime, int ColunasLCD);
#line 175 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void setup();
#line 312 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void callback(char* topic, byte* payload, unsigned int length);
#line 338 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void reconnectMQTT();
#line 360 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void reconnect();
#line 364 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void loop();
#line 461 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void granted(uint16_t setDelay);
#line 479 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void denied();
#line 486 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
uint8_t getID();
#line 510 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void ShowReaderDetails();
#line 535 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void cycleLeds();
#line 550 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void normalModeOn();
#line 557 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void readID(uint8_t number);
#line 564 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void writeID(byte a[]);
#line 582 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void deleteID(byte a[]);
#line 610 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
boolean checkTwo(byte a[], byte b[]);
#line 624 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
uint8_t findIDSLOT(byte find[]);
#line 636 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
boolean findID(byte find[]);
#line 650 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void successWrite();
#line 668 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void failedWrite();
#line 686 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void successDelete();
#line 704 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
boolean isMaster(byte test[]);
#line 711 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
bool monitorBotaoWipeutton(uint32_t interval);
#line 163 "D:\\GitRepositorio\\RFID-e-NFC\\RC522\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE\\Controle-de-Acesso-com-ESP32-RFID-RC222-RELE-LEDs-CARTAOMESTRE.ino"
void scrollText(int row, String message, int delayTime, int ColunasLCD) {
  for (int i = 0; i < ColunasLCD; i++) {
    message = " " + message;
  }
  message = message + " ";
  for (int pos = 0; pos < message.length(); pos++) {
    lcd.setCursor(0, row);
    lcd.print(message.substring(pos, pos + ColunasLCD));
    delay(delayTime);
  }
}
///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  EEPROM.begin(1024);

  //Arduino Pin Configuration
  pinMode(PortaAberta, INPUT_PULLUP);  //Sensor de fim de curso, o RFID só lerá outro cartão, quando a porta for fechada
  pinMode(LedVermelho, OUTPUT);
  pinMode(LedVerde, OUTPUT);
  pinMode(LedAzul, OUTPUT);
  pinMode(BotaoWipe, INPUT_PULLUP);  // Habilitar o resistor pull-up do pino.
  pinMode(Rele, OUTPUT);
  //Tenha cuidado com o comportamento do circuito do relé durante a reinicialização ou desligamento do seu Arduino.
  digitalWrite(Rele, HIGH);            // Certifique-se de que a porta esteja trancada
  digitalWrite(LedVermelho, LED_OFF);  // Certifique-se de que o LED esteja desligado
  digitalWrite(LedVerde, LED_OFF);     // Certifique-se de que o LED esteja desligado
  digitalWrite(LedAzul, LED_OFF);      // Certifique-se de que o LED esteja desligado

  //Protocolo de configuração
  Serial.begin(115200);  // Inicializar comunicações serial com o PC.
  while (!Serial)
    ;
  SPI.begin();         // O Módulo MFRC522 usa o protocolo SPI
  mfrc522.PCD_Init();  // Inicializa o Módulo MFRC522 

  // Conectar-se à rede WiFi
  /* Se já está conectado a rede WI-FI, nada é feito.
  Caso contrário, são efetuadas tentativas de conexão*/
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(ssid, password);
  Serial.println("Conectando à rede WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }

  // Configurar as funções de retorno de chamada para o cliente MQTT
  client.setCallback(callback);
  client.setServer(mqttServer, mqttPort);

  // Conectar-se ao servidor MQTT
  client.connect("ESP32Client", mqttUser, mqttPassword);
  while (!client.connected()) {
    Serial.println("Conectando ao servidor MQTT...");
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("Conectado ao servidor MQTT!");
    } else {
      Serial.print("Falha na conexão ao servidor MQTT, código de erro: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
  // Inscrever-se no tópico
  client.subscribe(subtopic);
  client.subscribe(subtopicnegado);
  client.subscribe(subtopicautorizado);
  client.subscribe(pubtopicautorizado);
  client.subscribe(pubtopicnegado);

  // Se você definir o Ganho da Antena como Max, ele aumentará a distância de leitura
  // mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  Serial.println(F("Controle de Acesso v0.1"));  // Para fins de depuração
  ShowReaderDetails();                           // Mostrar detalhes do leitor de cartão PCD - MFRC522.

  // Wipe Code - Se o botão (BotaoWipe) for pressionado durante a execução da configuração (ligada), a EEPROM será apagada.
  if (digitalRead(BotaoWipe) == LOW) {  // Quando o botão for pressionado, o pino deve ficar baixo, o botão está conectado ao GND
    digitalWrite(LedVermelho, LED_ON);  // O LED vermelho fica aceso para informar o usuário que vamos apagar
    Serial.println(F("Botao de formatacao apertado"));
    Serial.println(F("Voce tem 10 segundos para cancelar"));
    Serial.println(F("Isso vai apagar todos os seus registros e nao tem como desfazer"));
    bool buttonState = monitorBotaoWipeutton(10000);             // Dê ao usuário tempo suficiente para cancelar a operação.
    if (buttonState == true && digitalRead(BotaoWipe) == LOW) {  // Se o botão ainda estiver pressionado, apaga a EEPROM.
      Serial.println(F("Inicio da formatacao da EEPROM"));
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {  //Fim do loop do endereço da EEPROM
        if (EEPROM.read(x) == 0) {                            //Se endereço da EEPROM for 0
          // Não faz nada, pois já está limpo. Vá para o próximo endereço para economizar tempo e reduzir gravações na EEPROM
        } else {
          EEPROM.write(x, 0);  // Se não, escreva 0 para limpar, isso leva 3,3 milissegundos
        }
      }
      Serial.println(F("EEPROM formatada com sucesso"));
      digitalWrite(LedVermelho, LED_OFF);  // Visualize uma formatação bem-sucedida
      delay(200);
      digitalWrite(LedVermelho, LED_ON);
      delay(200);
      digitalWrite(LedVermelho, LED_OFF);
      delay(200);
      digitalWrite(LedVermelho, LED_ON);
      delay(200);
      digitalWrite(LedVermelho, LED_OFF);
    } else {
      Serial.println(F("Formatacao cancelada"));  // Mostra algum feedback de que o botão de formatação não foi pressionado por 15 segundos.
      digitalWrite(LedVermelho, LED_OFF);
    }
  }
// Verificar se o cartão mestre foi definido, caso contrário permitir que o usuário escolha um cartão mestre
// Isso também é útil para apenas redefinir o cartão mestre
// Você pode manter outros registros EEPROM, basta escrever outro número diferente de 143 no endereço EEPROM 1
// O endereço EEPROM 1 deve conter o número mágico '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("Cartao Mestre nao definido"));
    Serial.println(F("Leia um chip para definir cartao Mestre"));
    do {
      successRead = getID();          // Define a variável successRead como 1 quando conseguimos fazer a leitura do leitor, caso contrário, 0.
      digitalWrite(LedAzul, LED_ON);  // Visualizar a necessidade de definir o Cartão Mestre.
      delay(200);
      digitalWrite(LedAzul, LED_OFF);
      delay(200);
    } while (!successRead);              // O programa não irá avançar até que você obtenha uma leitura bem sucedida.
    for (uint8_t j = 0; j < 4; j++) {    // Laço de repetição 4 vezes
      EEPROM.write(2 + j, readCard[j]);  // Escreva o UID do PICC escaneado na EEPROM, começando do endereço 3
    }
    EEPROM.write(1, 143);  // Escreva no EEPROM o cartão mestre que definimos.
    Serial.println(F("Cartao Mestre definido"));
  }
  Serial.println(F("-------------------"));
  Serial.println(F("UID do cartao Mestre"));
  for (uint8_t i = 0; i < 4; i++) {      // Ler o UID do Cartão Mestre na EEPROM.
    masterCard[i] = EEPROM.read(2 + i);  // Escreva-o em masterCard.
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Tudo esta pronto"));
  Serial.println(F("Aguardando pelos chips para serem lidos"));
  cycleLeds();  // "Está tudo pronto, vamos dar ao usuário algum feedback por meio do ciclo de LEDs.

  EEPROM.commit();

  // Iniciar o LCD
  lcd.begin();
  // turn on LCD backlight
  lcd.backlight();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);
  Serial.print("Conteúdo: ");

  String data = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  if (strcmp(topic, pubtopicautorizado)) {
    for (unsigned int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      data += (char)payload[i];
    }
  }

  Serial.println();

  if (strcmp((char*)payload, "Negado") == 0) {
    client.publish(pubtopicnegado, "Negado");
  } else {
    client.publish(pubtopicautorizado, "Autorizado");
  }
}

void reconnectMQTT() {
  // Enquanto não estiver conectado, tente se reconectar
  while (!client.connected()) {
    Serial.println("Conectando ao servidor MQTT...");
    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("Conectado ao servidor MQTT!");
    } else {
      Serial.print("Falha na conexão ao servidor MQTT, código de erro: ");
      Serial.println(client.state());
      Serial.println("Haverá nova tentativa de conexão em 2s");
      delay(2000);
    }
  }

  // Inscrever-se no tópico
  client.subscribe(subtopic);
  client.subscribe(subtopicnegado);
  client.subscribe(subtopicautorizado);
  client.subscribe(pubtopicautorizado);
  client.subscribe(pubtopicnegado);
}

void reconnect() {
  // ...
}
///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop() {

  lcd.setCursor(5, 0);
  // imprimir mensagem estática
  lcd.print(messageStatic0);
  // Defina o cursor na segunda coluna, primeira linha
  lcd.setCursor(0, 1);
  // print static message
  lcd.print(messageStatic1);

  do {
    successRead = getID();  // Define a variável successRead como 1 quando conseguimos ler do leitor e como 0 caso contrário.
    // Quando o dispositivo está em uso, se o botão de limpeza for pressionado por 10 segundos, inicialize a limpeza do cartão mestre
    if (digitalRead(BotaoWipe) == LOW) {  // Verifique se o botão está pressionado
      // Visualize que a operação normal é interrompida ao pressionar o botão de limpeza. A cor vermelha é utilizada para indicar maior aviso ao usuário.
      digitalWrite(LedVermelho, LED_ON);  // Certifique-se de que o LED está desligado
      digitalWrite(LedVerde, LED_OFF);    // Certifique-se de que o LED está desligado
      digitalWrite(LedAzul, LED_OFF);     // Certifique-se de que o LED está desligado
      // Give some feedback
      Serial.println(F("Botao de formatacao apertado"));
      Serial.println(F("O cartao Mestre sera apagado! em 10 segundos"));
      bool buttonState = monitorBotaoWipeutton(10000);             // Dar ao usuário tempo suficiente para cancelar a operação
      if (buttonState == true && digitalRead(BotaoWipe) == LOW) {  // Se o botão ainda estiver pressionado, limpe a EEPROM.
        EEPROM.write(1, 0);                                        // Resetar o número mágico.
        EEPROM.commit();
        Serial.println(F("Cartao Mestre desvinculado do dispositivo"));
        Serial.println(F("Aperte o reset da placa para reprogramar o cartao Mestre"));
        while (1)
          ;
      }
      Serial.println(F("Desvinculo do cartao Mestre cancelado"));
    }
    if (programMode) {
      cycleLeds();  // Modo de Programação exibe uma sequência de cores vermelha, verde e azul, aguardando a leitura de um novo cartão.
    } else {
      normalModeOn();  // Normal mode, blue Power LED is on, all others are off
    }
  } while (!successRead);  //the program will not go further while you are not getting a successful read
  if (programMode) {
    if (isMaster(readCard)) {  //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Leitura do cartao Mestre"));
      Serial.println(F("Saindo do modo de programacao"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      return;
    } else {
      if (findID(readCard)) {  // If scanned card is known delete it
        Serial.println(F("Conheco este chip, removendo..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
      } else {  // If scanned card is not known add it
        Serial.println(F("Nao conheco este chip, incluindo..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
      }
    }
  } else {
    if (isMaster(readCard)) {  // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      Serial.println(F("Ola Mestre - Modo de programacao iniciado"));
      uint8_t count = EEPROM.read(0);  // Read the first Byte of EEPROM that
      Serial.print(F("Existem "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" registro(s) na EEPROM"));
      Serial.println("");
      Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
      Serial.println(F("Leia o cartao Mestre novamente para sair do modo de programacao"));
      Serial.println(F("-----------------------------"));
    } else {
      if (findID(readCard)) {  // If not, see if the card is in the EEPROM
        lcd.setCursor(2, 3);
        // print scrolling message
        lcd.print("Voce pode passar");
        Serial.println(F("Bem-vindo, voce pode passar"));
        granted(300);  // Open the door lock for 300 ms
      } else {         // If not, show that the ID was not valid
        Serial.println(F("Voce nao pode passar"));
        lcd.setCursor(0, 3);
        lcd.print("Voce nao pode passar");
        if (getID() == findID(readCard)) {
          client.publish(pubtopicnegado, "Negado");
        }
        delay(5000);
        lcd.clear();  // limpa a tela
        denied();
      }
    }
  }
  // Verifica a conexão com o servidor MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
}
/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted(uint16_t setDelay) {
  digitalWrite(LedAzul, LED_OFF);      // Turn off blue LED
  digitalWrite(LedVermelho, LED_OFF);  // Turn off red LED
  digitalWrite(LedVerde, LED_ON);      // Turn on green LED
  digitalWrite(Rele, LOW);             // Unlock door!
  delay(setDelay);                     // Hold door lock open for given seconds
  digitalWrite(Rele, HIGH);            // Relock door
  if (getID() == !findID(readCard)) {
    client.publish(pubtopicautorizado, "Autorizado");
  }
  //Mantém acesso liberado até acionar o sensor de porta
  while (digitalRead(PortaAberta)) digitalWrite(Rele, LOW);
  digitalWrite(Rele, HIGH);
  while (digitalRead(PortaAberta)) digitalWrite(LedVerde, LOW);
  digitalWrite(LedVerde, HIGH);
  lcd.clear();  // limpa a tela
}
///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  digitalWrite(LedAzul, LED_OFF);   // Make sure blue LED is off

  delay(1000);
}
///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if (!mfrc522.PICC_IsNewCardPresent()) {  //If a new PICC placed to RFID reader continue
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {  //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("UID do chip lido:"));
  for (uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
    lcd.setCursor(0, 2);
    lcd.print("USUARIO  AUTORIZADO?");
  }

  Serial.println("");
  mfrc522.PICC_HaltA();  // Stop reading
  return 1;
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("Versao do software MFRC522: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (desconhecido),provavelmente um clone chines?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("ALERTA: Falha na comunicacao, o modulo MFRC522 esta conectado corretamente?"));
    Serial.println(F("SISTEMA ABORTADO: Verifique as conexoes."));
    // Visualize system is halted
    digitalWrite(LedVerde, LED_OFF);    // Make sure green LED is off
    digitalWrite(LedAzul, LED_OFF);     // Make sure blue LED is off
    digitalWrite(LedVermelho, LED_ON);  // Turn on red LED
    while (true)
      ;  // do not go further
  }
}
///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  digitalWrite(LedVerde, LED_ON);      // Make sure green LED is on
  digitalWrite(LedAzul, LED_OFF);      // Make sure blue LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);     // Make sure green LED is off
  digitalWrite(LedAzul, LED_ON);       // Make sure blue LED is on
  delay(200);
  digitalWrite(LedVermelho, LED_ON);  // Make sure red LED is on
  digitalWrite(LedVerde, LED_OFF);    // Make sure green LED is off
  digitalWrite(LedAzul, LED_OFF);     // Make sure blue LED is off
  delay(200);
}
//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn() {
  digitalWrite(LedAzul, LED_ON);       // Blue LED ON and ready to read card
  digitalWrite(LedVermelho, LED_OFF);  // Make sure Red LED is off
  digitalWrite(LedVerde, LED_OFF);     // Make sure Green LED is off
  digitalWrite(Rele, HIGH);            // Make sure Door is Locked
}
//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID(uint8_t number) {
  uint8_t start = (number * 4) + 2;          // Figure out starting position
  for (uint8_t i = 0; i < 4; i++) {          // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);  // Assign values read from EEPROM to array
  }
}
///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID(byte a[]) {
  if (!findID(a)) {                    // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);      // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = (num * 4) + 6;     // Figure out where the next slot starts
    num++;                             // Increment the counter by one
    EEPROM.write(0, num);              // Write the new count to the counter
    for (uint8_t j = 0; j < 4; j++) {  // Loop 4 times
      EEPROM.write(start + j, a[j]);   // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();
    successWrite();
    Serial.println(F("ID adicionado na EEPROM com sucesso"));
  } else {
    failedWrite();
    Serial.println(F("Erro! Tem alguma coisa errada com o ID do chip ou problema na EEPROM"));
  }
}
///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID(byte a[]) {
  if (!findID(a)) {  // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();   // If not
    Serial.println(F("Erro! Tem alguma coisa errada com o ID do chip ou problema na EEPROM"));
  } else {
    uint8_t num = EEPROM.read(0);  // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;                  // Figure out the slot number of the card
    uint8_t start;                 // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;               // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0);  // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT(a);            // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;                                                  // Decrement the counter by one
    EEPROM.write(0, num);                                   // Write the new count to the counter
    for (j = 0; j < looping; j++) {                         // Loop the card shift times
      EEPROM.write(start + j, EEPROM.read(start + 4 + j));  // Shift the array values to 4 places earlier in the EEPROM
    }
    for (uint8_t k = 0; k < 4; k++) {  // Shifting loop
      EEPROM.write(start + j + k, 0);
    }
    EEPROM.commit();
    successDelete();
    Serial.println(F("ID removido da EEPROM com sucesso"));
  }
}
///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo(byte a[], byte b[]) {
  if (a[0] != 0)                     // Make sure there is something in the array first
    match = true;                    // Assume they match at first
  for (uint8_t k = 0; k < 4; k++) {  // Loop 4 times
    if (a[k] != b[k])                // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if (match) {    // Check to see if if match is still true
    return true;  // Return true
  } else {
    return false;  // Return false
  }
}
///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT(byte find[]) {
  uint8_t count = EEPROM.read(0);         // Read the first Byte of EEPROM that
  for (uint8_t i = 1; i <= count; i++) {  // Loop once for each EEPROM entry
    readID(i);                            // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) {     // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;  // The slot number of the card
      break;     // Stop looking we found it
    }
  }
}
///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID(byte find[]) {
  uint8_t count = EEPROM.read(0);         // Read the first Byte of EEPROM that
  for (uint8_t i = 1; i <= count; i++) {  // Loop once for each EEPROM entry
    readID(i);                            // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) {     // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    } else {  // If not, return false
    }
  }
  return false;
}
///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(LedAzul, LED_OFF);      // Make sure blue LED is off
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);     // Make sure green LED is on
  delay(200);
  digitalWrite(LedVerde, LED_ON);  // Make sure green LED is on
  delay(200);
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(LedVerde, LED_ON);  // Make sure green LED is on
  delay(200);
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(LedVerde, LED_ON);  // Make sure green LED is on
  delay(200);
}
///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(LedAzul, LED_OFF);      // Make sure blue LED is off
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);     // Make sure green LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_ON);  // Make sure red LED is on
  delay(200);
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_ON);  // Make sure red LED is on
  delay(200);
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_ON);  // Make sure red LED is on
  delay(200);
}
///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  digitalWrite(LedAzul, LED_OFF);      // Make sure blue LED is off
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);     // Make sure green LED is off
  delay(200);
  digitalWrite(LedAzul, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(LedAzul, LED_OFF);  // Make sure blue LED is off
  delay(200);
  digitalWrite(LedAzul, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(LedAzul, LED_OFF);  // Make sure blue LED is off
  delay(200);
  digitalWrite(LedAzul, LED_ON);  // Make sure blue LED is on
  delay(200);
}
////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster(byte test[]) {
  if (checkTwo(test, masterCard))
    return true;
  else
    return false;
}

bool monitorBotaoWipeutton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval) {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(BotaoWipe) != LOW)
        return false;
    }
  }
  return true;
}
