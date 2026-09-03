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
// UART
//
// Temporary replacement for LoRa.
//
// ESP32 #1 TX GPIO17 -> ESP32 #2 RX GPIO16
// ESP32 #1 RX GPIO16 <- ESP32 #2 TX GPIO17
// GND <--------------> GND
// ============================================================

#define UART_RX_PIN 16
#define UART_TX_PIN 17

#define UART_BAUD 115200

// ============================================================
// GLOBALS
// ============================================================

BLECharacteristic *txCharacteristic = nullptr;

bool deviceConnected = false;

// UART receive buffer
String uartReceiveBuffer = "";

// ============================================================
// SEND DATA TO PHONE
// ============================================================

void sendToPhone(String message)
{
  message.trim();

  if (message.length() == 0)
  {
    return;
  }

  if (!deviceConnected)
  {
    Serial.println();
    Serial.println("PHONE NOT CONNECTED");
    Serial.println("Data NOT sent to phone");

    return;
  }

  txCharacteristic->setValue(
      message.c_str()
  );

  txCharacteristic->notify();

  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("DATA SENT TO PHONE");
  Serial.print("Data: ");
  Serial.println(message);
  Serial.println("----------------------------------------");
}

// ============================================================
// SEND DATA TO OTHER ESP32
// ============================================================

void sendToOtherESP32(String message)
{
  message.trim();

  if (message.length() == 0)
  {
    return;
  }

  Serial2.println(message);

  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("DATA SENT TO OTHER ESP32");
  Serial.print("UART TX: ");
  Serial.println(message);
  Serial.println("----------------------------------------");
}

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

    message.trim();

    Serial.println();
    Serial.println("========================================");
    Serial.println("DATA RECEIVED FROM PHONE");
    Serial.println("================================");

    Serial.print("BLE RX: ");
    Serial.println(message);

    // ========================================================
    // LOCATION PACKET
    //
    // LOC,DeviceID,Latitude,Longitude,Timestamp,Battery
    // ========================================================

    if (message.startsWith("LOC,"))
    {
      Serial.println();
      Serial.println("LOCATION PACKET DETECTED");

      Serial.println("----------------------------------------");

      int firstComma =
          message.indexOf(',');

      int secondComma =
          message.indexOf(
              ',',
              firstComma + 1
          );

      int thirdComma =
          message.indexOf(
              ',',
              secondComma + 1
          );

      int fourthComma =
          message.indexOf(
              ',',
              thirdComma + 1
          );

      int fifthComma =
          message.indexOf(
              ',',
              fourthComma + 1
          );

      if (firstComma > 0 &&
          secondComma > firstComma &&
          thirdComma > secondComma &&
          fourthComma > thirdComma &&
          fifthComma > fourthComma)
      {
        String deviceId =
            message.substring(
                firstComma + 1,
                secondComma
            );

        String latitude =
            message.substring(
                secondComma + 1,
                thirdComma
            );

        String longitude =
            message.substring(
                thirdComma + 1,
                fourthComma
            );

        String timestamp =
            message.substring(
                fourthComma + 1,
                fifthComma
            );

        String battery =
            message.substring(
                fifthComma + 1
            );

        Serial.print("Device ID : ");
        Serial.println(deviceId);

        Serial.print("Latitude  : ");
        Serial.println(latitude);

        Serial.print("Longitude : ");
        Serial.println(longitude);

        Serial.print("Timestamp : ");
        Serial.println(timestamp);

        Serial.print("Battery   : ");
        Serial.print(battery);
        Serial.println("%");

        Serial.println("----------------------------------------");
        Serial.println("LOCATION DATA RECEIVED SUCCESSFULLY");
      }
      else
      {
        Serial.println("----------------------------------------");
        Serial.println("INVALID LOCATION PACKET");
      }
    }
    else
    {
      // ======================================================
      // NORMAL TEXT MESSAGE
      // ======================================================

      Serial.println();
      Serial.println("NORMAL TEXT MESSAGE");
      Serial.println("----------------------------------------");
      Serial.println(message);
    }

    // ========================================================
    // IMPORTANT
    //
    // Forward EVERYTHING received from the phone to the
    // second ESP32 through UART.
    //
    // This is our temporary replacement for LoRa TX.
    // ========================================================

    sendToOtherESP32(message);

    Serial.println("========================================");
  }
};

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(500);

  // ==========================================================
  // START UART BETWEEN ESP32s
  // ==========================================================

  Serial2.begin(
      UART_BAUD,
      SERIAL_8N1,
      UART_RX_PIN,
      UART_TX_PIN
  );

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 LORA TRACKER");
  Serial.println("BLE + UART TEST MODE");
  Serial.println("================================");

  Serial.println();
  Serial.println("UART CONFIGURATION");
  Serial.println("--------------------------------");
  Serial.print("RX GPIO : ");
  Serial.println(UART_RX_PIN);
  Serial.print("TX GPIO : ");
  Serial.println(UART_TX_PIN);
  Serial.print("BAUD    : ");
  Serial.println(UART_BAUD);
  Serial.println("--------------------------------");

  // ----------------------------------------------------------
  // Initialize BLE
  // ----------------------------------------------------------

  BLEDevice::init("ESP32_LORA_NODE3");

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
// UART RECEIVE FROM OTHER ESP32
// ============================================================

