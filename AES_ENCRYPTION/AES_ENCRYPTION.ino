#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <LoRa_E220.h>
#include "model.h"
#include "mbedtls/gcm.h"

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
// AES-256-GCM ENCRYPTION
// User messages are encrypted before being sent through LoRa.
// Health PING/PONG packets remain unencrypted because they are
// used internally for node-health measurement.
// ============================================================

const uint8_t AES_KEY[32] = {
  0x10, 0x22, 0x34, 0x48,
  0x55, 0x61, 0x73, 0x89,
  0x91, 0xA2, 0xB3, 0xC4,
  0xD5, 0xE6, 0xF7, 0x08,
  0x19, 0x2A, 0x3B, 0x4C,
  0x5D, 0x6E, 0x7F, 0x80,
  0x90, 0xA1, 0xB2, 0xC3
};

#define GCM_NONCE_SIZE 12
#define GCM_TAG_SIZE   16

char hexDigit(uint8_t value) {
  if (value < 10) return '0' + value;
  return 'A' + (value - 10);
}

String bytesToHex(const uint8_t *data, size_t length) {
  String result = "";
  result.reserve(length * 2);

  for (size_t i = 0; i < length; i++) {
    result += hexDigit((data[i] >> 4) & 0x0F);
    result += hexDigit(data[i] & 0x0F);
  }

  return result;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool hexToBytes(String hex, uint8_t *output, size_t outputSize) {
  hex.trim();

  if (hex.length() != outputSize * 2) return false;

  for (size_t i = 0; i < outputSize; i++) {
    int high = hexValue(hex[i * 2]);
    int low  = hexValue(hex[i * 2 + 1]);

    if (high < 0 || low < 0) return false;

    output[i] = (high << 4) | low;
  }

  return true;
}

// Packet format:
// E2E|NONCE_HEX|CIPHERTEXT_HEX|TAG_HEX
String encryptMessage(String plaintext) {
  plaintext.trim();

  if (plaintext.length() == 0) return "";

  size_t plaintextLength = plaintext.length();

  uint8_t nonce[GCM_NONCE_SIZE];
  uint8_t tag[GCM_TAG_SIZE];

  uint8_t *ciphertext = new uint8_t[plaintextLength];

  if (ciphertext == nullptr) return "";

  // ESP32 hardware RNG -> fresh nonce for every message.
  for (int i = 0; i < GCM_NONCE_SIZE; i++) {
    nonce[i] = (uint8_t)(esp_random() & 0xFF);
  }

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int result = mbedtls_gcm_setkey(
    &gcm,
    MBEDTLS_CIPHER_ID_AES,
    AES_KEY,
    256
  );

  if (result != 0) {
    mbedtls_gcm_free(&gcm);
    delete[] ciphertext;
    return "";
  }

  result = mbedtls_gcm_crypt_and_tag(
    &gcm,
    MBEDTLS_GCM_ENCRYPT,
    plaintextLength,
    nonce,
    GCM_NONCE_SIZE,
    nullptr,
    0,
    (const uint8_t *)plaintext.c_str(),
    ciphertext,
    GCM_TAG_SIZE,
    tag
  );

  mbedtls_gcm_free(&gcm);

  if (result != 0) {
    delete[] ciphertext;
    return "";
  }

  String packet =
    "E2E|" +
    bytesToHex(nonce, GCM_NONCE_SIZE) +
    "|" +
    bytesToHex(ciphertext, plaintextLength) +
    "|" +
    bytesToHex(tag, GCM_TAG_SIZE);

  delete[] ciphertext;
  return packet;
}

bool decryptMessage(String packet, String &plaintext) {
  packet.trim();

  int p1 = packet.indexOf('|');
  int p2 = packet.indexOf('|', p1 + 1);
  int p3 = packet.indexOf('|', p2 + 1);

  if (p1 <= 0 || p2 <= p1 || p3 <= p2) return false;

  String type      = packet.substring(0, p1);
  String nonceHex  = packet.substring(p1 + 1, p2);
  String cipherHex = packet.substring(p2 + 1, p3);
  String tagHex    = packet.substring(p3 + 1);

  if (type != "E2E") return false;

  if (nonceHex.length() != GCM_NONCE_SIZE * 2) return false;
  if (tagHex.length() != GCM_TAG_SIZE * 2) return false;

  if (cipherHex.length() == 0 || (cipherHex.length() % 2) != 0) {
    return false;
  }

  size_t cipherLength = cipherHex.length() / 2;

  uint8_t nonce[GCM_NONCE_SIZE];
  uint8_t tag[GCM_TAG_SIZE];

  uint8_t *ciphertext = new uint8_t[cipherLength];
  uint8_t *decrypted  = new uint8_t[cipherLength + 1];

  if (ciphertext == nullptr || decrypted == nullptr) {
    delete[] ciphertext;
    delete[] decrypted;
    return false;
  }

  if (!hexToBytes(nonceHex, nonce, GCM_NONCE_SIZE) ||
      !hexToBytes(tagHex, tag, GCM_TAG_SIZE) ||
      !hexToBytes(cipherHex, ciphertext, cipherLength)) {
    delete[] ciphertext;
    delete[] decrypted;
    return false;
  }

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);

  int result = mbedtls_gcm_setkey(
    &gcm,
    MBEDTLS_CIPHER_ID_AES,
    AES_KEY,
    256
  );

  if (result != 0) {
    mbedtls_gcm_free(&gcm);
    delete[] ciphertext;
    delete[] decrypted;
    return false;
  }

  result = mbedtls_gcm_auth_decrypt(
    &gcm,
    cipherLength,
    nonce,
    GCM_NONCE_SIZE,
    nullptr,
    0,
    tag,
    GCM_TAG_SIZE,
    ciphertext,
    decrypted
  );

  mbedtls_gcm_free(&gcm);

  if (result != 0) {
    delete[] ciphertext;
    delete[] decrypted;
    return false;
  }

  decrypted[cipherLength] = '\0';
  plaintext = String((char *)decrypted);

  delete[] ciphertext;
  delete[] decrypted;

  return true;
}

