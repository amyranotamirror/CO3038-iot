#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "DHT20.h"
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include <ArduinoOTA.h>

/* ========================================================================== */
/*                                PROGRAM SETUP                               */
/* ========================================================================== */
constexpr char WIFI_SSID[] = "Amyra";
constexpr char WIFI_PASSWORD[] = "66666666";

constexpr char TOKEN[] = "t8tby7r267y6q9pd7p85";

constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 9600U;

constexpr int16_t telemetrySendInterval = 10000U;
constexpr int16_t printTemperatureInterval = 1500U;
constexpr int16_t printHumidityInterval = 1000U;
constexpr int16_t readDHT20Interval = 1000U;
uint32_t temperature;
uint32_t humidity;
uint32_t previousDataSend;
uint32_t previousTemperaturePrint;
uint32_t previousHumidityPrint;

// WiFi Connection
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// Task handles
TaskHandle_t TaskPrintTemperatureHandle = NULL;
TaskHandle_t TaskPrintHumidityHandle = NULL;
TaskHandle_t TaskReadDHT20Handle = NULL;
TaskHandle_t TaskCheckConnectionHandle = NULL;
TaskHandle_t TaskSendDataHandle = NULL;

// DHT20 Sensor
DHT20 DHT;

/* ========================================================================== */
/*                               WIFI CONNECTION                              */
/* ========================================================================== */
void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // Attempting to establish a connection to the given WiFi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    // Delay 500ms until a connection has been successfully established
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}

const bool reconnect() {
  // Check to ensure we aren't connected yet
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }
  // If we aren't establish a new connection to the given WiFi network
  InitWiFi();
  return true;
}

/* ========================================================================== */
/*                        REAL-TIME OS TASK MANAGEMENT                        */
/* ========================================================================== */
void TaskTemperature(void *pvParameters) {
    while (1) {
      if(millis() - previousTemperaturePrint >= printTemperatureInterval){
        previousTemperaturePrint = millis();
        Serial.print("Temperature: ");
        Serial.print(temperature, 1);
        Serial.println(" °C");
      }
      vTaskDelay(pdMS_TO_TICKS(printTemperatureInterval));  // Delay 1500ms
    }
}

void TaskHumidity(void *pvParameters) {
    while (1) {
      if(millis() - previousHumidityPrint >= printHumidityInterval){
        previousHumidityPrint = millis();
        Serial.print("Humidity: ");
        Serial.print(humidity, 1);
        Serial.println(" %");
      }
      vTaskDelay(pdMS_TO_TICKS(printHumidityInterval));  // Delay 1500ms
    }
}

void TaskReadDHT20(void *pvParameters) {
    while (1) {
      if (millis() - DHT.lastRead() >= readDHT20Interval) {
          int status = DHT.read();
          temperature = DHT.getTemperature();
          humidity = DHT.getHumidity();
          Serial.println();
      }
      vTaskDelay(pdMS_TO_TICKS(readDHT20Interval));
    }
}

void checkConnection(void *pvParameters){
  while(1){
    if (!reconnect()) {
      return;
    }
  
    if (!tb.connected()) {
      Serial.print("Connecting to: ");
      Serial.print(THINGSBOARD_SERVER);
      Serial.print(" with token ");
      Serial.println(TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        Serial.println("Failed to connect");
        return;
      }
      tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
    }
    vTaskDelay(pdMS_TO_TICKS(4000U));
    tb.loop();
  }
}

void sendData(void *pvParameters){
  while(1){
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT20 sensor!");
    } else {
      tb.sendTelemetryData("temperature", temperature);
      tb.sendTelemetryData("humidity", humidity);
    }
    vTaskDelay(pdMS_TO_TICKS(telemetrySendInterval));
  }
}
/* ========================================================================== */
/*                              PROGRAM EXECUTION                             */
/* ========================================================================== */
void setup() {
    Serial.begin(SERIAL_DEBUG_BAUD);
    InitWiFi();
    Wire.begin();
    DHT.begin();

    // Create Tasks
    xTaskCreate(TaskTemperature, "printTemperature", 2048U, NULL, 1, &TaskPrintTemperatureHandle);
    xTaskCreate(TaskHumidity, "printHumidity", 2048U, NULL, 1, &TaskPrintHumidityHandle);
    xTaskCreate(TaskReadDHT20, "readDHT20", 2048U, NULL, 1, &TaskReadDHT20Handle);
    xTaskCreate(checkConnection, "checkConnection", 4096U, NULL, 1, &TaskCheckConnectionHandle);
    xTaskCreate(sendData, "sendData", 2048U, NULL, 1, &TaskSendDataHandle);
}

void loop() {
  
}