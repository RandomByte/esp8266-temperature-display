#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include "SH1106Wire.h"
#include "OLEDDisplayUi.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Copy over "_constants.h.example" to "_constants.h" and update it with values suitable
//	for your network
#include "_constants.h"
#include "font.h"

#define I2C_SDA 4
#define I2C_SCL 5

SH1106Wire display(0x3c, I2C_SDA, I2C_SCL);
OLEDDisplayUi ui ( &display );

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* mqtt_server = MQTT_SERVER_IP;
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
	delay(10);
	// We start by connecting to a WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
	delay(500);
	Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

void reconnect() {
	// Loop until we're reconnected
	while (!client.connected()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect("ESP8266 temperature display")) {
			Serial.println("connected");
      client.subscribe("Home/Temperature");
      client.subscribe("Home/Humidity");
		} else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

String lastTemp = "-";
String lastHum = "-";

const int tempHistLength = 13;
const int historyInterval = 600;
int tempHist[tempHistLength];
int tempHistIdx = 0;
int tempHistShift = -1;
long lastHistoryStore = 0;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg = msg + (char)payload[i];
  }
  Serial.println(msg);
  if (String(topic) == "Home/Temperature") {
    lastTemp = msg;

    long now = millis();
    if (now - lastHistoryStore > historyInterval) {
      lastHistoryStore = now;

      if (tempHistShift >= 0) {
        tempHistShift = tempHistIdx;
      }
      tempHist[tempHistIdx++] = (payload[0] - '0') * 100 + (payload[1] - '0') * 10 + (payload[3] - '0');
      if (tempHistIdx == tempHistLength) {
        tempHistShift = tempHistLength - 1;
        tempHistIdx = 0;
      }
    }
  } else if (String(topic) == "Home/Humidity") {
    lastHum = msg;
  } else {
    Serial.print("Unkown topic: ");
    Serial.print(topic);
    Serial.println();
  }
}

int getMinNotNull(int* array, int size)
{
  int minimum = array[0];
  for (int i = 0; i < size; i++)
  {
    if (array[i] < minimum && array[i] != 0) minimum = array[i];
  }
  return minimum;
}

int getMax(int* array, int size)
{
  int maximum = array[0];
  for (int i = 0; i < size; i++)
  {
    if (array[i] > maximum) maximum = array[i];
  }
  return maximum;
}

void tempFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(DejaVu_Sans_Mono_16);
  display->drawString(0, 0, lastTemp + " °C");

  int minVal = getMinNotNull(tempHist, tempHistLength);
  int maxVal = getMax(tempHist, tempHistLength);

  int lastXDeviation;
  for (int i = 0; i < tempHistLength; i++) {
  //for (int i = tempHistLength - 1; i >=0; i--) {
    int idx = i;
    if (tempHistShift >= 0) {
      idx = idx + tempHistShift;
    }
    // Serial.print(idx);
    // Serial.print("->");
    if (idx > tempHistLength - 1) {
      idx = idx - tempHistLength;
    }

    // Serial.print(idx);
    // Serial.print(", ");
    int val = tempHist[idx];
    if (val == 0) {
      continue;
    }
    int xDeviation = map(val, minVal, maxVal, -5, 5);
    if (i == 0) {
      lastXDeviation = xDeviation;
      continue;
    }
    display->drawLine((i - 1) * 10, 25 - lastXDeviation, (i - 1) * 10 + 10, 25 - xDeviation);
    lastXDeviation = xDeviation;
  }
  // Serial.println();

  display->setFont(DejaVu_Sans_Mono_8);
  display->drawString(0, 34, "(" + String(minVal / 10) + " °C - " + String(maxVal / 10) + " °C)");

  display->setFont(DejaVu_Sans_Mono_16);
  display->drawString(0, 48, lastHum + " %");
}

FrameCallback frames[] = { tempFrame};

const int frameCount = 1;

void setup() {
	Serial.begin(9600);
	Serial.println("wow");

  ui.setTargetFPS(30);
  ui.setFrames(frames, frameCount);
  ui.disableAllIndicators();
  ui.init();
  display.flipScreenVertically();

	setup_wifi();
	client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
}

void loop() {
  int remainingTimeBudget = ui.update();
  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    delay(remainingTimeBudget);
  }
}
