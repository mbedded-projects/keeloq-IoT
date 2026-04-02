// #define DEBUG
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <keeloq.h>
#include <config.h>
#include <utilities.h>

// Gate Controller
GateController gateController(GATE_PIN, OPEN_SENSOR, OPENING_SENSOR, CLOSED_SENSOR, CLOSING_SENSOR);

// CC1101
KeeLoq radio(CC1101_CS_PIN, CC1101_IRQ_PIN, CC1101_RST_PIN);

// MQTT
String topic = String(AIO_USERNAME) + "/feeds/" + AIO_FEED;
WiFiClientSecure net;
PubSubClient mqtt(net);

void callbackController(GateState newState)
{
  log("[Controller] Stan bramy został zmieniony na: %s\n", gateController.stateToString(newState));
  mqtt.publish(topic.c_str(), gateController.stateToString(newState)); // bezpieczne w przypadku braku połączenia
}

void callbackMQTT(char *t, byte *payload, unsigned int len)
{
  log("[MQTT] Wiadomość na temacie: %s\n       Treść: %s\n", t, parseToString(payload, len).c_str());
  if (parseToString(payload, len) == "toggle")
    gateController.toggleGate();
}

void callbackRadio(Frame &frame, const RawFrame &rawFrame, const uint32_t durations[], int n)
{
    log("[Radio] Ramka: %s", radio.frameToString(frame).c_str());
    gateController.toggleGate();
}

void connectWiFi()
{
  log("[WiFi] Łączenie do %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    log(".");
  }
  log("\n[WiFi] IP: %s", WiFi.localIP().toString().c_str());
}

void connectMQTT()
{
  if(!WiFi.isConnected()) connectWiFi();
  while (!mqtt.connected())
  {
    log("[MQTT] Łączenie...");
    if (mqtt.connect(clientId, AIO_USERNAME, AIO_KEY))
    {
      log(" OK\n");
      mqtt.publish(topic.c_str(), gateController.stateToString(gateController.getCurrentState())); // po połączeniu aktualizujemy stan bramy
      mqtt.subscribe(topic.c_str());
      log("[MQTT] Subskrybuję: %s\n", topic.c_str());
    }
    else
    {
      log(" FAILED, rc=%d - spróbuję za 5s\n", mqtt.state());
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

void radioTask(void*) {
  // Konfiguracja radia
  radio.setFilteredRemotes(remotes);
  radio.setReceiveCallback(callbackRadio);
  if (radio.begin() != 0)
  {
    log("\n[Radio] Radio init error...");
    while (1)
      vTaskDelay(pdMS_TO_TICKS(100));
  }
  log("\n[Radio] Rozpoczynam nasluch KeeLoq...\n");

  // Pętla radia
  for (;;) {
    radio.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void wifiTask(void*) {
  // Konfiguracja WiFi
  connectWiFi();
  net.setCACert(AIO_ROOT_CA);  // TLS

  // konfiguracja OTA
  ArduinoOTA.setHostname("GateESP");
  ArduinoOTA.begin();

  // Konfiguracja MQTT
  mqtt.setServer(AIO_SERVER, AIO_PORT);
  mqtt.setCallback(callbackMQTT);

  for (;;) {
    if (!mqtt.connected()) connectMQTT();
    mqtt.loop();
    ArduinoOTA.handle();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void controllerTask(void*) {
  // Konfiguracja kontrolera
  gateController.setCallback(callbackController);
  gateController.begin();

  // Pętla kontrolera
  for (;;) {
    gateController.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  log_begin(115200);

  xTaskCreatePinnedToCore(radioTask,      "Radio",      16384, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(wifiTask,       "WiFi",       8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(controllerTask, "Controller", 4096, NULL, 1, NULL, 0);
}

void loop()
{
  vTaskDelay(10 / portTICK_PERIOD_MS);
}
