#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// BLE UUIDs — KEEP UNCHANGED
// ============================================================

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================================================
// UART2
// ESP32 A TX -> ESP32 B RX
// ESP32 A RX <- ESP32 B TX
// ============================================================

#define UART_RX_PIN 16
#define UART_TX_PIN 17

#define UART_BAUD_RATE 115200

HardwareSerial SerialLoRa(2);

// ============================================================
// VOICE BINARY PROTOCOL
//
// Flutter sends:
//
// "VOIC"
// version
// messageId
// chunkIndex
// totalChunks
// flags
// payloadLength
// payload
// CRC16
//
// ESP32 DOES NOT MODIFY THIS DATA.
//
// ESP32 UART wrapper:
//
// 0xAA
// 0x55
// uint16_t payloadLength
// payload
//
// ============================================================

#define UART_FRAME_START_1 0xAA
#define UART_FRAME_START_2 0x55

#define VOICE_MAGIC_0 'V'
#define VOICE_MAGIC_1 'O'
#define VOICE_MAGIC_2 'I'
#define VOICE_MAGIC_3 'C'

#define MAX_BINARY_PACKET_SIZE 1024

// ============================================================
// BLE
// ============================================================

BLECharacteristic *txCharacteristic;
BLECharacteristic *rxCharacteristic;

bool deviceConnected = false;

// ============================================================
// EXISTING TEXT UART BUFFER
// ============================================================

String uartReceiveBuffer = "";

// ============================================================
// UART DISPATCHER STATE
//
// IMPORTANT:
// ONLY handleUART() READS SerialLoRa.
//
// This prevents voice and text handlers from stealing
// bytes from each other.
// ============================================================

enum UartReceiveMode
{
  UART_IDLE,
  UART_BINARY_LENGTH_1,
  UART_BINARY_LENGTH_2,
  UART_BINARY_PAYLOAD,
  UART_TEXT
};

UartReceiveMode uartMode = UART_IDLE;

uint16_t binaryExpectedLength = 0;
uint16_t binaryReceivedLength = 0;

uint8_t binaryReceiveBuffer[MAX_BINARY_PACKET_SIZE];

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void sendToPhone(String message);
void sendBinaryToPhone(const uint8_t *data, size_t length);

void sendToOtherESP32(String message);
void sendBinaryToOtherESP32(const uint8_t *data, size_t length);

void handleUART();
void handleSerialMonitor();

void processTextFromUART(String message);
void processBinaryFromUART(const uint8_t *data, size_t length);

bool isVoicePacket(const uint8_t *data, size_t length);

void parseLocationPacket(String message);

// ============================================================
// SEND NORMAL TEXT TO PHONE
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
    Serial.println("PHONE NOT CONNECTED");
    return;
  }

  Serial.println("================================");
  Serial.println("UART -> PHONE");
  Serial.print("MESSAGE: ");
  Serial.println(message);
  Serial.println("================================");

  txCharacteristic->setValue(message.c_str());
  txCharacteristic->notify();
}

// ============================================================
// SEND BINARY DATA TO PHONE
//
// IMPORTANT:
// DO NOT convert binary data to String.
// DO NOT trim.
// DO NOT UTF-8 decode.
// ============================================================

void sendBinaryToPhone(const uint8_t *data, size_t length)
{
  if (data == nullptr || length == 0)
  {
    return;
  }

  if (!deviceConnected)
  {
    Serial.println("VOICE: PHONE NOT CONNECTED");
    return;
  }

  Serial.println("================================");
  Serial.println("VOICE BINARY DATA -> PHONE");
  Serial.print("Length: ");
  Serial.println(length);
  Serial.println("================================");

  txCharacteristic->setValue(
      (uint8_t *)data,
      length
  );

  txCharacteristic->notify();
}

// ============================================================
// SEND NORMAL TEXT TO OTHER ESP32
//
// EXISTING LOGIC
// ============================================================

void sendToOtherESP32(String message)
{
  message.trim();

  if (message.length() == 0)
  {
    return;
  }

  Serial.println("================================");
  Serial.println("PHONE -> UART");
  Serial.print("MESSAGE: ");
  Serial.println(message);
  Serial.println("================================");

  SerialLoRa.println(message);

  SerialLoRa.flush();
}

