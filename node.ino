#include <painlessMesh.h>
#include <Arduino_JSON.h>

#define MESH_PREFIX "XirkaMesh"
#define MESH_PASSWORD "12345678"
#define MESH_PORT 5555

#define LED_PIN 4

String receivedData;
Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();

Task taskSendMessage(5000, TASK_FOREVER, &sendMessage);

void sendMessage() {
  String msg = receivedData;
  uint32_t destId = 365108269;
  mesh.sendSingle(destId, msg);
  Serial.printf("Data Sent via Mesh to: %u msg=%s\n", destId, msg.c_str());
}

void receivedCallback(uint32_t from, String &msg) {
  uint32_t nodeId = mesh.getNodeId();
  Serial.printf("Node ID: %u received from %u msg=%s\n", nodeId, from, msg.c_str());

  DynamicJsonDocument doc(200);
  deserializeJson(doc, msg);
  uint8_t percentage = doc["percentage"];
  setPercentLamp(percentage);

  // String route = mesh.subConnectionJson();
  // Serial.println("connection :" + route);
  String ackMessage = "ACK message from: " + String(nodeId);
  mesh.sendSingle(from, ackMessage);
  Serial.println(ackMessage);
  receivedData = msg.c_str();
}

void setPercentLamp(uint8_t percentage) {
  uint8_t dim = map(percentage, 0, 255, 0, 100);
  analogWrite(LED_PIN, dim);
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
  Serial.begin(9600);
  //Serial2.begin(115200, SERIAL_8N1, 17, 16);

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);


  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  mesh.update();
  userScheduler.execute();
}