// ============================================================
// NODE SELECTING + ENCRYPTED MESSAGE SENDING
// Manual/direct-node test format (optional):
//     ASMI|Hello
//     JESS|Hello
//     THANU|Hello
//     KAVIN|Hello
//
// The selected node address is used by E220 fixed addressing.
// The actual message is AES-256-GCM encrypted before transmission.
// ============================================================

bool getDestinationAddress(
  String name,
  uint8_t &addH,
  uint8_t &addL
) {
  name.trim();
  name.toUpperCase();

  if (name == "THANU") {
    addH = THANU_ADDH;
    addL = THANU_ADDL;
    return true;
  }

  if (name == "JESS") {
    addH = JESS_ADDH;
    addL = JESS_ADDL;
    return true;
  }

  if (name == "ASMI") {
    addH = ASMI_ADDH;
    addL = ASMI_ADDL;
    return true;
  }

  if (name == "KAVIN") {
    addH = KAVIN_ADDH;
    addL = KAVIN_ADDL;
    return true;
  }

  return false;
}

void sendEncryptedMessage(String destination, String message) {
  destination.trim();
  message.trim();

  uint8_t addH;
  uint8_t addL;

  if (!getDestinationAddress(destination, addH, addL)) {
    Serial.println("UNKNOWN DESTINATION");
    return;
  }

  if (destination.equalsIgnoreCase(MY_NAME)) {
    Serial.println("CANNOT SEND TO YOURSELF");
    return;
  }

  Serial.println();
  Serial.println("========================================");
  Serial.println("AES-256-GCM ENCRYPTION");
  Serial.println("========================================");
  Serial.print("Original message: ");
  Serial.println(message);

  String encrypted = encryptMessage(message);

  if (encrypted.length() == 0) {
    Serial.println("ENCRYPTION FAILED");
    return;
  }

  Serial.println();
  Serial.println("Encrypted packet:");
  Serial.println(encrypted);

  Serial.println();
  Serial.print("Sending encrypted data -> ");
  Serial.println(destination);

  ResponseStatus rs = e220ttl.sendFixedMessage(
    addH,
    addL,
    LORA_CHANNEL,
    encrypted
  );

  if (rs.code == E220_SUCCESS) {
    Serial.println("ENCRYPTED MESSAGE SENT SUCCESSFULLY");
  } else {
    Serial.print("LORA ERROR: ");
    Serial.println(rs.getResponseDescription());
  }

  Serial.println("========================================");
}


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

    // --------------------------------------------------------
    // TinyML prediction
    // --------------------------------------------------------
    // The current health structure contains RSSI, latency and
    // packet loss. SNR, distance and battery are not measured
    // by the current TinyML sketch, so they remain explicit
    // placeholders rather than being silently fabricated.
    float features[6] = {
      (float)health[i].rssi,
      0.0f,                         // SNR: not currently measured
      health[i].distance,           // Distance (km)
      health[i].battery,            // Battery (%)
      health[i].packetLoss,         // Packet Loss (%)
      (float)health[i].latency      // Latency (ms)
    };

    int mlResult = predictNodeHealth(features);

    Serial.println();
    Serial.print("ML Prediction: ");
    Serial.println(
      mlResult == 1 ? "NORMAL" : "ABNORMAL"
    );

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

