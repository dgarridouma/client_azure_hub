#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOURSSID";
const char* WIFI_PASSWORD = "***";

// ── DPS ───────────────────────────────────────────────────────────────────────
const char* DPS_HOST  = "global.azure-devices-provisioning.net";
const char* ID_SCOPE  = "0ne00XXXXXX";           // del portal, Overview del DPS
const char* GROUP_KEY = "<clave-primaria-del-enrollment-group>";

// ── Se calculan/rellenan en setup() ───────────────────────────────────────────
char REGISTRATION_ID[32];   // MAC del ESP32, ej. "esp32-aabbccddeeff"
char DEVICE_KEY[64];        // clave derivada de GROUP_KEY + REGISTRATION_ID
char IOT_HUB_HOST[128];     // asignado por DPS, ej. "miHub.azure-devices.net"

// ── IoT Hub (se construyen tras el provisioning) ───────────────────────────────
char SAS_TOKEN[512];
char MQTT_USERNAME[256];
char telemetryTopic[128];
char c2dTopic[128];

const int MQTT_PORT = 8883;
int period = 10;

// Certificado raíz (DigiCert Global Root G2, válido para DPS e IoT Hub)
const char* ROOT_CA =
"-----BEGIN CERTIFICATE-----\n"
"...DigiCert Global Root G2...\n"
"-----END CERTIFICATE-----\n";

WiFiClientSecure wifiClient;
PubSubClient     mqttClient(wifiClient);


// ── Helpers ───────────────────────────────────────────────────────────────────

// URL-encode un string (necesario para los SAS tokens)
void urlEncode(const char* src, char* dst, size_t dstLen) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j < dstLen - 4; i++) {
    char c = src[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      dst[j++] = c;
    } else {
      snprintf(&dst[j], 4, "%%%02X", (unsigned char)c);
      j += 3;
    }
  }
  dst[j] = '\0';
}

// Calcula la clave derivada del dispositivo: HMAC-SHA256(GROUP_KEY, deviceId)
// Igual que el script Python del paso 4, pero ejecutado en el propio ESP32
void deriveDeviceKey(const char* groupKey, const char* deviceId,
                     char* derivedKey, size_t derivedKeyLen) {
  unsigned char decodedKey[64];
  size_t decodedLen = 0;
  mbedtls_base64_decode(decodedKey, sizeof(decodedKey), &decodedLen,
                        (unsigned char*)groupKey, strlen(groupKey));

  unsigned char hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, decodedKey, decodedLen);
  mbedtls_md_hmac_update(&ctx, (unsigned char*)deviceId, strlen(deviceId));
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  size_t outLen = 0;
  mbedtls_base64_encode((unsigned char*)derivedKey, derivedKeyLen,
                        &outLen, hmacResult, 32);
  derivedKey[outLen] = '\0';
}

// Genera un SAS token firmado con la clave indicada
// resource debe estar ya URL-encoded
void generateSASToken(const char* resource, const char* key, long expiry,
                      char* token, size_t tokenLen) {
  char toSign[256];
  snprintf(toSign, sizeof(toSign), "%s\n%ld", resource, expiry);

  unsigned char decodedKey[64];
  size_t decodedLen = 0;
  mbedtls_base64_decode(decodedKey, sizeof(decodedKey), &decodedLen,
                        (unsigned char*)key, strlen(key));

  unsigned char hmac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, decodedKey, decodedLen);
  mbedtls_md_hmac_update(&ctx, (unsigned char*)toSign, strlen(toSign));
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  char sig64[64];
  size_t sigLen = 0;
  mbedtls_base64_encode((unsigned char*)sig64, sizeof(sig64), &sigLen, hmac, 32);
  sig64[sigLen] = '\0';

  char sigEncoded[128];
  urlEncode(sig64, sigEncoded, sizeof(sigEncoded));

  snprintf(token, tokenLen,
    "SharedAccessSignature sr=%s&sig=%s&se=%ld",
    resource, sigEncoded, expiry);
}


// ── DPS Provisioning ──────────────────────────────────────────────────────────

