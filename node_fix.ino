#include <painlessMesh.h>
#include <ArduinoJson.h>
#include "soc/rtc_cntl_reg.h"

#define MESH_PREFIX "XirkaMesh"
#define MESH_PASSWORD "12345678"
#define MESH_PORT 5555

String receivedData;
Scheduler userScheduler;
painlessMesh mesh;

StaticJsonDocument<200> setLampDoc;

unsigned long t1;
uint32_t sender;
uint8_t status;

void setLamp() {
  if (status == 1) {
    setLampDoc["LAMP"] = status;
    String jsonLedOn;
    serializeJson(setLampDoc, jsonLedOn);
    Serial2.println(jsonLedOn);
    Serial.println(jsonLedOn);
  } else if (status == 0) {
    setLampDoc["LAMP"] = status;
    String jsonLedOff;
    serializeJson(setLampDoc, jsonLedOff);
    Serial2.println(jsonLedOff);
    Serial.println(jsonLedOff);
  }
}

void sendMessage() {
  String msg = receivedData;
  uint32_t destId = 486284217;
  mesh.sendSingle(destId, msg);
  Serial.printf("Data Sent to: %u msg=%s\n", destId, msg.c_str());
}

void sendAck() {
  uint32_t nodeId = mesh.getNodeId();
  String ackMessage = "Ack Message from: " + String(nodeId);
  mesh.sendSingle(sender, ackMessage);
  Serial.println(ackMessage);
}

void receivedCallback(uint32_t from, String &msg) {
  uint32_t nodeId = mesh.getNodeId();
  Serial.printf("Node ID: %u received from %u msg=%s\n", nodeId, from, msg.c_str());
  sender = from;
  receivedData = msg.c_str();

  DynamicJsonDocument doc(200);
  deserializeJson(doc, msg);
  status = doc["LAMP"];
  // Serial.println(status);
  
  setLamp();
  sendAck();
  sendMessage();
}


void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // RX2=16, TX2=17

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

void loop() {
  mesh.update();
  userScheduler.execute();
}
