#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <LoRa_E220.h>
#include "model.h"

// ============================================================
// NODE CONFIGURATION
// ============================================================

// -------- CHANGE ONLY THESE TWO LINES --------

// JESS
#define MY_NAME "ASMI"
#define MY_ADDL 3

// THANU:
// #define MY_NAME "THANU"
// #define MY_ADDL 1

// ASMI:
// #define MY_NAME "ASMI"
// #define MY_ADDL 3

// KAVIN:
// #define MY_NAME "KAVIN"
// #define MY_ADDL 4

// ============================================================
// E220 PINS
// ============================================================

#define LORA_RX 16
#define LORA_TX 17

#define PIN_M0 25
#define PIN_M1 26
#define PIN_AUX 27

#define LORA_CHANNEL 18

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(
  &E220Serial,
  PIN_AUX,
  PIN_M0,
  PIN_M1
);

// ============================================================
// BLE UUIDs
// DO NOT CHANGE
// ============================================================

#define SERVICE_UUID \
"6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

#define RX_CHARACTERISTIC \
"6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

#define TX_CHARACTERISTIC \
"6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================================================
// OTHER NODE ADDRESSES
// ============================================================

#define THANU_ADDH 0
#define THANU_ADDL 1

#define JESS_ADDH 0
#define JESS_ADDL 2

#define ASMI_ADDH 0
#define ASMI_ADDL 3

#define KAVIN_ADDH 0
#define KAVIN_ADDL 4

// ============================================================
// BLE VARIABLES
// ============================================================

BLEServer *pServer = nullptr;
BLECharacteristic *txCharacteristic = nullptr;

bool deviceConnected = false;

// Phone ID connected to THIS ESP32
String myPhoneID = "";
// ============================================================
// NODE HEALTH MONITOR
// ============================================================

#define HEALTH_INTERVAL 10000
#define PING_TIMEOUT 3000

unsigned long lastHealthCheck = 0;
uint16_t healthSequence = 0;

struct NodeHealth {

  unsigned long sent = 0;

  unsigned long received = 0;

  unsigned long latency = 0;

  int rssi = 0;

  float packetLoss = 0;

  float distance = 0;

  float battery = 0;

};

NodeHealth health[5];

bool waitingForPong[5] = {false, false, false, false, false};

uint16_t pendingSequence[5] = {0, 0, 0, 0, 0};

unsigned long pingStartTime[5] = {0, 0, 0, 0, 0};

// ============================================================
// BLE SERVER CALLBACK
// ============================================================

class MyServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer *pServer) {

    deviceConnected = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("PHONE CONNECTED");
    Serial.println("================================");
  }

  void onDisconnect(BLEServer *pServer) {

    deviceConnected = false;

    Serial.println();
    Serial.println("================================");
    Serial.println("PHONE DISCONNECTED");
    Serial.println("================================");

    delay(200);

    BLEDevice::startAdvertising();

    Serial.println("BLE advertising restarted");
  }
};

// ============================================================
// CLEAN LORA DATA
// Removes unwanted characters such as �
// ============================================================

String cleanLoRaData(String data) {

  data.trim();

  while (data.length() > 0) {

    char lastChar = data[data.length() - 1];

    if (
      lastChar == '\0' ||
      lastChar == '\r' ||
      lastChar == '\n' ||
      !isPrintable(lastChar)
    ) {
      data.remove(data.length() - 1);
    }
    else {
      break;
    }
  }

  return data;
}

// ============================================================
// SEND TO PHONE
// ============================================================

void sendToPhone(String message) {

  if (!deviceConnected || txCharacteristic == nullptr) {

    Serial.println("PHONE NOT CONNECTED");
    return;
  }

  txCharacteristic->setValue(message.c_str());
  txCharacteristic->notify();

  Serial.println();
  Serial.println("--------------------------------");
  Serial.println("SENT TO PHONE");
  Serial.println(message);
  Serial.println("--------------------------------");
}

// ============================================================
// SEND FIXED LORA MESSAGE
// ============================================================

void sendLoRa(
  byte addh,
  byte addl,
  String message
) {

  ResponseStatus rs =
    e220ttl.sendFixedMessage(
      addh,
      addl,
      LORA_CHANNEL,
      message
    );

  Serial.print("LoRa Send Status: ");
  Serial.println(rs.getResponseDescription());
}

// ============================================================
// GET NODE ADDRESS
// ============================================================

