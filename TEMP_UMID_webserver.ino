#include "Secrets.h" //API KEY - WIFI INFO

#include <WiFi.h>
#include <ThingSpeak.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 12
#define DHTTYPE DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);

WiFiClient client;

unsigned long channelNumber = SECRET_CHANNEL_NUMBER;
const char * writeAPIKey = SECRET_WRITE_APIKEY; 

float temperatura = 0;
float umidade = 0;

// Mutex para proteger acesso às variáveis compartilhadas
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Protótipos
void leituraSensor(void * parameter);
void envioThingSpeak(void * parameter);

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(SECRET_SSID, SECRET_PASSWORD);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado!");
  ThingSpeak.begin(client);

  // Criação das tasks em núcleos diferentes
  xTaskCreatePinnedToCore(leituraSensor, "Leitura DHT", 2048, NULL, 1, NULL, 0); // Núcleo 0
  xTaskCreatePinnedToCore(envioThingSpeak, "Envio TS", 4096, NULL, 1, NULL, 1);  // Núcleo 1
}

void loop() {
  delay(100); // Pequeno delay para não travar o watchdog
}

// Task 1 - Leitura do DHT11
void leituraSensor(void * parameter) {
  for (;;) {
    sensors_event_t event;

    dht.temperature().getEvent(&event);
    if (!isnan(event.temperature)) {
      portENTER_CRITICAL(&mux);
      temperatura = event.temperature;
      portEXIT_CRITICAL(&mux);
      Serial.print("[Leitura] Temperatura: ");
      Serial.println(event.temperature);
    } else {
      Serial.println("[Leitura] Erro de temperatura!");
    }

    dht.humidity().getEvent(&event);
    if (!isnan(event.relative_humidity)) {
      portENTER_CRITICAL(&mux);
      umidade = event.relative_humidity;
      portEXIT_CRITICAL(&mux);
      Serial.print("[Leitura] Umidade: ");
      Serial.println(event.relative_humidity);
    } else {
      Serial.println("[Leitura] Erro de umidade!");
    }

    vTaskDelay(pdMS_TO_TICKS(5000)); // Delay de 5 segundos
  }
}

// Task 2 - Envio para o ThingSpeak
void envioThingSpeak(void * parameter) {
  for (;;) {
    float t, u;

    // Copia segura das variáveis (evita concorrência)
    portENTER_CRITICAL(&mux);
    t = temperatura;
    u = umidade;
    portEXIT_CRITICAL(&mux);

    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, u);
    int status = ThingSpeak.writeFields(channelNumber, writeAPIKey);

    if (status == 200) {
      Serial.println("[Envio] Dados enviados com sucesso.");
    } else {
      Serial.print("[Envio] Erro: ");
      Serial.println(status);
    }

    vTaskDelay(pdMS_TO_TICKS(15000)); // Envia a cada 15 segundos
  }
}
