#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// CHANGE ONLY THIS LINE ON EACH ESP32
// ============================================================

#define MY_NODE "THANU"

// ESP32 1 -> "ASMI"
// ESP32 2 -> "THANU"
// ESP32 3 -> "JESS"

// ============================================================
// BLE UUIDs
// SAME FOR ALL 3 ESP32s
// ============================================================

#define SERVICE_UUID       "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_CHARACTERISTIC  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_CHARACTERISTIC  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================================================
// E220 PINS
// ============================================================

#define LORA_RX 16
#define LORA_TX 17

#define PIN_M0  25
#define PIN_M1  26
#define PIN_AUX 27

HardwareSerial LoRaSerial(2);

// ============================================================
// GLOBAL VARIABLES
// ============================================================

BLECharacteristic *txCharacteristic = nullptr;

bool deviceConnected = false;

// ============================================================
// BLE SERVER CALLBACKS
// ============================================================

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *server)
  {
    deviceConnected = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("PHONE CONNECTED");
    Serial.print("NODE : ");
    Serial.println(MY_NODE);
    Serial.println("================================");
  }

  void onDisconnect(BLEServer *server)
  {
    deviceConnected = false;

    Serial.println();
    Serial.println("PHONE DISCONNECTED");

    delay(300);

    server->startAdvertising();

    Serial.println("BLE ADVERTISING RESTARTED");
  }
};

// ============================================================
// PHONE -> ESP32 -> LORA
//
// Phone sends:
// JESS|Hello Jess
//
// ESP32 converts it to:
// ASMI|JESS|Hello Jess
// ============================================================

class RxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic)
  {
    String message = characteristic->getValue();

    if (message.length() == 0)
      return;

    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("MESSAGE FROM PHONE");
    Serial.println("--------------------------------");

    Serial.print("Received : ");
    Serial.println(message);

    // ========================================================
    // FIND DESTINATION SEPARATOR
    // ========================================================

    int separator = message.indexOf('|');

    if (separator == -1)
    {
      Serial.println("ERROR: Invalid message format");
      Serial.println("Expected: DESTINATION|MESSAGE");
      Serial.println("--------------------------------");
      return;
    }

    // ========================================================
    // EXTRACT DESTINATION
    // ========================================================

    String destination = message.substring(0, separator);

    String text = message.substring(separator + 1);

    destination.trim();
    text.trim();

    if (destination.length() == 0 || text.length() == 0)
    {
      Serial.println("ERROR: Empty destination/message");
      Serial.println("--------------------------------");
      return;
    }

    // ========================================================
    // CREATE LORA PACKET
    //
    // FORMAT:
    //
    // FROM|TO|MESSAGE
    //
    // Example:
    //
    // ASMI|JESS|Hello Jess
    // ========================================================

    String packet;

    packet = String(MY_NODE);
    packet += "|";
    packet += destination;
    packet += "|";
    packet += text;

    Serial.println();
    Serial.println("LORA PACKET");
    Serial.println("--------------------------------");
    Serial.println(packet);

    // ========================================================
    // SEND START BYTE
    // ========================================================

    LoRaSerial.write(0x02);

    // ========================================================
    // SEND PACKET
    // ========================================================

    LoRaSerial.print(packet);

    // ========================================================
    // SEND END BYTE
    // ========================================================

    LoRaSerial.write(0x03);

    LoRaSerial.flush();

    Serial.println("SENT THROUGH E220");
    Serial.println("--------------------------------");
  }
};

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  // ==========================================================
  // PRINT NODE INFORMATION
  // ==========================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("   LORA TRACK CHAT NODE");
  Serial.println("================================");

  Serial.print("MY NODE : ");
  Serial.println(MY_NODE);

  // ==========================================================
  // E220 PIN CONFIGURATION
  // ==========================================================

  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);
  pinMode(PIN_AUX, INPUT);

  // ==========================================================
  // NORMAL / TRANSPARENT MODE
  // M0 = LOW
  // M1 = LOW
  // ==========================================================

  digitalWrite(PIN_M0, LOW);
  digitalWrite(PIN_M1, LOW);

  delay(100);

  // ==========================================================
  // E220 SERIAL
  // ==========================================================

  LoRaSerial.begin(
    9600,
    SERIAL_8N1,
    LORA_RX,
    LORA_TX
  );

  Serial.println("E220 SERIAL READY");

  // ==========================================================
  // BLE INITIALIZATION
  // ==========================================================

  String bleName = String(MY_NODE) + "_CHAT";

  BLEDevice::init(bleName.c_str());

  BLEServer *server = BLEDevice::createServer();

  server->setCallbacks(
    new ServerCallbacks()
  );

  // ==========================================================
  // BLE SERVICE
  // ==========================================================

  BLEService *service =
      server->createService(SERVICE_UUID);

  // ==========================================================
  // ESP32 -> PHONE
  // NOTIFICATION CHARACTERISTIC
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
  // WRITE CHARACTERISTIC
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

  advertising->addServiceUUID(SERVICE_UUID);

  advertising->setScanResponse(true);

  BLEDevice::startAdvertising();

  // ==========================================================
  // READY
  // ==========================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 NODE READY");
  Serial.println("================================");

  Serial.print("NODE NAME : ");
  Serial.println(MY_NODE);

  Serial.print("BLE NAME  : ");
  Serial.println(bleName);

  Serial.println("E220      : READY");
  Serial.println("BLE       : ADVERTISING");

  Serial.println();
  Serial.println("WAITING FOR MESSAGE...");
  Serial.println("================================");
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  static String receivedPacket = "";

  // ==========================================================
  // E220 -> ESP32
  // ==========================================================

  while (LoRaSerial.available())
  {
    byte data = LoRaSerial.read();

    // ========================================================
    // START OF PACKET
    // ========================================================

    if (data == 0x02)
    {
      receivedPacket = "";
    }

    // ========================================================
    // END OF PACKET
    // ========================================================

    else if (data == 0x03)
    {
      if (receivedPacket.length() > 0)
      {
        processLoRaPacket(receivedPacket);

        receivedPacket = "";
      }
    }

    // ========================================================
    // NORMAL DATA
    // ========================================================

    else
    {
      receivedPacket += (char)data;
    }
  }

  delay(5);
}