byte getNodeAddress(String nodeName) {

  if (nodeName == "THANU") return THANU_ADDL;
  if (nodeName == "JESS")  return JESS_ADDL;
  if (nodeName == "ASMI")  return ASMI_ADDL;
  if (nodeName == "KAVIN") return KAVIN_ADDL;

  return 0;
}


// ============================================================
// SEND HEALTH PING
// ============================================================

void sendHealthPing(byte targetAddl, String targetName) {

  healthSequence++;

  String packet =
    "HEALTHPING," +
    String(MY_NAME) + "," +
    String(healthSequence) + "," +
    String(millis());

  Serial.println();
  Serial.println("----------- HEALTH PING -----------");

  Serial.print("From       : ");
  Serial.println(MY_NAME);

  Serial.print("To         : ");
  Serial.println(targetName);

  Serial.print("Sequence   : ");
  Serial.println(healthSequence);

  sendLoRa(0, targetAddl, packet);

  health[targetAddl].sent++;

  waitingForPong[targetAddl] = true;

  pendingSequence[targetAddl] = healthSequence;

  pingStartTime[targetAddl] = millis();
}


// ============================================================
// SEND PONG
// ============================================================

void sendHealthPong(String sourceName, uint16_t sequence) {

  byte sourceAddl = getNodeAddress(sourceName);

  if (sourceAddl == 0) {
    return;
  }

  String packet =
    "HEALTHPONG," +
    String(MY_NAME) + "," +
    String(sequence);

  Serial.print("Sending PONG to: ");
  Serial.println(sourceName);

  sendLoRa(0, sourceAddl, packet);
}


// ============================================================
// PRINT HEALTH INFORMATION
// ============================================================

void printHealth() {

  Serial.println();
  Serial.println("========== NODE HEALTH ==========");

  for (int i = 1; i <= 4; i++) {

    if (i == MY_ADDL)
      continue;

    if (health[i].sent == 0)
      continue;

    // Calculate real packet loss
    health[i].packetLoss =
      ((float)(health[i].sent - health[i].received)
       / health[i].sent) * 100.0;

    Serial.println();

    Serial.print("Node       : ");

    if (i == 1)
      Serial.println("THANU");
    else if (i == 2)
      Serial.println("JESS");
    else if (i == 3)
      Serial.println("ASMI");
    else if (i == 4)
      Serial.println("KAVIN");

    Serial.print("Sent       : ");
    Serial.println(health[i].sent);

    Serial.print("Received   : ");
    Serial.println(health[i].received);

    Serial.print("Packet Loss: ");
    Serial.print(health[i].packetLoss);
    Serial.println(" %");

    Serial.print("Latency    : ");
    Serial.print(health[i].latency);
    Serial.println(" ms");

    Serial.print("RSSI       : ");
    Serial.print(health[i].rssi);
    Serial.println(" dBm");
  }

  Serial.println("=================================");
}



  
  


// ============================================================
// HEALTH CHECK
// ============================================================

void performHealthCheck() {

  if (millis() - lastHealthCheck < HEALTH_INTERVAL) {
    return;
  }

  lastHealthCheck = millis();

  if (MY_ADDL != THANU_ADDL) {
    sendHealthPing(THANU_ADDL, "THANU");
    delay(100);
  }

  if (MY_ADDL != JESS_ADDL) {
    sendHealthPing(JESS_ADDL, "JESS");
    delay(100);
  }

  if (MY_ADDL != ASMI_ADDL) {
    sendHealthPing(ASMI_ADDL, "ASMI");
    delay(100);
  }

  // KAVIN is VEGA, so don't test KAVIN today.
  // We will enable this when VEGA arrives.

  /*
  if (MY_ADDL != KAVIN_ADDL) {
    sendHealthPing(KAVIN_ADDL, "KAVIN");
  }
  */
}
// ============================================================
// SEND TO ALL OTHER NODES
// ============================================================

void sendToAllNodes(String packet) {

  Serial.println();
  Serial.println("================================");
  Serial.println("FORWARDING TO OTHER NODES");
  Serial.println("================================");

  // THANU
  if (MY_ADDL != THANU_ADDL) {

    Serial.println("Sending -> THANU");

    sendLoRa(
      THANU_ADDH,
      THANU_ADDL,
      packet
    );

    delay(100);
  }

  // JESS
  if (MY_ADDL != JESS_ADDL) {

    Serial.println("Sending -> JESS");

    sendLoRa(
      JESS_ADDH,
      JESS_ADDL,
      packet
    );

    delay(100);
  }

  // ASMI
  if (MY_ADDL != ASMI_ADDL) {

    Serial.println("Sending -> ASMI");

    sendLoRa(
      ASMI_ADDH,
      ASMI_ADDL,
      packet
    );

    delay(100);
  }

  // KAVIN
  if (MY_ADDL != KAVIN_ADDL) {

    Serial.println("Sending -> KAVIN");

    sendLoRa(
      KAVIN_ADDH,
      KAVIN_ADDL,
      packet
    );

    delay(100);
  }
}