bool     dpsAssigned = false;
char     dpsOperationId[128] = "";

// Callback MQTT del DPS: recibe la respuesta de registro
void dpsCallback(char* topic, byte* payload, unsigned int length) {
  char msg[512];
  memcpy(msg, payload, min(length, (unsigned int)(sizeof(msg) - 1)));
  msg[length] = '\0';

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, msg)) return;

  const char* status = doc["status"] | "";

  if (strcmp(status, "assigned") == 0) {
    const char* hub = doc["registrationState"]["assignedHub"] | "";
    strncpy(IOT_HUB_HOST, hub, sizeof(IOT_HUB_HOST) - 1);
    dpsAssigned = true;
    Serial.println("\nDPS asignado a: " + String(hub));

  } else if (strcmp(status, "assigning") == 0) {
    // DPS todavía procesando — guardar operationId para el poll
    const char* opId = doc["operationId"] | "";
    strncpy(dpsOperationId, opId, sizeof(dpsOperationId) - 1);
    Serial.println(" (asignando...)");
  }
}

// Conecta al DPS vía MQTT y obtiene el IoT Hub asignado
// Devuelve true si el provisioning tuvo éxito
bool provisionDevice() {
  // SAS token para DPS usando la clave derivada del dispositivo
  char resource[256], resourceEncoded[256];
  snprintf(resource, sizeof(resource), "%s/registrations/%s",
           ID_SCOPE, REGISTRATION_ID);
  urlEncode(resource, resourceEncoded, sizeof(resourceEncoded));

  char dpsSasToken[512];
  generateSASToken(resourceEncoded, DEVICE_KEY,
                   time(nullptr) + 3600, dpsSasToken, sizeof(dpsSasToken));

  char dpsUsername[256];
  snprintf(dpsUsername, sizeof(dpsUsername),
    "%s/registrations/%s/api-version=2019-03-31",
    ID_SCOPE, REGISTRATION_ID);

  WiFiClientSecure dpsClient;
  dpsClient.setCACert(ROOT_CA);
  PubSubClient dps(dpsClient);
  dps.setServer(DPS_HOST, MQTT_PORT);
  dps.setCallback(dpsCallback);
  dps.setBufferSize(1024);

  Serial.print("Contactando con DPS");
  int attempts = 0;
  while (!dps.connected() && attempts < 5) {
    if (dps.connect(REGISTRATION_ID, dpsUsername, dpsSasToken)) {
      Serial.print(" OK");
    } else {
      Serial.print(".");
      delay(3000);
      attempts++;
    }
  }
  if (!dps.connected()) {
    Serial.println("\nError: no se pudo conectar al DPS");
    return false;
  }

  // Suscribirse a respuestas y enviar la solicitud de registro
  dps.subscribe("$dps/registrations/res/#");
  char regPayload[128];
  snprintf(regPayload, sizeof(regPayload),
           "{\"registrationId\": \"%s\"}", REGISTRATION_ID);
  dps.publish("$dps/registrations/PUT/iotdps-register/?$rid=1", regPayload);

  // Esperar respuesta (máx. 15 segundos)
  // Si DPS responde 202 (assigning), hacer polling cada 3 segundos
  unsigned long start = millis();
  while (!dpsAssigned && millis() - start < 15000) {
    dps.loop();
    if (!dpsAssigned && strlen(dpsOperationId) > 0) {
      char pollTopic[256];
      snprintf(pollTopic, sizeof(pollTopic),
        "$dps/registrations/GET/iotdps-get-operationstatus/?$rid=1&operationId=%s",
        dpsOperationId);
      dps.publish(pollTopic, "");
      delay(3000);
    }
  }

  dps.disconnect();
  return dpsAssigned;
}


// ── NTP ───────────────────────────────────────────────────────────────────────

void syncNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Sincronizando NTP");
  time_t now = time(nullptr);
  while (now < 1000000000L) { delay(500); Serial.print("."); now = time(nullptr); }
  Serial.println(" OK");
}

void getTimestamp(char* buf, size_t len) {
  time_t now = time(nullptr);
  struct tm* t = gmtime(&now);
  strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", t);
}


