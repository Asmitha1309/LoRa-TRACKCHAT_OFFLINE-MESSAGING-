#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// BLE UUIDs - SAME FOR BOTH DEVICES
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
// GLOBAL
// ============================================================

BLECharacteristic *txCharacteristic = nullptr;
bool deviceConnected = false;

// ============================================================
// BLE SERVER
// ============================================================

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *server)
  {
    deviceConnected = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("Asmi PHONE CONNECTED");
    Serial.println("================================");
  }

  void onDisconnect(BLEServer *server)
  {
    deviceConnected = false;

    Serial.println("PHONE DISCONNECTED");

    delay(300);
    server->startAdvertising();

    Serial.println("BLE ADVERTISING RESTARTED");
  }
};

// ============================================================
// PHONE 1 -> ESP32-1 -> E220
// ============================================================

class RxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic)
  {
    // IMPORTANT:
    // Your BLE library returns Arduino String

    String message = characteristic->getValue();

    if (message.length() == 0)
      return;

    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("PHONE 3 MESSAGE");
    Serial.println("--------------------------------");
    Serial.println(message);

    // --------------------------------------------------------
    // SEND TO E220
    // --------------------------------------------------------

    LoRaSerial.write(0x02);

    LoRaSerial.print(message);

    LoRaSerial.write(0x03);

    LoRaSerial.flush();

    Serial.println("SENT TO E220");
    Serial.println("--------------------------------");
  }
};

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  // ==========================================================
  // E220
  // ==========================================================

  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);
  pinMode(PIN_AUX, INPUT);

  // NORMAL / TRANSPARENT MODE
  digitalWrite(PIN_M0, LOW);
  digitalWrite(PIN_M1, LOW);

  LoRaSerial.begin(
    9600,
    SERIAL_8N1,
    LORA_RX,
    LORA_TX
  );

  // ==========================================================
  // BLE
  // ==========================================================

  BLEDevice::init("Asmi");

  BLEServer *server = BLEDevice::createServer();

  server->setCallbacks(
    new ServerCallbacks()
  );

  BLEService *service =
      server->createService(SERVICE_UUID);

  // ESP32 -> PHONE
  txCharacteristic =
      service->createCharacteristic(
        TX_CHARACTERISTIC,
        BLECharacteristic::PROPERTY_NOTIFY
      );

  txCharacteristic->addDescriptor(
    new BLE2902()
  );

  // PHONE -> ESP32
  BLECharacteristic *rxCharacteristic =
      service->createCharacteristic(
        RX_CHARACTERISTIC,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
      );

  rxCharacteristic->setCallbacks(
    new RxCallbacks()
  );

  service->start();

  // ==========================================================
  // ADVERTISING
  // ==========================================================

  BLEAdvertising *advertising =
      BLEDevice::getAdvertising();

  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);

  BLEDevice::startAdvertising();

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-1 READY");
  Serial.println("BLE NAME : Asmi");
  Serial.println("E220     : READY");
  Serial.println("WAITING FOR PHONE...");
  Serial.println("================================");
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ==========================================================
  // E220 -> ESP32-1 -> PHONE 1
  // ==========================================================

  static String receivedMessage = "";

  while (LoRaSerial.available())
  {
    byte data = LoRaSerial.read();

    // START
    if (data == 0x02)
    {
      receivedMessage = "";
    }

    // END
    else if (data == 0x03)
    {
      if (receivedMessage.length() > 0)
      {
        Serial.println();
        Serial.println("--------------------------------");
        Serial.println("MESSAGE FROM PERSON 3);
        Serial.println("--------------------------------");
        Serial.println(receivedMessage);

        if (deviceConnected)
        {
          txCharacteristic->setValue(
            receivedMessage.c_str()
          );

          txCharacteristic->notify();

          Serial.println("SENT TO PHONE 1");
        }

        receivedMessage = "";
      }
    }

    // NORMAL DATA
    else
    {
      receivedMessage += (char)data;
    }
  }

  delay(5);
}