void handleUART()
{
  while (Serial2.available())
  {
    char incoming =
        Serial2.read();

    // --------------------------------------------------------
    // Ignore carriage return
    // --------------------------------------------------------

    if (incoming == '\r')
    {
      continue;
    }

    // --------------------------------------------------------
    // New line = complete message
    // --------------------------------------------------------

    if (incoming == '\n')
    {
      uartReceiveBuffer.trim();

      if (uartReceiveBuffer.length() > 0)
      {
        String message =
            uartReceiveBuffer;

        uartReceiveBuffer = "";

        Serial.println();
        Serial.println("========================================");
        Serial.println("DATA RECEIVED FROM OTHER ESP32");
        Serial.println("================================");

        Serial.print("UART RX: ");
        Serial.println(message);

        // ====================================================
        // LOCATION PACKET
        // ====================================================

        if (message.startsWith("LOC,"))
        {
          Serial.println();
          Serial.println("LOCATION PACKET FROM OTHER ESP32");

          Serial.println("----------------------------------------");

          int firstComma =
              message.indexOf(',');

          int secondComma =
              message.indexOf(
                  ',',
                  firstComma + 1
              );

          int thirdComma =
              message.indexOf(
                  ',',
                  secondComma + 1
              );

          int fourthComma =
              message.indexOf(
                  ',',
                  thirdComma + 1
              );

          int fifthComma =
              message.indexOf(
                  ',',
                  fourthComma + 1
              );

          if (firstComma > 0 &&
              secondComma > firstComma &&
              thirdComma > secondComma &&
              fourthComma > thirdComma &&
              fifthComma > fourthComma)
          {
            String deviceId =
                message.substring(
                    firstComma + 1,
                    secondComma
                );

            String latitude =
                message.substring(
                    secondComma + 1,
                    thirdComma
                );

            String longitude =
                message.substring(
                    thirdComma + 1,
                    fourthComma
                );

            String timestamp =
                message.substring(
                    fourthComma + 1,
                    fifthComma
                );

            String battery =
                message.substring(
                    fifthComma + 1
                );

            Serial.print("Device ID : ");
            Serial.println(deviceId);

            Serial.print("Latitude  : ");
            Serial.println(latitude);

            Serial.print("Longitude : ");
            Serial.println(longitude);

            Serial.print("Timestamp : ");
            Serial.println(timestamp);

            Serial.print("Battery   : ");
            Serial.print(battery);
            Serial.println("%");

            Serial.println("----------------------------------------");
            Serial.println("LOCATION DATA RECEIVED FROM NODE");
          }
          else
          {
            Serial.println("----------------------------------------");
            Serial.println("INVALID LOCATION PACKET");
          }
        }
        else
        {
          // ==================================================
          // NORMAL TEXT MESSAGE
          // ==================================================

          Serial.println();
          Serial.println("NORMAL TEXT MESSAGE FROM NODE");
          Serial.println("----------------------------------------");
          Serial.println(message);
        }

        // ====================================================
        // UART RX -> BLE TX
        //
        // Send received data to the phone connected to this
        // ESP32.
        // ====================================================

        sendToPhone(message);

        Serial.println("========================================");
      }
    }
    else
    {
      // ------------------------------------------------------
      // Add character to buffer
      // ------------------------------------------------------

      uartReceiveBuffer += incoming;

      // ------------------------------------------------------
      // Safety limit
      // ------------------------------------------------------

      if (uartReceiveBuffer.length() > 512)
      {
        Serial.println();
        Serial.println("UART BUFFER OVERFLOW");
        Serial.println("Clearing buffer");

        uartReceiveBuffer = "";
      }
    }
  }
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ==========================================================
  // OTHER ESP32 -> THIS ESP32
  // UART RECEIVE
  // ==========================================================

  handleUART();

  // ==========================================================
  // SERIAL MONITOR -> PHONE
  //
  // Existing functionality preserved.
  //
  // If you type something into Serial Monitor, it will be
  // sent directly to the connected phone.
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
    Serial.println("MESSAGE FROM ESP32 SERIAL");
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