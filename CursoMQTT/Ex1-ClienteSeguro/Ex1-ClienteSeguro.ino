/********************************************************
 * CANAL INTERNET E COISAS
 * Curso MQTT - Parte 6
 * Cliente MQTT
 * 02/2019 - Andre Michelon
 */

// Bibliotecas
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Wi-Fi
const char* ssid = "home2";
const char* password = "home2hotspot";

// Instancias
WiFiClientSecure espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // Processa mensagens recebidas
  Serial.print("Recebido [");
  Serial.print(topic);
  Serial.print("] ");
  String s = "";
  for (int i = 0; i < length; i++) {
    s += (char)payload[i];
  }
  Serial.println(s);
}

void reconnect() {
  // Reconecta ao Broker
  while (!client.connected()) {
    Serial.println("Connectando...");
    // Conecta
    if (client.connect("ESP8266", "andre", "teste")) {
      Serial.println("Conectado");

      // Assina topico "test"
      client.subscribe("test");
    } else {
      // Falha na conexao
      Serial.print("Falha...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(74880);

  // Conecta ao Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nOk");

  // Testa conexao
  if (!espClient.connect("home2", 8883)) {
    Serial.println("Falha na conexao.");
    while(1);
  }

  // Valida assinatura do Servidor
  if (espClient.verify("B0 86 88 B7 C0 8F 89 33 FA A4 71 2E 71 26 4B B0 31 5A 12 2D", "home2")) {
    Serial.println("Identidade do Servidor ok");
  } else {
    Serial.println("Identidade do Servidor nao confirmada");
  }

  client.disconnect();

  // Define broker
  client.setServer("home2", 8883);

  // Define funcao para recebimento de mensagens
  client.setCallback(callback);
}

unsigned long t;
String msg;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Publica mensagem a cada 5s
  if (millis() - t > 5000) {
    t = millis();
    msg = "Tempo ativo " + String(t / 1000) + "s";
    Serial.println("Publicado: " + msg);
    client.publish("test", msg.c_str());
  }
}
