#include <painlessMesh.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "soc/rtc_cntl_reg.h"

#define MESH_PREFIX "XirkaMesh"
#define MESH_PASSWORD "12345678"
#define MESH_PORT 5555

uint8_t nodeNumber;
char buffValue[100];
StaticJsonDocument<200> doc;
StaticJsonDocument<200> newDoc;
StaticJsonDocument<200> controlLedDoc;

unsigned long t1;
unsigned long tl;
uint32_t sender;
uint32_t newId;
bool flag = 0;
String receivedData;

Scheduler userScheduler;
painlessMesh mesh;

const int MAX_NEW_CONNECTIONS = 1;
uint32_t newNodes[MAX_NEW_CONNECTIONS] = {};
int jumlahNodes = 0;

const int MAX_NEXT_CONNECTION = 1;
uint32_t nextNodes[MAX_NEXT_CONNECTION];
int jumlahNextNodes = 0;

void ledSetup() {
  uint8_t msg = 1;
  controlLedDoc["LAMP"] = msg;
  String jsonLedOn;
  serializeJson(controlLedDoc, jsonLedOn);
  Serial2.println(jsonLedOn);
}

//function for receive data from microcontroller and send data via mesh
void receiveSerial2() {
  while (Serial2.available() > 0) {
    static uint8_t x = 0;
    buffValue[x] = (char)Serial2.read();
    x += 1;
    if (buffValue[x - 1] == '\n') {
      DeserializationError error = deserializeJson(doc, buffValue);
      if (!error) {
        float vsolar = doc["VSOLAR"];
        float vbat = doc["VBAT"];

        ledSetup();
        nodeNumber = 1;

        newDoc["Node"] = nodeNumber;
        newDoc["VSOLAR"] = round(vsolar * 100.0) / 100.0;
        newDoc["VBAT"] = round(vbat * 100.0) / 100.0;  // Round vbat to two decimal places
        newDoc["LAMP"] = (controlLedDoc["LAMP"] == 1) ? "ON" : "OFF";

        String jsonString;
        serializeJson(newDoc, jsonString);
        mesh.sendSingle(sender, jsonString);
        Serial.println("Data: " + jsonString);
      }
      x = 0;
      memset(buffValue, 0, sizeof(buffValue));
      break;
    }
  }
}

void sendMessage() {
  String msg = receivedData;
  uint32_t targetNode;

  bool isFromNewNode = false;
  for (int i = 0; i < jumlahNodes; i++) {
    if (sender == newNodes[i]) {
      isFromNewNode = true;
      break;
    }
  }

  if (isFromNewNode && jumlahNextNodes > 0) {
    for (int i = 0; i < jumlahNextNodes; i++) {
      mesh.sendSingle(nextNodes[i], msg);
      Serial.printf("Data Sent to: %u msg=%s\n", nextNodes[i], msg.c_str());
    }
  } else if (!isFromNewNode && jumlahNodes > 0 && jumlahNextNodes > 0) {
    for (int i = 0; i < jumlahNodes; i++) {
      mesh.sendSingle(newNodes[i], msg);
      Serial.printf("Data Sent to: %u msg=%s\n", newNodes[i], msg.c_str());
    }
  }

  if (jumlahNextNodes > 0) {
    for (int i = 0; i < jumlahNextNodes; i++) {
      if (!mesh.isConnected(nextNodes[i])) {
        Serial.printf("Next Node %u tidak terhubung. Menghapus dari daftar.\n", nextNodes[i]);
        for (int i = 0; i < MAX_NEXT_CONNECTION; i++) {
          nextNodes[i] = 0;
        }
        jumlahNextNodes = 0;
        Serial.printf("Jumlah Next Node: %d\n", jumlahNextNodes);
      }
    }
  }
}

void checkConnectionStatus() {
  if (jumlahNodes > 0) {
    for (int i = 0; i < jumlahNodes; i++) {
      if (!mesh.isConnected(newNodes[i])) {
        Serial.printf("Normal Node %u tidak terhubung. Menghapus dari daftar.\n", newNodes[i]);
        for (int j = i; j < jumlahNodes - 1; j++) {
          newNodes[j] = newNodes[j + 1];
        }
        jumlahNodes--;
        Serial.printf("Jumlah Normal Node Terhubung: %d\n", jumlahNodes);
      }
    }
  }
}

void saveNode() {
  if (jumlahNodes < MAX_NEW_CONNECTIONS) {
    newNodes[jumlahNodes++] = newId;
    Serial.printf("Node baru disimpan di Nodes indeks ke-%d\n", jumlahNodes - 1);
    Serial.printf("Normal Node: %d\n", newNodes[jumlahNodes - 1]);
  } else if (jumlahNodes >= MAX_NEW_CONNECTIONS && jumlahNextNodes < MAX_NEXT_CONNECTION) {
    nextNodes[jumlahNextNodes++] = newId;
    Serial.printf("Node baru disimpan ke Next Node indeks ke-%d\n", jumlahNextNodes - 1);
    Serial.printf("Next Node: %d\n", nextNodes[jumlahNextNodes - 1]);
  } else {
    Serial.println("Tidak dapat menyimpan node baru karena kedua daftar sudah mencapai batas maksimal.");
  }
}

void receivedCallback(uint32_t from, String& msg) {
  uint32_t nodeId = mesh.getNodeId();
  Serial.printf("received from %u msg=%s\n", from, msg.c_str());
  sender = from;
  receivedData = msg.c_str();

  sendMessage();
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
  newId = nodeId;
  checkConnectionStatus();
  saveNode();
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
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

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

  if (millis() - t1 > 1000) {
    receiveSerial2();
    t1 = millis();
  }

  // if (millis() - tl > 2000) {
  //   sendMessage();
  //   tl = millis();
  // }
}