// ============================================================
// SEND BINARY VOICE PACKET TO OTHER ESP32
//
// UART FRAME:
//
// AA
// 55
// LENGTH MSB
// LENGTH LSB
// PAYLOAD
//
// The payload is the EXACT Flutter VOIC packet.
// ============================================================

void sendBinaryToOtherESP32(
    const uint8_t *data,
    size_t length
)
{
  if (data == nullptr || length == 0)
  {
    return;
  }

  if (length > MAX_BINARY_PACKET_SIZE)
  {
    Serial.println("VOICE: Binary packet too large");
    return;
  }

  Serial.println("================================");
  Serial.println("VOICE BINARY PACKET -> UART");
  Serial.print("Payload Length: ");
  Serial.println(length);
  Serial.println("================================");

  // ----------------------------------------------------------
  // FRAME HEADER
  // ----------------------------------------------------------

  SerialLoRa.write(UART_FRAME_START_1);
  SerialLoRa.write(UART_FRAME_START_2);

  // ----------------------------------------------------------
  // LENGTH - BIG ENDIAN
  // ----------------------------------------------------------

  uint16_t packetLength = (uint16_t)length;

  SerialLoRa.write(
      (uint8_t)((packetLength >> 8) & 0xFF)
  );

  SerialLoRa.write(
      (uint8_t)(packetLength & 0xFF)
  );

  // ----------------------------------------------------------
  // BINARY PAYLOAD
  // ----------------------------------------------------------

  SerialLoRa.write(data, length);

  SerialLoRa.flush();

  Serial.println("VOICE: Binary packet sent to UART");
}

// ============================================================
// CHECK VOICE PACKET
//
// Flutter VOICE packet starts with:
//
// 56 4F 49 43
// V  O  I  C
// ============================================================

bool isVoicePacket(
    const uint8_t *data,
    size_t length
)
{
  if (data == nullptr)
  {
    return false;
  }

  if (length < 4)
  {
    return false;
  }

  if (data[0] != VOICE_MAGIC_0)
  {
    return false;
  }

  if (data[1] != VOICE_MAGIC_1)
  {
    return false;
  }

  if (data[2] != VOICE_MAGIC_2)
  {
    return false;
  }

  if (data[3] != VOICE_MAGIC_3)
  {
    return false;
  }

  return true;
}

// ============================================================
// BLE SERVER CALLBACKS
// ============================================================

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("BLE PHONE CONNECTED");
    Serial.println("================================");
  }

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;

    Serial.println();
    Serial.println("================================");
    Serial.println("BLE PHONE DISCONNECTED");
    Serial.println("================================");

    delay(500);

    BLEDevice::startAdvertising();

    Serial.println("BLE ADVERTISING RESTARTED");
  }
};

// ============================================================
// BLE RX CALLBACKS
//
// RECEIVES BOTH:
//
// 1. NORMAL TEXT / LOCATION
// 2. BINARY VOICE PACKETS
// ============================================================

class MyRxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic)
  {
    // ESP32 Arduino core 3.3.8 returns Arduino String
    String value = characteristic->getValue();

    if (value.length() == 0)
    {
      return;
    }

    // IMPORTANT:
    // Check for binary VOIC packet BEFORE trim()
    // because voice data can contain 0x00 and other binary bytes.
    const uint8_t *data =
        reinterpret_cast<const uint8_t *>(value.c_str());

    size_t length = value.length();

    // --------------------------------------------------
    // VOICE PACKET
    // --------------------------------------------------
    if (isVoicePacket(data, length))
    {
      Serial.println();
      Serial.println("================================");
      Serial.println("VOICE PACKET RECEIVED FROM PHONE");
      Serial.println("================================");

      Serial.print("BLE Length: ");
      Serial.println(length);

      Serial.println("Forwarding binary packet to other ESP32...");

      sendBinaryToOtherESP32(data, length);

      Serial.println("VOICE PACKET FORWARDED");
      Serial.println("================================");

      return;
    }

    // --------------------------------------------------
    // NORMAL TEXT / LOCATION PACKET
    // --------------------------------------------------
    value.trim();

    if (value.length() == 0)
    {
      return;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("NORMAL BLE MESSAGE");
    Serial.println("================================");

    Serial.print("MESSAGE: ");
    Serial.println(value);

    Serial.println("================================");

    // Location packet
    if (value.startsWith("LOC,"))
    {
      Serial.println("LOCATION PACKET RECEIVED FROM PHONE");

      parseLocationPacket(value);
    }
    else
    {
      Serial.println("TEXT MESSAGE RECEIVED FROM PHONE");
    }

    // Forward normal text/location to other ESP32
    sendToOtherESP32(value);
  }
};

