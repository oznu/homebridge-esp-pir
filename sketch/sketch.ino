#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

WebSocketsServer webSocket = WebSocketsServer(81);

// Hostname
const char* accessoryName = "homebridge-pir";

int inputPin = 0;

#if defined(ESP32)
int inputPin = 19;
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
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      JsonObject root = jsonBuffer.as<JsonObject>();

      if (root.containsKey("noMotionDelay")) {
        noMotionDelay = root["noMotionDelay"];
        Serial.printf("Set no motion delay to %u\n", noMotionDelay);
      }

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
  DynamicJsonDocument jsonBuffer(1024);
  JsonObject root = jsonBuffer.to<JsonObject>();

  if (pirState == HIGH) {
    root["motion"] = true;
  } else {
    root["motion"] = false;
  }

  String res;
  serializeJson(root, res);

  Serial.println(res);

  webSocket.broadcastTXT(res);
}

void checkForMotion() {
  // read input value
  val = digitalRead(inputPin);
  currentMillis = millis();

  // check if the input is HIGH
  if (val == HIGH) {
    
    // set time when motion was last detected
    lastMotionDetected = currentMillis;
    
    if (pirState == LOW) {
      Serial.println("Motion detected!");
      pirState = HIGH;
      sendUpdate();
    }
    
  } else {

    // only broadcast change if there has been no motion for x milliseconds
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

  // turn LED on at boot
  digitalWrite(LED_BUILTIN, LOW);

  delay(1000);

  Serial.begin(115200);

  #if defined(ESP8266)
  WiFi.mode(WIFI_STA);
  #endif

  Serial.println("");

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // sets timeout until configuration portal gets turned off
  wm.setTimeout(600);

  // Wait for connection
  if (!wm.autoConnect(accessoryName, "password"))
  {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);

    // reset and try again
    ESP.restart();
    delay(5000);
  }

  #if defined(ESP8266)
  WiFi.hostname(accessoryName);
  #endif

  if (!MDNS.begin(accessoryName)) {
    Serial.println("Error starting mDNS");
    return;
  }

  // declare sensor as input
  pinMode(inputPin, INPUT);    

  Serial.println("MDNS responder started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Web socket server started on port 81");

  // Add service to mdns-sd
  MDNS.addService("oznu-platform", "tcp", 81);
  MDNS.addServiceTxt("oznu-platform", "tcp", "type", "pir");
  MDNS.addServiceTxt("oznu-platform", "tcp", "mac", WiFi.macAddress());

  checkForMotion();

  // turn LED off once ready
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop(void) {
  webSocket.loop();
  checkForMotion();
  #if defined(ESP8266)
  MDNS.update();
  #endif
}
