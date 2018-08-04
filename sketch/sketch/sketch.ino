
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>


MDNSResponder mdns;
WebSocketsServer webSocket = WebSocketsServer(81);

// Replace with your network credentials
const char* ssid = "xxxx";
const char* password = "xxxx";

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
  delay(1000);

  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(accessoryName);

  WiFi.begin(ssid, password);

  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // declare sensor as input
  pinMode(inputPin, INPUT);    

  if (mdns.begin(accessoryName, WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Web socket server started on port 81");

  // Add service to mdns-sd
  mdns.addService("oznu-platform", "tcp", 81);
  mdns.addServiceTxt("oznu-platform", "tcp", "type", "pir");
  mdns.addServiceTxt("oznu-platform", "tcp", "mac", WiFi.macAddress());

  checkForMotion();
}

void loop(void) {
  webSocket.loop();
  checkForMotion();
}