// ============================================================
// CHECK WHETHER THIS PACKET IS A ROUTING PACKET
// ============================================================

bool isRoutingPacket(String packet) {

  return packet.startsWith("MSG,COMMON,") ||
         packet.startsWith("MSG,PRIVATE,");
}

// ============================================================
// GET PRIVATE DESTINATION PHONE ID
//
// Packet:
//
// MSG,PRIVATE,FROM_PHONE,TO_PHONE,BASE64
//
// We only need TO_PHONE here.
// ============================================================

String getPrivateDestination(String packet) {

  int p1 = packet.indexOf(',');

  if (p1 < 0) return "";

  int p2 = packet.indexOf(',', p1 + 1);

  if (p2 < 0) return "";

  int p3 = packet.indexOf(',', p2 + 1);

  if (p3 < 0) return "";

  int p4 = packet.indexOf(',', p3 + 1);

  if (p4 < 0) return "";

  String destination =
    packet.substring(p3 + 1, p4);

  destination.trim();

  return destination;
}

// ============================================================
// HANDLE BLE MESSAGE FROM PHONE
// ============================================================

class MyCallbacks : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *characteristic) {

    String value = characteristic->getValue();

    if (value.length() == 0) {
      return;
    }

    value.trim();

    Serial.println();
    Serial.println("================================");
    Serial.println("MESSAGE FROM PHONE");
    Serial.println("================================");

    Serial.print("BLE Data: ");
    Serial.println(value);

    // ========================================================
    // LOCATION PACKET
    //
    // LOC,PHONE_ID,LAT,LON,TIME,BATTERY
    // ========================================================

    if (value.startsWith("LOC,")) {

      Serial.println();
      Serial.println("LOCATION PACKET");

      // Extract phone ID
      int firstComma = value.indexOf(',');

      int secondComma =
        value.indexOf(',', firstComma + 1);

      if (secondComma > 0) {

        myPhoneID =
          value.substring(
            firstComma + 1,
            secondComma
          );

        myPhoneID.trim();

        Serial.print("My Phone ID = ");
        Serial.println(myPhoneID);
      }

      // Show complete location
      Serial.print("Location = ");
      Serial.println(value);

      // Send location to every other ESP32
      sendToAllNodes(value);

      return;
    }

    // ========================================================
    // COMMON MESSAGE
    //
    // MSG,COMMON,PHONE_ID,ALL,BASE64
    // ========================================================

    if (value.startsWith("MSG,COMMON,")) {

      Serial.println();
      Serial.println("COMMON MESSAGE");

      Serial.println("Forwarding common message...");

      sendToAllNodes(value);

      // Also show it on local phone
      // This is useful if another phone sends through
      // this node and you want local visibility.
      sendToPhone(value);

      return;
    }

    // ========================================================
    // PRIVATE MESSAGE
    //
    // MSG,PRIVATE,FROM_PHONE,TO_PHONE,BASE64
    // ========================================================

    if (value.startsWith("MSG,PRIVATE,")) {

      Serial.println();
      Serial.println("PRIVATE MESSAGE");

      String destination =
        getPrivateDestination(value);

      Serial.print("Destination Phone ID: ");
      Serial.println(destination);

      Serial.print("My Phone ID: ");
      Serial.println(myPhoneID);

      // ------------------------------------------------------
      // IMPORTANT:
      //
      // The app uses PHONE_xxxxxxxx as destination.
      //
      // Therefore we cannot decide destination using
      // JESS/ASMI/THANU/KAVIN.
      //
      // Every ESP32 receives the packet.
      // Only the ESP32 connected to the target phone
      // delivers it to that phone.
      // ------------------------------------------------------

      if (
        destination.length() > 0 &&
        myPhoneID.length() > 0 &&
        destination == myPhoneID
      ) {

        Serial.println();
        Serial.println("***** PRIVATE MESSAGE FOR THIS PHONE *****");

        sendToPhone(value);

      }
      else {

        Serial.println(
          "Not for this phone"
        );

        Serial.println(
          "Forwarding to other nodes..."
        );

        sendToAllNodes(value);
      }

      return;
    }

