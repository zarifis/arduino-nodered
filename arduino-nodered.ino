/*
 * 
 * This detects advertising messages of BLE devices and compares it with stored MAC addresses. 
 * If one matches, it sends an MQTT message to swithc something
   Copyright <2017> <Andreas Spiess>
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
   
   Based on Neil Kolban's example file: https://github.com/nkolban/ESP32_BLE_Arduino
 */

#include "BLEDevice.h"
#include <WiFi.h>
#include <PubSubClient.h>

static BLEAddress *pServerAddress;

#define LED 22

BLEScan* pBLEScan;
BLEClient*  pClient;

String knownAddresses[] = {"ce:53:10:69:20:63", "d1:4f:f0:7a:ce:ff"};  //Needs lowercase
String mac1 = "ce:53:10:69:20:63"; // my keys
String mac2 = "d1:4f:f0:7a:ce:ff"; // my airpods case
bool device1Seen = false;
bool device2Seen = false;
unsigned long device1LastSeen;
unsigned long device2LastSeen;
bool known = false;
int currentStatus; // (0 = No device in range, 1 = dev1 in ragne, 2 = dev2 in range, 3 = both in range)
int previousStatus;

const char* ssid = "Shields on the Go";
const char* pass = "ineedwifi";

unsigned long entry;

WiFiClient wifiClient;
PubSubClient MQTTclient("192.168.1.9", 1883, NULL, wifiClient); 

void updateCurrentStatus(){
    if (!device1Seen && !device2Seen) {
      currentStatus = 0;
    }
    if (device1Seen && !device2Seen) {
      currentStatus = 1;
    }
    if (!device1Seen && device2Seen) {
      currentStatus = 2;
    }
    if (device1Seen && device2Seen) {
      currentStatus = 3;
    }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE message.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (strcmp(advertisedDevice.getName().c_str(), "Tile") == 0) {
        Serial.println("Tile BLE Advertisement received: ");   
        Serial.print(advertisedDevice.toString().c_str());
        Serial.print(" RSSI: ");  
        Serial.println(advertisedDevice.getRSSI());
        
        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
  
        known = false;
        for (int i = 0; i < (sizeof(knownAddresses) / sizeof(knownAddresses[0])); i++) {
          if (strcmp(pServerAddress->toString().c_str(), knownAddresses[i].c_str()) == 0) known = true;
        }
        if (known) {
          if (advertisedDevice.getRSSI() > -60) {
            if (strcmp(pServerAddress->toString().c_str(), mac1.c_str()) == 0) {
              Serial.print("Device 1 found in range: ");
              Serial.println(advertisedDevice.getRSSI());
              device1Seen = true;
              device1LastSeen = millis();
              updateCurrentStatus();
            }
            if (strcmp(pServerAddress->toString().c_str(), mac2.c_str()) == 0) {
              Serial.print("Device 2 found in range: ");
              Serial.println(advertisedDevice.getRSSI());
              device2Seen = true;
              device2LastSeen = millis();
              updateCurrentStatus();
            }
          }
          advertisedDevice.getScan()->stop();
        }
      }
    }
};

void WiFiStart(){
  // Connecting to WiFi network
  Serial.println("Starting Wifi...");
  delay(500);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  
  entry = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - entry >= 15000) esp_restart();
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
  Serial.println("Wifi started.");
}

void WiFiStop(){
  Serial.println("Stopping Wifi...");
  delay(500);
  WiFi.mode(WIFI_OFF);
  delay(500);
  Serial.println("Wifi stopped.");
}

void sendMessage() {

  //delay(500);
  //btStop(); // Stopping/Starting bluetooth seems to break it (stops seeing BLE ads)
  //delay(500);
  pBLEScan->setActiveScan(false); // doing this instead seems to work
  
  WiFiStart();

  // Connect to Losant MQTT broker
  Serial.println("Connect to Losant MQTT broker...");
  //device.connect(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET); 
  //client.connect() 
  
  //if (device.connected() == 0){
  //  Serial.println("Could not connect to LosAnt MQTT broker.");
  //  Serial.println(device.mqttClient.state());
  //}
  while (!MQTTclient.connected()){
    Serial.print("Attempting MQTT connection...");
    if (!MQTTclient.connect("topic","admin","admin")) {
      Serial.println("failed");
      Serial.println(MQTTclient.state());
      Serial.println("Retry in 3 seconds...");
      delay(3000);
    }
    else {
      Serial.println("Connected. Sending state.");
      /*
      StaticJsonBuffer<100> jsonBuffer;
      JsonObject& state = jsonBuffer.createObject();
      */
      String state;
      if (currentStatus==0){
        //state["device1"] = "None";
        state = "None";
      }
      if (currentStatus==1){
        //state["device1"] = "A";
        state = "A";
      }
      if (currentStatus==2){
        //state["device1"] = "B";
        state = "B";
      }
      if (currentStatus==3){
        //state["device1"] = "Both";
        state = "Both";
      }
      String payload = "{\"device1\":\"" + state + "\"}";
      // Send the state to node-red 
      MQTTclient.publish("IoWeMQTT", payload.c_str());
    }
  }
  
  WiFiStop();
  
  //delay(500);    
  //btStart(); // Stopping/Starting bluetooth seems to break it 
  //(does not see BLE advertisements after restart, though it worked at some point, not sure what I changed)
  //delay(500);
  pBLEScan->setActiveScan(true); // doing this instead seems to work
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  previousStatus = 0; // Neither device detected
  device1LastSeen = entry;
  device2LastSeen = entry;

  BLEDevice::init("");
  pClient  = BLEDevice::createClient();
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
}

void loop() {
  Serial.println();
  Serial.println("BLE Scan restarted...");
  pBLEScan->start(30);
  Serial.print("currentStatus: ");
  Serial.println(currentStatus);
  
  // Check if status has changed and notify platform if so
  if (currentStatus != previousStatus) {
    digitalWrite(LED, LOW);
    Serial.println("Status changed. Sending message to platform via WiFi");
    sendMessage();
    Serial.println("Waiting for 5 seconds");
    delay(5000);
    // Update status
    previousStatus = currentStatus;
  }
  else {
    Serial.println("Status has not changed");
  }
  
  // Mark devices as out of range if not seen for 15 seconds
  if (millis() - device1LastSeen >= 15000){
    device1Seen = false;
  }
  if (millis() - device2LastSeen >= 15000){
    device2Seen = false;
  }
  updateCurrentStatus();
}
