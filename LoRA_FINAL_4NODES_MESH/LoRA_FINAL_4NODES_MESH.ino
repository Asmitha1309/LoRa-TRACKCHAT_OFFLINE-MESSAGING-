#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <LoRa_E220.h>
#include "mbedtls/gcm.h"

// ============================================================
// NODE NAME
// ============================================================

// JESS ESP32
#define MY_NAME "ASMI"

// For ASMI board, change to:
// #define MY_NAME "ASMI"

// For THANU board, change to:
// #define MY_NAME "THANU"

// ============================================================
// NODE ID
// ============================================================

#define MY_NODE_ID 3

// ============================================================
// E220 PINS
// ============================================================

#define LORA_RX 16
#define LORA_TX 17

#define PIN_M0  25
#define PIN_M1  26
#define PIN_AUX 27

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(
  &E220Serial,
  PIN_AUX,
  PIN_M0,
  PIN_M1
);

// ============================================================
// LORA CONFIGURATION
// ============================================================

#define LORA_CHANNEL 18

// ============================================================
// NODE ADDRESSES
// ============================================================

#define THANU_ADDH 0
#define THANU_ADDL 1

#define JESS_ADDH  0
#define JESS_ADDL  2

#define ASMI_ADDH  0
#define ASMI_ADDL  3

#define KAVIN_ADDH  0
#define KAVIN_ADDL  4

// ============================================================
// BLE UUIDs
// ============================================================

#define SERVICE_UUID \
"6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

#define RX_CHARACTERISTIC \
"6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

#define TX_CHARACTERISTIC \
"6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================================================
// AES-256 KEY
//
// IMPORTANT:
// SAME KEY MUST BE PRESENT ON BOTH TEST NODES.
//
// THIS IS ONLY A TEST KEY.
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

// ============================================================
// AES-GCM
// ============================================================

#define GCM_NONCE_SIZE 12
#define GCM_TAG_SIZE   16

// ============================================================
// BLE GLOBALS
// ============================================================

BLECharacteristic *txCharacteristic = nullptr;

bool deviceConnected = false;

// ============================================================
// HEX DIGIT
// ============================================================

char hexDigit(uint8_t value)
{
  if (value < 10)
  {
    return '0' + value;
  }

  return 'A' + (value - 10);
}

// ============================================================
// BYTES TO HEX
// ============================================================

String bytesToHex(
  const uint8_t *data,
  size_t length
)
{
  String result = "";

  for (size_t i = 0; i < length; i++)
  {
    result += hexDigit(
      (data[i] >> 4) & 0x0F
    );

    result += hexDigit(
      data[i] & 0x0F
    );
  }

  return result;
}

// ============================================================
// HEX VALUE
// ============================================================

int hexValue(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }

  if (c >= 'A' && c <= 'F')
  {
    return c - 'A' + 10;
  }

  if (c >= 'a' && c <= 'f')
  {
    return c - 'a' + 10;
  }

  return -1;
}

// ============================================================
// HEX TO BYTES
// ============================================================

bool hexToBytes(
  String hex,
  uint8_t *output,
  size_t outputSize
)
{
  hex.trim();

  if (hex.length() != outputSize * 2)
  {
    return false;
  }

  for (size_t i = 0; i < outputSize; i++)
  {
    int high =
      hexValue(hex[i * 2]);

    int low =
      hexValue(hex[i * 2 + 1]);

    if (high < 0 || low < 0)
    {
      return false;
    }

    output[i] =
      (high << 4) | low;
  }

  return true;
}

// ============================================================
// AES-256-GCM ENCRYPT
//
// PACKET FORMAT:
//
// E2E|NONCE|CIPHERTEXT|TAG
// ============================================================

