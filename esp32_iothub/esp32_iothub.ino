#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID     = "YOURSSID";
const char* WIFI_PASSWORD = "***";

const char* IOT_HUB_HOST  = "YOURHUB.azure-devices.net";
const char* DEVICE_ID     = "YOURDEVICE";
const char* SAS_TOKEN     = "SharedAccessSignature sr=...";
const int   MQTT_PORT     = 8883;
const char* MQTT_USERNAME = "YOURHUB.azure-devices.net/YOURDEVICE/?api-version=2021-04-12";

char telemetryTopic[128];
char c2dTopic[128];

const char* ROOT_CA =
"-----BEGIN CERTIFICATE-----\n"
"...DigiCert Global Root G2...\n"
"-----END CERTIFICATE-----\n";

WiFiClientSecure wifiClient;
PubSubClient     mqttClient(wifiClient);

const int LED_PIN = 2;

void onCommandReceived(char* topic, byte* payload, unsigned int length) {
  Serial.println("\nComando recibido desde la nube:");
  char message[256];
  memcpy(message, payload, min(length, sizeof(message) - 1));
  message[length] = '\0';
  Serial.println(message);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, message)) return;

  const char* comando = doc["comando"];

  if      (strcmp(comando, "led_on")    == 0) { digitalWrite(LED_PIN, HIGH); Serial.println("-> LED encendido"); }
  else if (strcmp(comando, "led_off")   == 0) { digitalWrite(LED_PIN, LOW);  Serial.println("-> LED apagado");   }
  else if (strcmp(comando, "reiniciar") == 0) { Serial.println("-> Reiniciando..."); delay(3000); ESP.restart(); }
  else { Serial.print("-> Comando desconocido: "); Serial.println(comando); }
}

void connectWiFi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" OK");
}

void connectMQTT() {
  wifiClient.setCACert(ROOT_CA);
  mqttClient.setServer(IOT_HUB_HOST, MQTT_PORT);
  mqttClient.setCallback(onCommandReceived);
  mqttClient.setBufferSize(1024);

  snprintf(telemetryTopic, sizeof(telemetryTopic),
    "devices/%s/messages/events/$.ct=application%%2Fjson&$.ce=utf-8", DEVICE_ID);
  snprintf(c2dTopic, sizeof(c2dTopic),
    "devices/%s/messages/devicebound/#", DEVICE_ID);

  Serial.print("Conectando a IoT Hub");
  while (!mqttClient.connected()) {
    if (mqttClient.connect(DEVICE_ID, MQTT_USERNAME, SAS_TOKEN)) {
      Serial.println(" OK");
      mqttClient.subscribe(c2dTopic);
      Serial.println("Escuchando comandos C2D...");
    } else {
      Serial.print(" Error: "); Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}

void sendTelemetry() {
  StaticJsonDocument<128> doc;
  doc["temperatura"] = random(20, 35);
  doc["humedad"]     = random(40, 80);
  doc["led"]         = digitalRead(LED_PIN) ? "on" : "off";

  char payload[128];
  serializeJson(doc, payload);

  if (mqttClient.publish(telemetryTopic, payload)) {
    Serial.print("Enviado: "); Serial.println(payload);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  connectWiFi();
  connectMQTT();
}

void loop() {
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop(); // <- procesa mensajes C2D entrantes
  sendTelemetry();
  delay(5000);
}