value.trim();

if (value.length() == 0) {
    return;
}

    if (value.length() == 0) return;

    value.trim();

    Serial.println();
    Serial.println("================================");
    Serial.println("MESSAGE FROM PHONE");
    Serial.println("================================");
    Serial.print("BLE Data: ");
    Serial.println(value);

    // ========================================================
    // DIRECT NODE-SELECT + ENCRYPTED MESSAGE
    //
    // Examples:
    // ASMI|Hello
    // JESS|Hello
    // THANU|Hello
    // KAVIN|Hello
    //
    // This is the encrypted-data path.
    // ========================================================

    int nodeSeparator = value.indexOf('|');

    if (nodeSeparator > 0) {

      String destination = value.substring(0, nodeSeparator);
      String text = value.substring(nodeSeparator + 1);

      destination.trim();
      text.trim();

      uint8_t testH;
      uint8_t testL;

      if (getDestinationAddress(destination, testH, testL)) {

        if (text.length() == 0) {
          Serial.println("INVALID MESSAGE");
          return;
        }

        sendEncryptedMessage(destination, text);
        return;
      }
    }

    // ========================================================
    // LOCATION PACKET
    //
    // LOC,PHONE_ID,LAT,LON,TIME,BATTERY
    // ========================================================

    if (value.startsWith("LOC,")) {

      Serial.println();
      Serial.println("LOCATION PACKET");

      int firstComma = value.indexOf(',');
      int secondComma = value.indexOf(',', firstComma + 1);

      if (secondComma > 0) {
        myPhoneID = value.substring(firstComma + 1, secondComma);
        myPhoneID.trim();

        Serial.print("My Phone ID = ");
        Serial.println(myPhoneID);
      }

      Serial.print("Location = ");
      Serial.println(value);

      sendToAllNodes(value);
      return;
    }

    // ========================================================
    // COMMON MESSAGE
    // ========================================================

    if (value.startsWith("MSG,COMMON,")) {

      Serial.println();
      Serial.println("COMMON MESSAGE");
      Serial.println("Forwarding common message...");

      sendToAllNodes(value);
      sendToPhone(value);
      return;
    }

    // ========================================================
    // PRIVATE MESSAGE - AES-256-GCM PATH
    //
    // The mobile app protocol is NOT changed:
    // MSG,PRIVATE,FROM_PHONE,TO_PHONE,BASE64_MESSAGE
    //
    // The complete application packet is encrypted before LoRa.
    // Other nodes receive only E2E|NONCE|CIPHERTEXT|TAG.
    // The destination node decrypts and then delivers the
    // original MSG,PRIVATE packet to the correct phone.
    // ========================================================

    if (value.startsWith("MSG,PRIVATE,")) {

      Serial.println();
      Serial.println("========================================");
      Serial.println("PRIVATE MESSAGE FROM PHONE");
      Serial.println("================================");

      String destination = getPrivateDestination(value);

      Serial.print("Sender/Original packet: ");
      Serial.println(value);

      Serial.print("Destination Phone ID: ");
      Serial.println(destination);

      if (destination.length() == 0) {
        Serial.println("INVALID PRIVATE MESSAGE - NO DESTINATION");
        return;
      }

      Serial.println();
      Serial.println("---------- AES-256-GCM ----------");

      String encrypted = encryptMessage(value);

      if (encrypted.length() == 0) {
        Serial.println("Encryption            : FAILED");
        Serial.println("Private message NOT sent over LoRa");
        Serial.println("---------------------------------");
        return;
      }

      Serial.println("Encryption            : SUCCESS");
      Serial.println("Secure packet         :");
      Serial.println(encrypted);
      Serial.println("---------------------------------");

      Serial.println("Broadcasting encrypted private message...");
      sendToAllNodes(encrypted);

      Serial.println("PRIVATE MESSAGE ENCRYPTED AND SENT");
      Serial.println("========================================");

      return;
    }

    Serial.println();
    Serial.println("UNKNOWN BLE PACKET");
    Serial.println(value);
  }
};