// ============================================================
// LOCATION PACKET PARSER
//
// Expected:
//
// LOC,<mobileDeviceId>,<latitude>,<longitude>,<timestamp>,<batteryLevel>
// ============================================================

void parseLocationPacket(String message)
{
  Serial.println("--------------------------------");
  Serial.println("LOCATION PACKET");
  Serial.println("--------------------------------");

  int index1 = message.indexOf(',');
  int index2 = message.indexOf(',', index1 + 1);
  int index3 = message.indexOf(',', index2 + 1);
  int index4 = message.indexOf(',', index3 + 1);
  int index5 = message.indexOf(',', index4 + 1);

  if (index1 < 0 ||
      index2 < 0 ||
      index3 < 0 ||
      index4 < 0 ||
      index5 < 0)
  {
    Serial.println("INVALID LOCATION PACKET");
    Serial.println("--------------------------------");
    return;
  }

  String deviceId =
      message.substring(
          index1 + 1,
          index2
      );

  String latitude =
      message.substring(
          index2 + 1,
          index3
      );

  String longitude =
      message.substring(
          index3 + 1,
          index4
      );

  String timestamp =
      message.substring(
          index4 + 1,
          index5
      );

  String battery =
      message.substring(
          index5 + 1
      );

  battery.trim();

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

  Serial.println("--------------------------------");
}

// ============================================================
// PROCESS NORMAL TEXT RECEIVED FROM UART
//
// THIS IS THE EXISTING TEXT / LOCATION PATH.
// ============================================================