String encryptMessage(
  String plaintext
)
{
  plaintext.trim();

  if (plaintext.length() == 0)
  {
    return "";
  }

  uint8_t nonce[GCM_NONCE_SIZE];

  uint8_t tag[GCM_TAG_SIZE];

  size_t plaintextLength =
    plaintext.length();

  uint8_t *ciphertext =
    new uint8_t[plaintextLength];

  // ==========================================================
  // GENERATE RANDOM NONCE
  // ==========================================================

  for (int i = 0; i < GCM_NONCE_SIZE; i++)
  {
    nonce[i] =
      (uint8_t)(esp_random() & 0xFF);
  }

  // ==========================================================
  // INITIALIZE AES-GCM
  // ==========================================================

  mbedtls_gcm_context gcm;

  mbedtls_gcm_init(&gcm);

  int result =
    mbedtls_gcm_setkey(
      &gcm,
      MBEDTLS_CIPHER_ID_AES,
      AES_KEY,
      256
    );

  if (result != 0)
  {
    Serial.println(
      "AES KEY SET FAILED"
    );

    mbedtls_gcm_free(&gcm);

    delete[] ciphertext;

    return "";
  }

  // ==========================================================
  // ENCRYPT
  // ==========================================================

  result =
    mbedtls_gcm_crypt_and_tag(
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

  if (result != 0)
  {
    Serial.println(
      "AES ENCRYPTION FAILED"
    );

    delete[] ciphertext;

    return "";
  }

  // ==========================================================
  // CONVERT TO HEX
  // ==========================================================

  String nonceHex =
    bytesToHex(
      nonce,
      GCM_NONCE_SIZE
    );

  String cipherHex =
    bytesToHex(
      ciphertext,
      plaintextLength
    );

  String tagHex =
    bytesToHex(
      tag,
      GCM_TAG_SIZE
    );

  delete[] ciphertext;

  // ==========================================================
  // CREATE ENCRYPTED PACKET
  // ==========================================================

  String packet =
    "E2E|" +
    nonceHex +
    "|" +
    cipherHex +
    "|" +
    tagHex;

  return packet;
}

// ============================================================
// AES-256-GCM DECRYPT
//
// INPUT:
//
// E2E|NONCE|CIPHERTEXT|TAG
// ============================================================

bool decryptMessage(
  String packet,
  String &plaintext
)
{
  packet.trim();

  // ==========================================================
  // FIND SEPARATORS
  // ==========================================================

  int p1 =
    packet.indexOf('|');

  int p2 =
    packet.indexOf(
      '|',
      p1 + 1
    );

  int p3 =
    packet.indexOf(
      '|',
      p2 + 1
    );

  if (
    p1 <= 0 ||
    p2 <= p1 ||
    p3 <= p2
  )
  {
    return false;
  }

  // ==========================================================
  // EXTRACT FIELDS
  // ==========================================================

  String type =
    packet.substring(
      0,
      p1
    );

  String nonceHex =
    packet.substring(
      p1 + 1,
      p2
    );

  String cipherHex =
    packet.substring(
      p2 + 1,
      p3
    );

  String tagHex =
    packet.substring(
      p3 + 1
    );

  // ==========================================================
  // CHECK PACKET TYPE
  // ==========================================================

  if (type != "E2E")
  {
    return false;
  }

  // ==========================================================
  // CHECK NONCE
  // ==========================================================

  if (
    nonceHex.length() !=
    GCM_NONCE_SIZE * 2
  )
  {
    return false;
  }

  // ==========================================================
  // CHECK TAG
  // ==========================================================

  if (
    tagHex.length() !=
    GCM_TAG_SIZE * 2
  )
  {
    return false;
  }

  // ==========================================================
  // CHECK CIPHERTEXT
  // ==========================================================

  if (
    cipherHex.length() == 0 ||
    cipherHex.length() % 2 != 0
  )
  {
    return false;
  }

  size_t cipherLength =
    cipherHex.length() / 2;

  // ==========================================================
  // CREATE BUFFERS
  // ==========================================================

  uint8_t nonce[GCM_NONCE_SIZE];

  uint8_t tag[GCM_TAG_SIZE];

  uint8_t *ciphertext =
    new uint8_t[cipherLength];

  uint8_t *decrypted =
    new uint8_t[cipherLength + 1];

  // ==========================================================
  // CONVERT NONCE
  // ==========================================================

  if (
    !hexToBytes(
      nonceHex,
      nonce,
      GCM_NONCE_SIZE
    )
  )
  {
    delete[] ciphertext;
    delete[] decrypted;

    return false;
  }

  // ==========================================================
  // CONVERT TAG
  // ==========================================================

  if (
    !hexToBytes(
      tagHex,
      tag,
      GCM_TAG_SIZE
    )
  )
  {
    delete[] ciphertext;
    delete[] decrypted;

    return false;
  }

  // ==========================================================
  // CONVERT CIPHERTEXT
  // ==========================================================

  if (
    !hexToBytes(
      cipherHex,
      ciphertext,
      cipherLength
    )
  )
  {
    delete[] ciphertext;
    delete[] decrypted;

    return false;
  }

  // ==========================================================
  // INITIALIZE AES-GCM
  // ==========================================================

  mbedtls_gcm_context gcm;

  mbedtls_gcm_init(&gcm);

  int result =
    mbedtls_gcm_setkey(
      &gcm,
      MBEDTLS_CIPHER_ID_AES,
      AES_KEY,
      256
    );

  if (result != 0)
  {
    mbedtls_gcm_free(&gcm);

    delete[] ciphertext;
    delete[] decrypted;

    return false;
  }

  // ==========================================================
  // AUTHENTICATED DECRYPTION
  // ==========================================================

  result =
    mbedtls_gcm_auth_decrypt(
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

  // ==========================================================
  // AUTHENTICATION / DECRYPTION FAILED
  // ==========================================================

  if (result != 0)
  {
    delete[] ciphertext;
    delete[] decrypted;

    return false;
  }

  // ==========================================================
  // TERMINATE STRING
  // ==========================================================

  decrypted[cipherLength] =
    '\0';

  plaintext =
    String(
      (char *)decrypted
    );

  delete[] ciphertext;
  delete[] decrypted;

  return true;
}

// ============================================================
// SEND DATA TO PHONE
// ============================================================

void sendToPhone(
  String data
)
{
  data.trim();

  if (data.length() == 0)
  {
    return;
  }

  if (!deviceConnected)
  {
    Serial.println();
    Serial.println(
      "PHONE NOT CONNECTED"
    );

    return;
  }

  txCharacteristic->setValue(
    data.c_str()
  );

  txCharacteristic->notify();

  Serial.println();
  Serial.println(
    "DATA SENT TO PHONE"
  );

  Serial.print(
    "Data: "
  );

  Serial.println(data);
}

// ============================================================
// GET NODE ADDRESS
// ============================================================

bool getDestinationAddress(
  String name,
  uint8_t &addH,
  uint8_t &addL
)
{
  name.trim();

  name.toUpperCase();

  if (name == "THANU")
  {
    addH = THANU_ADDH;
    addL = THANU_ADDL;

    return true;
  }

  if (name == "JESS")
  {
    addH = JESS_ADDH;
    addL = JESS_ADDL;

    return true;
  }

  if (name == "ASMI")
  {
    addH = ASMI_ADDH;
    addL = ASMI_ADDL;

    return true;
  }
  if (name == "KAVIN")
  {
    addH = KAVIN_ADDH;
    addL = KAVIN_ADDL;
    return true;
  }

  return false;
}

// ============================================================
// SEND ENCRYPTED MESSAGE THROUGH LORA
// ============================================================

void sendEncryptedMessage(
  String destination,
  String message
)
{
  destination.trim();
  message.trim();

  uint8_t addH;
  uint8_t addL;

  // ==========================================================
  // FIND DESTINATION
  // ==========================================================

  if (
    !getDestinationAddress(
      destination,
      addH,
      addL
    )
  )
  {
    Serial.println();
    Serial.println(
      "UNKNOWN DESTINATION"
    );

    return;
  }

  // ==========================================================
  // DON'T SEND TO SELF
  // ==========================================================

  if (
    destination.equalsIgnoreCase(
      MY_NAME
    )
  )
  {
    Serial.println();
    Serial.println(
      "CANNOT SEND TO YOURSELF"
    );

    return;
  }

  // ==========================================================
  // ENCRYPT
  // ==========================================================

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.println(
    "AES-256-GCM ENCRYPTION"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "Original message: "
  );

  Serial.println(message);

  String encrypted =
    encryptMessage(
      message
    );

  if (encrypted.length() == 0)
  {
    Serial.println(
      "ENCRYPTION FAILED"
    );

    return;
  }

  // ==========================================================
  // SHOW ENCRYPTED PACKET
  // ==========================================================

  Serial.println();

  Serial.println(
    "Encrypted packet:"
  );

  Serial.println(
    encrypted
  );

  // ==========================================================
  // SEND
  // ==========================================================

  Serial.println();

  Serial.println(
    "SENDING ENCRYPTED DATA THROUGH LORA..."
  );

  ResponseStatus rs =
    e220ttl.sendFixedMessage(
      addH,
      addL,
      LORA_CHANNEL,
      encrypted
    );

  if (
    rs.code == E220_SUCCESS
  )
  {
    Serial.println();

    Serial.println(
      "ENCRYPTED MESSAGE SENT SUCCESSFULLY"
    );
  }
  else
  {
    Serial.print(
      "LORA ERROR: "
    );

    Serial.println(
      rs.getResponseDescription()
    );
  }

  Serial.println(
    "========================================"
  );
}

// ============================================================
// BLE SERVER CALLBACKS
// ============================================================

class ServerCallbacks :
  public BLEServerCallbacks
{
  void onConnect(
    BLEServer *server
  )
  {
    deviceConnected = true;

    Serial.println();
    Serial.println(
      "========================================"
    );

    Serial.println(
      "PHONE CONNECTED"
    );

    Serial.println(
      "========================================"
    );
  }

  void onDisconnect(
    BLEServer *server
  )
  {
    deviceConnected = false;

    Serial.println();
    Serial.println(
      "PHONE DISCONNECTED"
    );

    delay(300);

    server->startAdvertising();

    Serial.println(
      "BLE ADVERTISING RESTARTED"
    );
  }
};

// ============================================================
// PHONE -> ESP32
//
// IMPORTANT:
//
// Your BLE library returns std::string.
//
// Therefore we MUST use:
//
// std::string rxValue = characteristic->getValue();
//
// ============================================================

class RxCallbacks :
  public BLECharacteristicCallbacks
{
  void onWrite(
    BLECharacteristic *characteristic
  )
  {
    // ========================================================
    // GET DATA FROM PHONE
    // ========================================================

    String data =
  characteristic->getValue();
    

    data.trim();

    if (data.length() == 0)
    {
      return;
    }

    // ========================================================
    // DISPLAY
    // ========================================================

    Serial.println();
    Serial.println(
      "========================================"
    );

    Serial.println(
      "DATA RECEIVED FROM PHONE"
    );

    Serial.println(
      "========================================"
    );

    Serial.print(
      "Phone Data: "
    );

    Serial.println(data);

    // ========================================================
    // FORMAT:
    //
    // ASMI|Hello
    // JESS|Hello
    // THANU|Hello
    // ========================================================

    int separator =
      data.indexOf('|');

    if (separator == -1)
    {
      Serial.println();

      Serial.println(
        "INVALID PHONE MESSAGE FORMAT"
      );

      Serial.println(
        "Use: ASMI|Hello"
      );

      Serial.println(
        "Use: JESS|Hello"
      );

      Serial.println(
        "Use: THANU|Hello"
      );
      Serial.println(
        "Use: KAVIN|Hello"
      );

      return;
    }

    // ========================================================
    // GET DESTINATION
    // ========================================================

    String destination =
      data.substring(
        0,
        separator
      );

    // ========================================================
    // GET MESSAGE
    // ========================================================

    String text =
      data.substring(
        separator + 1
      );

    destination.trim();
    text.trim();

    // ========================================================
    // VALIDATE
    // ========================================================

    if (
      destination.length() == 0 ||
      text.length() == 0
    )
    {
      Serial.println();

      Serial.println(
        "INVALID DESTINATION OR MESSAGE"
      );

      return;
    }

    // ========================================================
    // DISPLAY
    // ========================================================

    Serial.println();

    Serial.println(
      "----------------------------------------"
    );

    Serial.print(
      "Destination: "
    );

    Serial.println(destination);

    Serial.print(
      "Message: "
    );

    Serial.println(text);

    Serial.println(
      "----------------------------------------"
    );

    // ========================================================
    // ENCRYPT + SEND
    // ========================================================

    sendEncryptedMessage(
      destination,
      text
    );

    Serial.println(
      "========================================"
    );
  }
};

// ============================================================
// LORA RECEIVE
//
// LORA -> ESP32 -> DECRYPT -> BLE -> PHONE
// ============================================================

void handleLoRa()
{
  if (!E220Serial.available())
  {
    return;
  }

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.println(
    "ENCRYPTED LORA DATA RECEIVED"
  );

  Serial.println(
    "========================================"
  );

  // ==========================================================
  // RECEIVE
  // ==========================================================

  ResponseContainer rc =
    e220ttl.receiveMessage();

  if (
    rc.status.code !=
    E220_SUCCESS
  )
  {
    Serial.print(
      "LORA RECEIVE ERROR: "
    );

    Serial.println(
      rc.status.getResponseDescription()
    );

    return;
  }

  // ==========================================================
  // GET ENCRYPTED PACKET
  // ==========================================================

  String encryptedPacket =
    rc.data;

  encryptedPacket.trim();

  if (
    encryptedPacket.length() == 0
  )
  {
    return;
  }

  Serial.println();

  Serial.println(
    "Encrypted packet received:"
  );

  Serial.println(
    encryptedPacket
  );

  // ==========================================================
  // DECRYPT
  // ==========================================================

  Serial.println();

  Serial.println(
    "DECRYPTING..."
  );

  String plaintext;

  bool success =
    decryptMessage(
      encryptedPacket,
      plaintext
    );

  // ==========================================================
  // DECRYPTION FAILED
  // ==========================================================

  if (!success)
  {
    Serial.println();

    Serial.println(
      "========================================"
    );

    Serial.println(
      "DECRYPTION FAILED"
    );

    Serial.println(
      "========================================"
    );

    Serial.println(
      "Possible reasons:"
    );

    Serial.println(
      "1. Wrong AES key"
    );

    Serial.println(
      "2. Corrupted packet"
    );

    Serial.println(
      "3. Invalid packet format"
    );

    Serial.println(
      "Message NOT sent to phone"
    );

    return;
  }

  // ==========================================================
  // DECRYPTION SUCCESS
  // ==========================================================

  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "DECRYPTION SUCCESS"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "Decrypted message: "
  );

  Serial.println(
    plaintext
  );

  // ==========================================================
  // SEND PLAINTEXT TO PHONE
  // ==========================================================

  sendToPhone(
    plaintext
  );

  Serial.println(
    "LORA -> AES DECRYPT -> BLE -> PHONE"
  );

  Serial.println(
    "========================================"
  );
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );

  delay(1000);

  // ==========================================================
  // E220 CONTROL PINS
  // ==========================================================

  pinMode(
    PIN_M0,
    OUTPUT
  );

  pinMode(
    PIN_M1,
    OUTPUT
  );

  pinMode(
    PIN_AUX,
    INPUT
  );

  // ==========================================================
  // NORMAL MODE
  // ==========================================================

  digitalWrite(
    PIN_M0,
    LOW
  );

  digitalWrite(
    PIN_M1,
    LOW
  );

  delay(100);

  // ==========================================================
  // E220 UART
  // ==========================================================

  E220Serial.begin(
    9600,
    SERIAL_8N1,
    LORA_RX,
    LORA_TX
  );

  delay(500);

  // ==========================================================
  // START E220
  // ==========================================================

  e220ttl.begin();

  delay(500);

  // ==========================================================
  // DISPLAY LORA INFORMATION
  // ==========================================================

  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "ESP32 E220 ENCRYPTED CHAT"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "MY NODE: "
  );

  Serial.println(
    MY_NAME
  );

  Serial.print(
    "NODE ID: "
  );

  Serial.println(
    MY_NODE_ID
  );

  Serial.println(
    "LORA CHANNEL: 18"
  );

  Serial.println(
    "FREQUENCY: 868.125 MHz"
  );

  Serial.println(
    "AIR RATE: 2.4 kbps"
  );

  Serial.println(
    "UART: 9600 8N1"
  );

  Serial.println(
    "POWER: 22 dBm"
  );

  Serial.println(
    "ENCRYPTION: AES-256-GCM"
  );

  Serial.println(
    "========================================"
  );

  // ==========================================================
  // BLE
  // ==========================================================

  String bleName =
    String(MY_NAME) +
    "_CHAT";

  BLEDevice::init(
    bleName.c_str()
  );

  // ==========================================================
  // CREATE BLE SERVER
  // ==========================================================

  BLEServer *server =
    BLEDevice::createServer();

  server->setCallbacks(
    new ServerCallbacks()
  );

  // ==========================================================
  // CREATE SERVICE
  // ==========================================================

  BLEService *service =
    server->createService(
      SERVICE_UUID
    );

  // ==========================================================
  // ESP32 -> PHONE
  // ==========================================================

  txCharacteristic =
    service->createCharacteristic(
      TX_CHARACTERISTIC,
      BLECharacteristic::PROPERTY_NOTIFY
    );

  txCharacteristic->addDescriptor(
    new BLE2902()
  );

  // ==========================================================
  // PHONE -> ESP32
  // ==========================================================

  BLECharacteristic *rxCharacteristic =
    service->createCharacteristic(
      RX_CHARACTERISTIC,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR
    );

  rxCharacteristic->setCallbacks(
    new RxCallbacks()
  );

  // ==========================================================
  // START BLE SERVICE
  // ==========================================================

  service->start();

  // ==========================================================
  // BLE ADVERTISING
  // ==========================================================

  BLEAdvertising *advertising =
    BLEDevice::getAdvertising();

  advertising->addServiceUUID(
    SERVICE_UUID
  );

  advertising->setScanResponse(
    true
  );

  BLEDevice::startAdvertising();

  // ==========================================================
  // READY
  // ==========================================================

  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "NODE READY"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "BLE NAME: "
  );

  Serial.println(
    bleName
  );

  Serial.println(
    "STATUS: READY"
  );

  Serial.println(
    "========================================"
  );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ==========================================================
  // CHECK FOR LORA DATA
  // ==========================================================

  handleLoRa();

  delay(5);
}