    // ========================================================
    // UNKNOWN PACKET
    // ========================================================

    Serial.println();
    Serial.println("UNKNOWN BLE PACKET");
    Serial.println(value);
  }
};

// ============================================================
// HANDLE LORA RECEIVED DATA
// ============================================================

void handleLoRa() {

  if (!e220ttl.available()) {
    return;
  }

  ResponseContainer rc = e220ttl.receiveMessageRSSI();
  Serial.print("Received LoRa message: ");
Serial.println(rc.data);

Serial.print("Raw RSSI: ");
Serial.println(rc.rssi);

  if (rc.status.code != 1) {

    Serial.print("LoRa Receive Error: ");
    Serial.println(
      rc.status.getResponseDescription()
    );

    return;
  }

  String packet =
    cleanLoRaData(rc.data);

  if (packet.length() == 0) {
    return;
  }
    // ========================================================
  // HEALTH PING
  // ========================================================

  if (packet.startsWith("HEALTHPING,")) {

    Serial.println();
    Serial.println("******** HEALTH PING RECEIVED ********");

    int p1 = packet.indexOf(',');
    int p2 = packet.indexOf(',', p1 + 1);
    int p3 = packet.indexOf(',', p2 + 1);

    if (p1 > 0 && p2 > 0 && p3 > 0) {

      String sourceName =
        packet.substring(p1 + 1, p2);

      uint16_t sequence =
        packet.substring(p2 + 1, p3).toInt();

      Serial.print("Source   : ");
      Serial.println(sourceName);

      Serial.print("Sequence : ");
      Serial.println(sequence);

      sendHealthPong(sourceName, sequence);
    }

    return;
  }


  // ========================================================
  // HEALTH PONG
  // ========================================================

  if (packet.startsWith("HEALTHPONG,")) {

    Serial.println();
    Serial.println("******** HEALTH PONG RECEIVED ********");

    int p1 = packet.indexOf(',');
    int p2 = packet.indexOf(',', p1 + 1);

    if (p1 > 0 && p2 > 0) {

      String sourceName =
        packet.substring(p1 + 1, p2);

      uint16_t sequence =
        packet.substring(p2 + 1).toInt();

      byte sourceAddl =
        getNodeAddress(sourceName);

      Serial.print("Source   : ");
      Serial.println(sourceName);

      Serial.print("Sequence : ");
      Serial.println(sequence);

      if (sourceAddl > 0) {

        if (waitingForPong[sourceAddl] &&
            pendingSequence[sourceAddl] == sequence) {

          unsigned long currentTime = millis();

          health[sourceAddl].latency =
            currentTime -
            pingStartTime[sourceAddl];

          health[sourceAddl].received++;

          health[sourceAddl].rssi =
            -(256 - rc.rssi);

          waitingForPong[sourceAddl] = false;

          Serial.print("RSSI     : ");
          Serial.print(health[sourceAddl].rssi);
          Serial.println(" dBm");

          Serial.print("Latency  : ");
          Serial.print(health[sourceAddl].latency);
          Serial.println(" ms");

          printHealth();
        }
      }
    }

    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("DATA RECEIVED FROM LORA");
  Serial.println("================================");

  Serial.print("LoRa Data: ");
  Serial.println(packet);

  // ========================================================
  // LOCATION
  // ========================================================

  if (packet.startsWith("LOC,")) {

    Serial.println();
    Serial.println("LOCATION RECEIVED");

    Serial.println(
      "Sending location to phone..."
    );

    sendToPhone(packet);

    return;
  }

  // ========================================================
  // COMMON MESSAGE
  // ========================================================

  if (packet.startsWith("MSG,COMMON,")) {

    Serial.println();
    Serial.println("COMMON MESSAGE RECEIVED");

    Serial.println(
      "Sending common message to phone..."
    );

    sendToPhone(packet);

    return;
  }

  // ========================================================
  // PRIVATE MESSAGE
  // ========================================================

  if (packet.startsWith("MSG,PRIVATE,")) {

    Serial.println();
    Serial.println("PRIVATE MESSAGE RECEIVED");

    String destination =
      getPrivateDestination(packet);

    Serial.print("Destination = ");
    Serial.println(destination);

    Serial.print("My Phone ID = ");
    Serial.println(myPhoneID);

    // ------------------------------------------------------
    // This node is the destination
    // ------------------------------------------------------

    if (
      destination.length() > 0 &&
      myPhoneID.length() > 0 &&
      destination == myPhoneID
    ) {

      Serial.println();
      Serial.println(
        "***** THIS IS MY PRIVATE MESSAGE *****"
      );

      sendToPhone(packet);

    }

    // ------------------------------------------------------
    // Not this node
    // ------------------------------------------------------

    else {

      Serial.println(
        "Private message is NOT for this phone"
      );

      Serial.println(
        "Ignoring packet"
      );
    }

    return;
  }

  // ========================================================
  // UNKNOWN LORA PACKET
  // ========================================================

  Serial.println();
  Serial.println("UNKNOWN LORA PACKET");
}

// ============================================================
// BLE SETUP
// ============================================================

void setupBLE() {

  BLEDevice::init(MY_NAME);

  pServer =
    BLEDevice::createServer();

  pServer->setCallbacks(
    new MyServerCallbacks()
  );

  BLEService *service =
    pServer->createService(
      SERVICE_UUID
    );

  // Phone -> ESP32
  BLECharacteristic *rxCharacteristic =
    service->createCharacteristic(
      RX_CHARACTERISTIC,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR
    );

  // ESP32 -> Phone
  txCharacteristic =
    service->createCharacteristic(
      TX_CHARACTERISTIC,
      BLECharacteristic::PROPERTY_NOTIFY
    );

  txCharacteristic->addDescriptor(
    new BLE2902()
  );

  rxCharacteristic->setCallbacks(
    new MyCallbacks()
  );

  service->start();

  BLEAdvertising *advertising =
    BLEDevice::getAdvertising();

  advertising->addServiceUUID(
    SERVICE_UUID
  );

  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println();
  Serial.println("================================");
  Serial.println("BLE READY");
  Serial.print("BLE NAME: ");
  Serial.println(MY_NAME);
  Serial.println("================================");
}

// ============================================================
// SETUP
// ============================================================
void testMLModel()
{
    float features[6] = {
        -60.0,   // RSSI
        5.0,     // SNR
        1.0,     // Distance (km)
        80.0,    // Battery (%)
        2.0,     // Packet Loss (%)
        100.0    // Latency (ms)
    };

    int result = predictNodeHealth(features);

    Serial.print("ML Node Health: ");

    if (result == 1)
        Serial.println("NORMAL");
    else
        Serial.println("ABNORMAL");
}
void setup() {

  Serial.begin(115200);
testMLModel();
  delay(1000);

  Serial.println();
  Serial.println("################################");
  Serial.println("LORA TRACK CHAT NODE");
  Serial.println("################################");

  Serial.print("NODE NAME: ");
  Serial.println(MY_NAME);

  Serial.print("NODE ADDRESS: 0,");
  Serial.println(MY_ADDL);

  // ========================================================
  // E220 UART
  // ========================================================

  E220Serial.begin(
    9600,
    SERIAL_8N1,
    LORA_RX,
    LORA_TX
  );

  delay(500);

  Serial.println();
  Serial.println("Starting E220...");

  bool result =
    e220ttl.begin();

if (result) {

    Serial.println(
      "E220 STARTED SUCCESSFULLY"
    );

    // ========================================================
    // ENABLE RSSI
    // ========================================================

    ResponseStructContainer c =
      e220ttl.getConfiguration();

    if (c.status.code == 1) {

      Configuration configuration =
        *(Configuration*) c.data;

      Serial.print("Current RSSI setting: ");
      Serial.println(
        configuration.TRANSMISSION_MODE.enableRSSI
      );

      // Enable RSSI
      configuration.TRANSMISSION_MODE.enableRSSI =
        RSSI_ENABLED;

      ResponseStatus rs =
        e220ttl.setConfiguration(
          configuration,
          WRITE_CFG_PWR_DWN_SAVE
        );

      Serial.print("RSSI configuration: ");
      Serial.println(
        rs.getResponseDescription()
      );

      c.close();

    }
    else {

      Serial.println(
        "Could not read E220 configuration"
      );
    }
}
else {

    Serial.println(
      "E220 START FAILED"
    );
}

  // ========================================================
  // BLE
  // ========================================================

  setupBLE();

  Serial.println();
  Serial.println("SYSTEM READY");
  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  handleLoRa();

  performHealthCheck();

  delay(5);
}