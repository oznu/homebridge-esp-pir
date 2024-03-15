#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#endif

#include <DNSServer.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

WebSocketsServer webSocket = WebSocketsServer(81);

// Hostname
const char* accessoryName = "homebridge-pir";

#if defined(ESP8266)
int inputPin = D6;
#else
int inputPin = 19;
#endif

#if !defined(LED_BUILTIN)
int LED_BUILTIN = 2;
#endif

int noMotionDelay = 30000;

int pirState = HIGH;
int val = 0;
unsigned long lastMotionDetected;
unsigned long currentMillis;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected from url: %s\n", num, payload);
      sendUpdate();
      break;
    case WStype_TEXT: {
      Serial.printf("[%u] get Text: %s\n", num, payload);

      DynamicJsonDocument jsonBuffer(1024);
      DeserializationError error = deserializeJson(jsonBuffer, (char *)&payload[0]);
      if (error) {
        Serial.println("deserializeJson() failed: " + String(error.c_str()));
        return;
      }
      
      #if defined(ESP8266)
        if (jsonBuffer.containsKey("noMotionDelay")) {
          noMotionDelay = root["noMotionDelay"];
          Serial.printf("Set no motion delay to %u\n", noMotionDelay);
        }
      #else
        JsonObject root = jsonBuffer.as<JsonObject>();
        if (root.containsKey("noMotionDelay")) {
          noMotionDelay = root["noMotionDelay"];
          Serial.printf("Set no motion delay to %u\n", noMotionDelay);
        }
      #endif
      break;
    }
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\n", num, length);
      break;
    case WStype_PING:
      Serial.printf("[%u] Got Ping!\n", num);
      break;
    case WStype_PONG:
      Serial.printf("[%u] Got Pong!\n", num);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\n", type);
      break;
  }
}

void sendUpdate() {
  #if defined(ESP8266)
    DynamicJsonDocument doc(1024);
  #else
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject doc = jsonBuffer.to<JsonObject>();
  #endif

  if (pirState == HIGH) {
    doc["motion"] = true;
  } else {
    doc["motion"] = false;
  }

  String res;
  serializeJson(doc, res);

  Serial.println(res);
  webSocket.broadcastTXT(res);
}

void checkForMotion() {
  val = digitalRead(inputPin);
  currentMillis = millis();

  if (val == HIGH) {
    lastMotionDetected = currentMillis;
    if (pirState == LOW) {
      Serial.println("Motion detected!");
      pirState = HIGH;
      sendUpdate();
    }
  } else {
    if (currentMillis - lastMotionDetected >= noMotionDelay) {
      if (pirState == HIGH){
        Serial.println("Motion ended!");
        pirState = LOW;
        sendUpdate();
      }
    }
  }
}

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);

  #if defined(ESP8266)
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    WiFi.mode(WIFI_STA);
    // WiFi.hostname(accessoryName);
  #else
    Serial.begin(115200);
  #endif

  WiFi.hostname(accessoryName);
  Serial.println("");

  WiFiManager wm;
  wm.setTimeout(600);

  if (!wm.autoConnect(accessoryName, "password")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  pinMode(inputPin, INPUT);
  #if defined(ESP8266)
    Serial.println("Starting MDNS");
    while (!MDNS.begin(accessoryName, WiFi.localIP())) {
      delay(250);
      Serial.print('.');
    }
  #endif
  Serial.println("MDNS responder started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Web socket server started on port 81");

  MDNS.addService("oznu-platform", "tcp", 81);
  MDNS.addServiceTxt("oznu-platform", "tcp", "type", "pir");
  MDNS.addServiceTxt("oznu-platform", "tcp", "mac", WiFi.macAddress());

  checkForMotion();
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop(void) {
  webSocket.loop();
  checkForMotion();
  #if defined(ESP8266)
    MDNS.update();
  #endif
}