// ============================================================
// HANDLE LORA RECEIVED DATA
// ============================================================

void handleLoRa() {

  if (!e220ttl.available()) return;

  ResponseContainer rc = e220ttl.receiveMessageRSSI();

  if (rc.status.code != E220_SUCCESS) {
    Serial.print("LoRa Receive Error: ");
    Serial.println(rc.status.getResponseDescription());
    return;
  }

  String packet = cleanLoRaData(rc.data);

  if (packet.length() == 0) return;

  Serial.println();
  Serial.println("================================");
  Serial.println("LORA DATA RECEIVED");
  Serial.println("================================");
  Serial.print("Raw packet: ");
  Serial.println(packet);
  Serial.print("Raw RSSI: ");
  Serial.println(rc.rssi);

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

      String sourceName = packet.substring(p1 + 1, p2);
      uint16_t sequence = packet.substring(p2 + 1, p3).toInt();

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

      String sourceName = packet.substring(p1 + 1, p2);
      uint16_t sequence = packet.substring(p2 + 1).toInt();

      byte sourceAddl = getNodeAddress(sourceName);

      if (sourceAddl > 0 &&
          waitingForPong[sourceAddl] &&
          pendingSequence[sourceAddl] == sequence) {

        unsigned long currentTime = millis();

        health[sourceAddl].latency =
          currentTime - pingStartTime[sourceAddl];

        health[sourceAddl].received++;

        // E220 receiveMessageRSSI() returns the library's RSSI value.
        // Keep the same conversion used in the working TinyML code.
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

    return;
  }

  // ========================================================
  // AES-256-GCM ENCRYPTED USER DATA
  //
  // E2E|NONCE|CIPHERTEXT|TAG
  // ========================================================

  if (packet.startsWith("E2E|")) {

    Serial.println();
    Serial.println("========================================");
    Serial.println("SECURE MESSAGE RECEIVED");
    Serial.println("========================================");
    Serial.println("AES-256-GCM DECRYPTING...");

    String plaintext;

    bool success = decryptMessage(packet, plaintext);

    if (!success) {

      Serial.println();
      Serial.println("Decryption             : FAILED");
      Serial.println("Authentication         : INVALID");
      Serial.println("Possible reasons:");
      Serial.println("1. Wrong AES key");
      Serial.println("2. Corrupted packet");
      Serial.println("3. Invalid packet format");
      Serial.println("Message NOT sent to phone");
      Serial.println("========================================");

      return;
    }

    Serial.println();
    Serial.println("Decryption             : SUCCESS");
    Serial.println("Authentication         : VALID");
    Serial.print("Decrypted packet       : ");
    Serial.println(plaintext);

    // The encrypted payload contains the original app packet.
    // Only the destination phone receives a private message.
    if (plaintext.startsWith("MSG,PRIVATE,")) {

      String destination = getPrivateDestination(plaintext);

      Serial.print("Destination Phone ID  : ");
      Serial.println(destination);
      Serial.print("My Phone ID           : ");
      Serial.println(myPhoneID);

      if (
        destination.length() > 0 &&
        myPhoneID.length() > 0 &&
        destination == myPhoneID
      ) {

        Serial.println();
        Serial.println("***** PRIVATE MESSAGE FOR THIS PHONE *****");
        Serial.println("Sending decrypted packet to phone...");
        sendToPhone(plaintext);

      } else {

        Serial.println("Private message is NOT for this phone");
        Serial.println("Encrypted payload discarded after routing check");
      }

    } else if (plaintext.startsWith("MSG,COMMON,")) {

      Serial.println("COMMON MESSAGE DECRYPTED");
      sendToPhone(plaintext);

    } else {

      Serial.println("Unknown encrypted application packet");
    }

    Serial.println("LORA -> AES-256-GCM DECRYPT -> ROUTE -> BLE");
    Serial.println("========================================");

    return;
  }

  // ========================================================
  // LEGACY / EXISTING APP DATA
  // ========================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("DATA RECEIVED FROM LORA");
  Serial.println("================================");

  // LOCATION
  if (packet.startsWith("LOC,")) {

    Serial.println("LOCATION RECEIVED");
    sendToPhone(packet);
    return;
  }

  // COMMON MESSAGE
  if (packet.startsWith("MSG,COMMON,")) {

    Serial.println("COMMON MESSAGE RECEIVED");
    sendToPhone(packet);
    return;
  }

  // PRIVATE MESSAGE
  if (packet.startsWith("MSG,PRIVATE,")) {

    Serial.println("PRIVATE MESSAGE RECEIVED");

    String destination = getPrivateDestination(packet);

    Serial.print("Destination = ");
    Serial.println(destination);

    Serial.print("My Phone ID = ");
    Serial.println(myPhoneID);

    if (
      destination.length() > 0 &&
      myPhoneID.length() > 0 &&
      destination == myPhoneID
    ) {

      Serial.println("***** THIS IS MY PRIVATE MESSAGE *****");
      sendToPhone(packet);

    } else {

      Serial.println("Private message is NOT for this phone");
      Serial.println("Ignoring packet");
    }

    return;
  }

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
    -60.0,  // RSSI
    5.0,    // SNR
    1.0,    // Distance (km)
    80.0,   // Battery (%)
    2.0,    // Packet Loss (%)
    100.0   // Latency (ms)
  };

  int result = predictNodeHealth(features);

  Serial.println();
  Serial.println("========== TINYML MODEL TEST ==========");
  Serial.print("RSSI         : "); Serial.println(features[0]);
  Serial.print("SNR          : "); Serial.println(features[1]);
  Serial.print("Distance     : "); Serial.println(features[2]);
  Serial.print("Battery      : "); Serial.println(features[3]);
  Serial.print("Packet Loss  : "); Serial.println(features[4]);
  Serial.print("Latency      : "); Serial.println(features[5]);

  Serial.print("ML Node Health: ");

  if (result == 1)
    Serial.println("NORMAL");
  else
    Serial.println("ABNORMAL");

  Serial.println("=======================================");
}
void setup() {

  Serial.begin(115200);
testMLModel();
  Serial.println("Encryption: AES-256-GCM");
  Serial.println("Secure packet: E2E|NONCE|CIPHERTEXT|TAG");
  delay(1000);

  Serial.println();
  Serial.println("################################");
  Serial.println("LORA SECURE CHAT + TINYML NODE");
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
  Serial.println("SYSTEM READY - AES-256-GCM + TINYML");
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