
#include <ESP8266WiFi.h>
#include <DNSServer.h>              // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>       // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>            // https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

WebSocketsServer webSocket = WebSocketsServer(81);

// Hostname
const char* accessoryName = "homebridge-pir";

// choose the input pin (for PIR sensor)
int inputPin = 0;

int noMotionDelay = 30000;

int pirState = HIGH;
int val = 0;
unsigned long lastMotionDetected;
unsigned long currentMillis;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected from url: %s\r\n", num, payload);
      sendUpdate();
      break;
    case WStype_TEXT: {
      Serial.printf("[%u] get Text: %s\r\n", num, payload);

      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject((char *)&payload[0]);

      if (root.containsKey("noMotionDelay")) {
        noMotionDelay = root["noMotionDelay"];
        Serial.printf("Set no motion delay to %u\r\n", noMotionDelay);
      }

      break;
    }
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      break;
    case WStype_PING:
      // Serial.printf("[%u] Got Ping!\r\n", num);
      break;
    case WStype_PONG:
      // Serial.printf("[%u] Got Pong!\r\n", num);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}

void sendUpdate() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  if (pirState == HIGH) {
    root["motion"] = true;
  } else {
    root["motion"] = false;
  }

  String res;
  root.printTo(res);

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

  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(accessoryName);

  Serial.println("");

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // sets timeout until configuration portal gets turned off
  wm.setTimeout(600);

  // Wait for connection
  // first parameter is name of access point, second is the password
  if (!wm.autoConnect(accessoryName, "password"))
  {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);

    // reset and try again
    ESP.reset();
    delay(5000);
  }

  WiFi.hostname(accessoryName);

  // declare sensor as input
  pinMode(inputPin, INPUT);    

  if (MDNS.begin(accessoryName, WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

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
  MDNS.update();
}