// ============================================================
// PROCESS RECEIVED LORA PACKET
// ============================================================

void processLoRaPacket(String packet)
{
  Serial.println();
  Serial.println("================================");
  Serial.println("LORA PACKET RECEIVED");
  Serial.println("================================");

  Serial.print("Packet : ");
  Serial.println(packet);

  // ==========================================================
  // FIND FIRST |
  // ==========================================================

  int firstSeparator = packet.indexOf('|');

  if (firstSeparator == -1)
  {
    Serial.println("ERROR: Invalid packet");
    Serial.println("================================");
    return;
  }

  // ==========================================================
  // FIND SECOND |
  // ==========================================================

  int secondSeparator =
      packet.indexOf('|', firstSeparator + 1);

  if (secondSeparator == -1)
  {
    Serial.println("ERROR: Invalid packet");
    Serial.println("================================");
    return;
  }

  // ==========================================================
  // EXTRACT SOURCE
  // ==========================================================

  String sender =
      packet.substring(
        0,
        firstSeparator
      );

  // ==========================================================
  // EXTRACT DESTINATION
  // ==========================================================

  String destination =
      packet.substring(
        firstSeparator + 1,
        secondSeparator
      );

  // ==========================================================
  // EXTRACT MESSAGE
  // ==========================================================

  String message =
      packet.substring(
        secondSeparator + 1
      );

  sender.trim();
  destination.trim();
  message.trim();

  // ==========================================================
  // DISPLAY PACKET INFORMATION
  // ==========================================================

  Serial.print("FROM        : ");
  Serial.println(sender);

  Serial.print("TO          : ");
  Serial.println(destination);

  Serial.print("MESSAGE     : ");
  Serial.println(message);

  Serial.print("MY NODE     : ");
  Serial.println(MY_NODE);

  // ==========================================================
  // CHECK DESTINATION
  // ==========================================================

  if (
    destination != MY_NODE &&
    destination != "ALL"
  )
  {
    // ========================================================
    // MESSAGE IS FOR ANOTHER NODE
    // ========================================================

    Serial.println();
    Serial.println("MESSAGE NOT FOR THIS NODE");

    Serial.println("IGNORING PACKET");

    Serial.println("================================");

    return;
  }

  // ==========================================================
  // MESSAGE IS FOR THIS NODE
  // ==========================================================

  Serial.println();
  Serial.println("******** MESSAGE FOR ME ********");

  Serial.print("FROM : ");
  Serial.println(sender);

  Serial.print("TEXT : ");
  Serial.println(message);

  // ==========================================================
  // SEND MESSAGE TO PHONE
  // ==========================================================

  if (deviceConnected)
  {
    txCharacteristic->setValue(
      message.c_str()
    );

    txCharacteristic->notify();

    Serial.println();
    Serial.println("SENT TO PHONE");
  }
  else
  {
    Serial.println();
    Serial.println("PHONE NOT CONNECTED");
  }

  Serial.println("================================");
}