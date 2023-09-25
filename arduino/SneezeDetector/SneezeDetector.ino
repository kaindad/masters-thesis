#include <Wire.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <edge-impulse-sdk/classifier/ei_run_classifier.h>
#include <ArduinoJson.h>

// WiFi and MQTT settings
#define WLAN_SSID       "your_SSID"
#define WLAN_PASS       "your_PASSWORD"
#define MQTT_SERVER     "your_MQTT_SERVER"
#define MQTT_PORT       1883
#define MQTT_USERNAME   "your_MQTT_USERNAME"
#define MQTT_PASSWORD   "your_MQTT_PASSWORD"

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD);
Adafruit_MQTT_Publish sneezePublisher = Adafruit_MQTT_Publish(&mqtt, "sneeze_sounds");

void setup() {
  Serial.begin(115200);
  delay(10);

  // Connect to WiFi
  Serial.println(F("Connecting to WiFi..."));
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("Connected!"));

  // Connect to MQTT
  connectToMQTT();

  // Initialize Edge Impulse
  ei_impulse_init();
}

void loop() {
  // Ensure MQTT connection
  if (!mqtt.connected()) {
    connectToMQTT();
  }
  mqtt.processPackets(10);

  // Run the classifier
  ei_impulse_result_t result;
  EI_IMPULSE_ERROR res = run_classifier(&result, false);
  if (res != 0) {
    Serial.println("Failed to run classifier");
    return;
  }

  // Check if sneeze is detected
  if (result.classifications[0].value > 0.7) { // Assuming the first class is "sneeze" and threshold is 0.7
    sendSneezeDetected(result);
    delay(1000); // Basic debounce to avoid multiple detections
  }
}

void connectToMQTT() {
  while (!mqtt.connected()) {
    Serial.print(F("Connecting to MQTT... "));
    if (mqtt.connect()) {
      Serial.println(F("Connected!"));
    } else {
      Serial.print(F("Failed, rc="));
      Serial.print(mqtt.connectErrorString());
      Serial.println(F("Retrying in 5 seconds..."));
      delay(5000);
    }
  }
}

void sendSneezeDetected(ei_impulse_result_t result) {
  StaticJsonDocument<256> doc; // Adjust the size based on your needs

  doc["event"] = "sneeze_detected";
  doc["timestamp"] = millis();

  JsonArray predictions = doc.createNestedArray("predictions");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    JsonObject prediction = predictions.createNestedObject();
    prediction["label"] = result.classification[ix].label;
    prediction["value"] = result.classification[ix].value;
  }

  String payload;
  serializeJson(doc, payload);

  sneezePublisher.publish(payload.c_str());
  Serial.println(F("Sneeze detected and sent to MQTT!"));
}
