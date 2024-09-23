#include <WiFi.h>
#include <PubSubClient.h>

// Configurações do Wi-Fi
const char* ssid = "";
const char* password = "";

// Configurações do MQTT
const char* mqtt_server = "";
const int mqtt_port = 1883; // Porta padrão do MQTT
const char* mqtt_user = ""; // Se necessário
const char* mqtt_password = ""; // Se necessário

WiFiClient espClient;
PubSubClient client(espClient);

const int relayPin = 27; // Pino do relé
const int trigPin = 5;  // Pino Trigger do sensor ultrassônico
const int echoPin = 18; // Pino Echo do sensor ultrassônico

void setup() {
  Serial.begin(115200);
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // Inicializa o relé desligado

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Conectar ao Wi-Fi
  setupWiFi();

  // Configurar o cliente MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  // Conectar ao broker MQTT
  reconnectMQTT();
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  
  client.loop();

  // Medir a distância e publicar no tópico
  static unsigned long lastMeasurement = 0;
  unsigned long now = millis();
  if (now - lastMeasurement > 5000) { // Medir e publicar a cada 5 segundos
    lastMeasurement = now;

    long distance = measureDistance();
    String distanceStr = String(distance) + " cm";
    client.publish("sensor/distance", distanceStr.c_str());

    String statusMessage;
    if (distance <= 15) {
      statusMessage = "Objeto muito perto";
      digitalWrite(relayPin, LOW); // Ligar o relé
      client.publish("relay/status", "Relé ligado, objeto perto");
    } else if (distance >= 20) {
      statusMessage = "Objeto longe";
      digitalWrite(relayPin, HIGH); // Desligar o relé
      client.publish("relay/status", "Relé desligado, objeto longe");
    } else {
     statusMessage = "Distância média: " + distanceStr;
    }

    client.publish("sensor/status", statusMessage.c_str());

    Serial.print("Distância: ");
    Serial.print(distance);
    Serial.println(" cm");
    Serial.print("Status: ");
    Serial.println(statusMessage);
  }
}

void setupWiFi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Conectado ao Wi-Fi");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    
    if (client.connect("esp32Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado ao MQTT");
      
      // Inscreva-se em tópicos se necessário
      client.subscribe("relay/control");
      client.subscribe("comando");
    } else {
      Serial.print("Falha na conexão. Código de erro: ");
      Serial.print(client.state());
      delay(5000); // Tentar novamente após 5 segundos
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico ");
  Serial.print(topic);
  Serial.print(": ");

  String command = "";
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  Serial.println(command);

  // Controlar o relé com base no comando recebido
  if (command == "ligar") {
    digitalWrite(relayPin, LOW); // Ligar o relé
    client.publish("comando", "ON");
  } else if (command == "desligar") {
    digitalWrite(relayPin, HIGH); // Desligar o relé
    client.publish("comando", "OFF");
  }
}

long measureDistance() {
  // Enviar pulso de 10 microsegundos
  digitalWrite(trigPin, LOW);
  delayMicroseconds(10);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Ler o tempo de resposta do pino Echo
  long duration = pulseIn(echoPin, HIGH);

  // Converter o tempo em distância (cm)
  long distance = (duration * 0.0344) / 2; // Calcula a distância em cm

  return distance;
}
