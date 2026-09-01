#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// BLE UUIDs
// ============================================================

#define SERVICE_UUID       "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_CHARACTERISTIC  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_CHARACTERISTIC  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================================================
// GLOBALS
// ============================================================

BLECharacteristic *txCharacteristic = nullptr;

bool deviceConnected = false;

// ============================================================
// SERVER CALLBACKS
// ============================================================

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *server)
  {
    deviceConnected = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("PHONE CONNECTED");
    Serial.println("================================");
  }

  void onDisconnect(BLEServer *server)
  {
    deviceConnected = false;

    Serial.println();
    Serial.println("================================");
    Serial.println("PHONE DISCONNECTED");
    Serial.println("================================");

    delay(500);

    server->startAdvertising();

    Serial.println("BLE advertising restarted");
  }
};

// ============================================================
// PHONE -> ESP32
// ============================================================

class RxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic)
  {
    String message = characteristic->getValue();

    if (message.length() == 0)
    {
      return;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("MESSAGE FROM PHONE");
    Serial.println("========================================");

    Serial.print("Message: ");
    Serial.println(message);

    Serial.println("========================================");

    // IMPORTANT:
    // DO NOT send this message back to the phone.
    //
    // Phone -> ESP32 ends here.
  }
};

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 LORA TRACKER");
  Serial.println("BLE STARTING...");
  Serial.println("================================");

  // ----------------------------------------------------------
  // Initialize BLE
  // ----------------------------------------------------------

  BLEDevice::init("ESP32_LORA_NODE");

  // ----------------------------------------------------------
  // Create BLE server
  // ----------------------------------------------------------

  BLEServer *server =
      BLEDevice::createServer();

  server->setCallbacks(
      new ServerCallbacks()
  );

  // ----------------------------------------------------------
  // Create BLE service
  // ----------------------------------------------------------

  BLEService *service =
      server->createService(
          SERVICE_UUID
      );

  // ==========================================================
  // ESP32 -> PHONE
  // TX CHARACTERISTIC
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
  // RX CHARACTERISTIC
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

  // ----------------------------------------------------------
  // Start BLE service
  // ----------------------------------------------------------

  service->start();

  // ----------------------------------------------------------
  // BLE advertising
  // ----------------------------------------------------------

  BLEAdvertising *advertising =
      BLEDevice::getAdvertising();

  advertising->addServiceUUID(
      SERVICE_UUID
  );

  advertising->setScanResponse(true);

  BLEDevice::startAdvertising();

  Serial.println();
  Serial.println("================================");
  Serial.println("BLE ADVERTISING STARTED");
  Serial.println("Device: ESP32_LORA_NODE");
  Serial.println("Waiting for phone...");
  Serial.println("================================");
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ==========================================================
  // ESP32 SERIAL MONITOR -> PHONE
  // ==========================================================

  if (Serial.available())
  {
    String message =
        Serial.readStringUntil('\n');

    message.trim();

    if (message.length() == 0)
    {
      return;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("MESSAGE FROM ESP32");
    Serial.println("================================");

    Serial.print("Message: ");
    Serial.println(message);

    // --------------------------------------------------------
    // Send to Flutter only when phone is connected
    // --------------------------------------------------------

    if (deviceConnected)
    {
      txCharacteristic->setValue(
          message.c_str()
      );

      txCharacteristic->notify();

      Serial.println();
      Serial.println("MESSAGE SENT TO PHONE");
      Serial.println("================================");
    }
    else
    {
      Serial.println();
      Serial.println("PHONE NOT CONNECTED");
      Serial.println("Message NOT sent");
      Serial.println("================================");
    }
  }

  delay(10);
}