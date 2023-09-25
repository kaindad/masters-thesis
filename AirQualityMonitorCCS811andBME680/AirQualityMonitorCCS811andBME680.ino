/*********
Author: Kainda Kiniel DAKA
Project: Air Quality Monitoring for a Poultry System to be used for Precision Agriculture

*********/

#include <Wire.h>
#include <PubSubClient.h>
#include <WiFiNINA.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <ArduinoJson.h>
#include "Adafruit_CCS811.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display definitions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SSD1306_I2C_ADDRESS 0x3C 

#define SEALEVELPRESSURE_HPA (1013.25)
#define MQTT_PORT 8883
#define MQTT_CLIENT_ID "iot33-air-quality"


//Initialize the SSD1306 OLED Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initialize the BME280 Pressure sensor
Adafruit_BME680 bme;  // I2C

// Initilaize the CCS811 gas sensor
Adafruit_CCS811 ccs;  //I2C

// Initializes the WIFI sslClient
WiFiSSLClient sslClient;

// Initialize the PubSubClient for MQTT subscriptions
PubSubClient client(sslClient);


// Change the credentials below, to connect to the router
const char* ssid = "Vodafone-57D4";
const char* password = "FJymxE7etCzmHaZa";

// MQTT broker credentials
const char* MQTT_username = "iot33-air-quality";
const char* MQTT_password = "password1234";

// Change the variable to the IP address of the MQTT server
const char* mqtt_server = "70yyej.stackhero-network.com";

unsigned long delayTime;

// Variables for the air quality
float pressure;
float altitude;
float humidity;
float temperature;
float gasResistance;
int co2 = 0;
int tvoc = 0;

// Timers auxilliary variables
long now = millis();
long lastMeasure = 0;

void setup() {
  Serial.begin(9600);
  
  setupOledDisplay();
  setupCCS811Sensor();
  delay(500);
  setupBME680Sensor();
  delay(500);

  setupWIFI();
  client.setServer(mqtt_server, MQTT_PORT);

  delayTime = 60000;

  Serial.println();
}

void setupOledDisplay(){
  // Initialize the OLED display
  if(!display.begin(SSD1306_I2C_ADDRESS, OLED_RESET, SCREEN_WIDTH, SCREEN_HEIGHT)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.display();
}

//Initialize CCS811 Sensor
void setupCCS811Sensor() {
  //Initialize CCS811 sensor
  Serial.println("CCS811 setup");
  if (!ccs.begin()) {
    Serial.println("Failed to start CCS811 sensor! Please check your wiring.");
    while (1)
      ;
  }

  // Wait for the sensor to be ready
  while (!ccs.available())
    ;
}

//Initialize BME680 Sensor
void setupBME680Sensor() {
  while (!Serial)
    ;

  if (!bme.begin(0x77)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1)
      ;
  }
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // 320*C for 150 ms
}

// This functions connects your ESP8266 to your router
void setupWIFI() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - Arduino IP address: ");
  Serial.println(WiFi.localIP());
}

//
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_username, MQTT_password)) {
      Serial.println("connected");
      // Subscribe or resubscribe to a topic
      // You can subscribe to more topics (to control more LEDs in this example)
      client.subscribe("device/air_quality");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      client.disconnect();
      delay(5000);
    }
  }
}

void displayValuesOnOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Temp: "); display.print(temperature); display.println(" *C");
  display.print("Pressure: "); display.print(pressure); display.println(" hPa");
  display.print("Altitude: "); display.print(altitude); display.println(" m");
  display.print("Humidity: "); display.print(humidity); display.println(" %");
  display.print("Gas: "); display.print(gasResistance); display.println(" KOhms");
  display.print("CO2: "); display.print(co2); display.println(" ppm");
  display.print("TVOC: "); display.print(tvoc);
  display.display();
}

void printToSerial(){
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(altitude);
  Serial.println(" m");

  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Gas = ");
  Serial.print(gasResistance);
  Serial.println(" KOhms");

  Serial.print("CO2: ");
  Serial.print(co2);
  Serial.print("ppm, TVOC: ");
  Serial.println(tvoc);
}

void sendAirQualityValues(PubSubClient client) {

  pressure = bme.readPressure() / 100.0F;
  altitude = 0;
  humidity = bme.readHumidity();
  temperature = bme.readTemperature();
  gasResistance = bme.gas_resistance / 1000.0;

  if (isnan(pressure)) {
    Serial.println("Pressure reading is null. Failed to read altitude");
  } else {
    altitude = bme.readAltitude(pressure);
  }

  if (ccs.available()) {
    if (!ccs.readData()) {
      co2 = ccs.geteCO2();
      tvoc = ccs.getTVOC();
    } else {
      Serial.println("ERROR!");
      while (1)
        ;
    }
  }

  StaticJsonDocument<128> airQualityDocument;
  airQualityDocument["air_pressure"] = pressure;
  airQualityDocument["altitude"] = altitude;
  airQualityDocument["humidity"] = humidity;
  airQualityDocument["temperature"] = temperature;
  airQualityDocument["tvoc"] = tvoc;
  airQualityDocument["co2"] = co2;
  airQualityDocument["gas_resistance"] = gasResistance;

  char output[256];
  int b = serializeJson(airQualityDocument, output);

  client.publish("device/air_quality", output);

  Serial.println();
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  if (!client.loop())
    client.connect(MQTT_CLIENT_ID, MQTT_username, MQTT_password);

  now = millis();

  sendAirQualityValues(client);
  printToSerial();
  displayValuesOnOLED();
  delay(delayTime);
  client.flush();
}