void processTextFromUART(String message)
{
  message.trim();

  if (message.length() == 0)
  {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("TEXT PACKET RECEIVED FROM UART");
  Serial.println("================================");

  Serial.print("MESSAGE: ");
  Serial.println(message);

  Serial.println("================================");

  // ----------------------------------------------------------
  // LOCATION
  // ----------------------------------------------------------

  if (message.startsWith("LOC,"))
  {
    Serial.println(
        "LOCATION PACKET RECEIVED FROM OTHER ESP32"
    );

    parseLocationPacket(message);
  }
  else
  {
    Serial.println(
        "TEXT MESSAGE RECEIVED FROM OTHER ESP32"
    );
  }

  // ----------------------------------------------------------
  // FORWARD TO PHONE
  // ----------------------------------------------------------

  sendToPhone(message);
}

// ============================================================
// PROCESS BINARY DATA RECEIVED FROM UART
// ============================================================

void processBinaryFromUART(
    const uint8_t *data,
    size_t length
)
{
  if (data == nullptr || length == 0)
  {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("BINARY PACKET RECEIVED FROM UART");
  Serial.print("Length: ");
  Serial.println(length);
  Serial.println("================================");

  // ----------------------------------------------------------
  // Verify only the transport payload type.
  //
  // ESP32 does not process the audio.
  // ----------------------------------------------------------

  if (isVoicePacket(data, length))
  {
    Serial.println(
        "VOICE PACKET RECEIVED FROM OTHER ESP32"
    );

    Serial.println(
        "FORWARDING VOICE PACKET TO PHONE"
    );

    sendBinaryToPhone(
        data,
        length
    );
  }
  else
  {
    Serial.println(
        "UNKNOWN BINARY PACKET - IGNORED"
    );
  }
}

// ============================================================
// SINGLE UART DISPATCHER
//
// IMPORTANT:
//
// THIS IS THE ONLY FUNCTION THAT READS SerialLoRa.
//
// It handles:
//
//     NORMAL TEXT
//     LOCATION
//     BINARY VOICE
//
// ============================================================

void handleUART()
{
  while (SerialLoRa.available())
  {
    uint8_t incomingByte =
        (uint8_t)SerialLoRa.read();

    // ========================================================
    // IDLE
    // ========================================================

    if (uartMode == UART_IDLE)
    {
      // ------------------------------------------------------
      // Possible binary frame
      // ------------------------------------------------------

      if (incomingByte == UART_FRAME_START_1)
      {
        uartMode = UART_BINARY_LENGTH_1;

        binaryExpectedLength = 0;
        binaryReceivedLength = 0;

        continue;
      }

      // ------------------------------------------------------
      // Otherwise this is normal text
      // ------------------------------------------------------

      uartMode = UART_TEXT;

      if (incomingByte == '\n')
      {
        if (uartReceiveBuffer.length() > 0)
        {
          processTextFromUART(
              uartReceiveBuffer
          );

          uartReceiveBuffer = "";
        }

        uartMode = UART_IDLE;
      }
      else if (incomingByte != '\r')
      {
        uartReceiveBuffer +=
            (char)incomingByte;
      }

      continue;
    }

    // ========================================================
    // BINARY START BYTE 1 WAS RECEIVED
    //
    // We are waiting for 0x55.
    // ========================================================

    if (uartMode == UART_BINARY_LENGTH_1)
    {
      if (incomingByte == UART_FRAME_START_2)
      {
        uartMode = UART_BINARY_LENGTH_2;

        binaryExpectedLength = 0;
        binaryReceivedLength = 0;

        continue;
      }

      // ------------------------------------------------------
      // It was not a binary frame.
      //
      // Treat previous 0xAA as text/data and process current
      // byte normally.
      // ------------------------------------------------------

      uartReceiveBuffer +=
          (char)UART_FRAME_START_1;

      uartMode = UART_TEXT;

      if (incomingByte == '\n')
      {
        if (uartReceiveBuffer.length() > 0)
        {
          processTextFromUART(
              uartReceiveBuffer
          );

          uartReceiveBuffer = "";
        }

        uartMode = UART_IDLE;
      }
      else if (incomingByte != '\r')
      {
        uartReceiveBuffer +=
            (char)incomingByte;
      }

      continue;
    }

    // ========================================================
    // FIRST LENGTH BYTE
    // ========================================================

    if (uartMode == UART_BINARY_LENGTH_2)
    {
      binaryExpectedLength =
          ((uint16_t)incomingByte << 8);

      uartMode = UART_BINARY_PAYLOAD;

      continue;
    }

    // ========================================================
    // BINARY PAYLOAD
    //
    // Length-based.
    //
    // Therefore 0xAA / 0x55 inside the audio packet causes
    // NO PROBLEM.
    // ========================================================

    if (uartMode == UART_BINARY_PAYLOAD)
    {
      // ------------------------------------------------------
      // The second length byte is needed first.
      // ------------------------------------------------------

      if (binaryExpectedLength == 0)
      {
        binaryExpectedLength =
            incomingByte;

        if (binaryExpectedLength == 0)
        {
          Serial.println(
              "UART: Invalid binary length"
          );

          uartMode = UART_IDLE;
          continue;
        }

        if (binaryExpectedLength >
            MAX_BINARY_PACKET_SIZE)
        {
          Serial.println(
              "UART: Binary packet too large"
          );

          uartMode = UART_IDLE;
          binaryExpectedLength = 0;
          binaryReceivedLength = 0;

          continue;
        }

        binaryReceivedLength = 0;

        continue;
      }

      // ------------------------------------------------------
      // Store payload
      // ------------------------------------------------------

      if (binaryReceivedLength <
          MAX_BINARY_PACKET_SIZE)
      {
        binaryReceiveBuffer[
            binaryReceivedLength
        ] = incomingByte;

        binaryReceivedLength++;
      }

      // ------------------------------------------------------
      // Complete binary packet
      // ------------------------------------------------------

      if (binaryReceivedLength >=
          binaryExpectedLength)
      {
        processBinaryFromUART(
            binaryReceiveBuffer,
            binaryExpectedLength
        );

        binaryExpectedLength = 0;
        binaryReceivedLength = 0;

        uartMode = UART_IDLE;
      }

      continue;
    }

    // ========================================================
    // NORMAL TEXT
    // ========================================================

    if (uartMode == UART_TEXT)
    {
      if (incomingByte == '\n')
      {
        if (uartReceiveBuffer.length() > 0)
        {
          processTextFromUART(
              uartReceiveBuffer
          );

          uartReceiveBuffer = "";
        }

        uartMode = UART_IDLE;
      }
      else if (incomingByte != '\r')
      {
        uartReceiveBuffer +=
            (char)incomingByte;

        // ----------------------------------------------------
        // Safety against unlimited String growth.
        // ----------------------------------------------------

        if (uartReceiveBuffer.length() >
            512)
        {
          Serial.println(
              "UART: Text buffer overflow - reset"
          );

          uartReceiveBuffer = "";
          uartMode = UART_IDLE;
        }
      }

      continue;
    }
  }
}

// ============================================================
// SERIAL MONITOR HANDLER
//
// PC -> USB Serial -> ESP32
//
// This is kept separate from Serial2.
// ============================================================

void handleSerialMonitor()
{
  if (!Serial.available())
  {
    return;
  }

  String message =
      Serial.readStringUntil('\n');

  message.trim();

  if (message.length() == 0)
  {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("SERIAL MONITOR INPUT");
  Serial.println("================================");

  Serial.print("MESSAGE: ");
  Serial.println(message);

  Serial.println("================================");

  // ----------------------------------------------------------
  // Send serial monitor text to other ESP32
  // ----------------------------------------------------------

  sendToOtherESP32(message);
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  // ==========================================================
  // USB SERIAL
  // ==========================================================

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 LORA TRACKER NODE");
  Serial.println("================================");

  // ==========================================================
  // UART2
  //
  // RX = GPIO16
  // TX = GPIO17
  // ==========================================================

  SerialLoRa.begin(
      UART_BAUD_RATE,
      SERIAL_8N1,
      UART_RX_PIN,
      UART_TX_PIN
  );

  Serial.println(
      "UART2 initialized"
  );

  Serial.print("UART RX Pin: ");
  Serial.println(UART_RX_PIN);

  Serial.print("UART TX Pin: ");
  Serial.println(UART_TX_PIN);

  Serial.print("UART Baud: ");
  Serial.println(UART_BAUD_RATE);

  // ==========================================================
  // BLE INITIALIZATION
  // ==========================================================

  Serial.println(
      "Initializing BLE..."
  );

  BLEDevice::init(
      "ESP32_LORA_NODE3"
  );

  // ==========================================================
  // BLE SERVER
  // ==========================================================

  BLEServer *server =
      BLEDevice::createServer();

  server->setCallbacks(
      new MyServerCallbacks()
  );

  // ==========================================================
  // BLE SERVICE
  // ==========================================================

  BLEService *service =
      server->createService(
          SERVICE_UUID
      );

  // ==========================================================
  // TX CHARACTERISTIC
  //
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
  // RX CHARACTERISTIC
  //
  // PHONE -> ESP32
  //
  // Supports normal text AND binary voice packets.
  // ==========================================================

  rxCharacteristic =
      service->createCharacteristic(
          RX_CHARACTERISTIC,
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_WRITE_NR
      );

  rxCharacteristic->setCallbacks(
      new MyRxCallbacks()
  );

  // ==========================================================
  // START SERVICE
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

  advertising->setScanResponse(true);

  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  // ==========================================================
  // READY
  // ==========================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 READY");
  Serial.println("================================");

  Serial.println(
      "BLE Service Ready"
  );

  Serial.println(
      "BLE waiting for phone..."
  );

  Serial.println(
      "UART2 waiting for ESP32..."
  );

  Serial.println();
  Serial.println(
      "VOICE TRANSPORT ENABLED"
  );

  Serial.println(
      "TEXT TRANSPORT ENABLED"
  );

  Serial.println(
      "LOCATION TRANSPORT ENABLED"
  );

  Serial.println();
  Serial.println(
      "Phone <-> ESP32 <-> UART <-> ESP32 <-> Phone"
  );

  Serial.println(
      "================================"
  );
}

// ============================================================
// MAIN LOOP
//
// IMPORTANT:
//
// ONLY handleUART() reads SerialLoRa.
//
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // UART2 dispatcher
  // Handles BOTH text and binary.
  // ----------------------------------------------------------

  handleUART();

  // ----------------------------------------------------------
  // USB serial monitor
  // ----------------------------------------------------------

  handleSerialMonitor();

  // ----------------------------------------------------------
  // Small delay
  // ----------------------------------------------------------

  delay(10);
}