// ── WiFi ──────────────────────────────────────────────────────────────────────

void connectWiFi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" OK");
}


// ── C2D handler ───────────────────────────────────────────────────────────────
// Igual que el original: {"period": 5, "message": "period changed"}

void message_handler(char* topic, byte* payload, unsigned int length) {
  Serial.println("\nMensaje recibido desde la nube:");
  char message[256];
  memcpy(message, payload, min(length, (unsigned int)(sizeof(message) - 1)));
  message[length] = '\0';

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, message)) {
    Serial.println("Error al parsear JSON");
    return;
  }
  period = doc["period"] | period;
  const char* msg = doc["message"] | "";
  if (strlen(msg) > 0) Serial.println(msg);
}


// ── MQTT IoT Hub ──────────────────────────────────────────────────────────────

void connectMQTT() {
  // SAS token para el IoT Hub usando la clave derivada del dispositivo
  char resource[256], resourceEncoded[256];
  snprintf(resource, sizeof(resource), "%s/devices/%s", IOT_HUB_HOST, REGISTRATION_ID);
  urlEncode(resource, resourceEncoded, sizeof(resourceEncoded));
  generateSASToken(resourceEncoded, DEVICE_KEY,
                   time(nullptr) + 86400, SAS_TOKEN, sizeof(SAS_TOKEN));

  snprintf(MQTT_USERNAME, sizeof(MQTT_USERNAME),
    "%s/%s/?api-version=2021-04-12", IOT_HUB_HOST, REGISTRATION_ID);

  snprintf(telemetryTopic, sizeof(telemetryTopic),
    "devices/%s/messages/events/$.ct=application%%2Fjson&$.ce=utf-8",
    REGISTRATION_ID);

  snprintf(c2dTopic, sizeof(c2dTopic),
    "devices/%s/messages/devicebound/#", REGISTRATION_ID);

  wifiClient.setCACert(ROOT_CA);
  mqttClient.setServer(IOT_HUB_HOST, MQTT_PORT);
  mqttClient.setCallback(message_handler);
  mqttClient.setBufferSize(1024);

  Serial.print("Conectando a IoT Hub");
  while (!mqttClient.connected()) {
    if (mqttClient.connect(REGISTRATION_ID, MQTT_USERNAME, SAS_TOKEN)) {
      Serial.println(" OK");
      mqttClient.subscribe(c2dTopic);
    } else {
      Serial.print(" Error: "); Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}


// ── Telemetría ────────────────────────────────────────────────────────────────

void sendTelemetry() {
  int temperature = random(25, 30);
  int humidity    = random(50, 100);
  int pressure    = random(900, 1100);
  char when[32];
  getTimestamp(when, sizeof(when));

  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"]    = humidity;
  doc["pressure"]    = pressure;
  doc["when"]        = when;
  doc["deviceId"]    = REGISTRATION_ID;  // útil para identificar el ESP32 en los datos

  char payload[200];
  serializeJson(doc, payload);

  Serial.print(temperature); Serial.print(" ");
  Serial.print(humidity);    Serial.print(" ");
  Serial.println(pressure);

  mqttClient.publish(telemetryTopic, payload);
}


// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Iniciar WiFi primero para que la MAC esté disponible
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Ahora sí leer la MAC
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(REGISTRATION_ID, sizeof(REGISTRATION_ID),
    "esp32-%02x%02x%02x%02x%02x%02x",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("Device ID: " + String(REGISTRATION_ID));

  // Calcular clave derivada
  deriveDeviceKey(GROUP_KEY, REGISTRATION_ID, DEVICE_KEY, sizeof(DEVICE_KEY));

  // Esperar a que conecte (lo que antes hacía connectWiFi())
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" OK");

  syncNTP();
  if (!provisionDevice()) {
    Serial.println("Error fatal: provisioning fallido. Reiniciando...");
    delay(5000);
    ESP.restart();
  }
  connectMQTT();
}

void loop() {
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();
  sendTelemetry();
  delay(period * 1000);
}
