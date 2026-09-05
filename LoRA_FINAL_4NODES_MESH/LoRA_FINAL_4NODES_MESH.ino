#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <LoRa_E220.h>

// ============================================================
// BLE UUIDs — KEEP UNCHANGED
// ============================================================

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================================================
// BLE DEVICE NAME
//
// Your App scans devices using this name format.
//
// Node 1 = ESP32_LORA_NODE1
// Node 2 = ESP32_LORA_NODE2
// Node 3 = ESP32_LORA_NODE3
// Node 4 = ESP32_LORA_NODE4
// ============================================================

#define BLE_DEVICE_NAME "ESP32_LORA_NODE3"

// ============================================================
// E220 LORA PINS
//
// ESP32 GPIO16 <- E220 TX
// ESP32 GPIO17 -> E220 RX
//
// M0  = GPIO25
// M1  = GPIO26
// AUX = GPIO27
// ============================================================

#define LORA_RX_PIN 16
#define LORA_TX_PIN 17

#define LORA_M0_PIN 25
#define LORA_M1_PIN 26
#define LORA_AUX_PIN 27

#define LORA_BAUD_RATE 9600

// Your configured E220 channel
#define LORA_CHANNEL 18

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(
    &E220Serial,
    LORA_AUX_PIN,
    LORA_M0_PIN,
    LORA_M1_PIN
);

// ============================================================
// NODE ADDRESSES
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
// CURRENT NODE
//
// This code is for JESS = Node 2
// ============================================================

#define MY_NAME "JESS"

#define MY_ADDH JESS_ADDH
#define MY_ADDL JESS_ADDL

// ============================================================
// DESTINATION
//
// JESS -> THANU
// ============================================================

#define DEST_NAME "THANU"

#define DEST_ADDH THANU_ADDH
#define DEST_ADDL THANU_ADDL

// ============================================================
// BLE VARIABLES
// ============================================================

BLECharacteristic *txCharacteristic;
BLECharacteristic *rxCharacteristic;

bool deviceConnected = false;

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void sendToPhone(String message);

void sendToLoRa(String message);

void handleLoRa();

void processTextFromLoRa(String message);

void parseLocationPacket(String message);

// ============================================================
// SEND TEXT TO PHONE
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

    Serial.println();
    Serial.println("================================");
    Serial.println("LORA -> PHONE");
    Serial.println("================================");

    Serial.print("MESSAGE: ");
    Serial.println(message);

    Serial.println("================================");

    txCharacteristic->setValue(
        message.c_str()
    );

    txCharacteristic->notify();
}

// ============================================================
// SEND TEXT TO OTHER ESP32 THROUGH E220
//
// JESS -> THANU
// ============================================================

void sendToLoRa(String message)
{
    message.trim();

    if (message.length() == 0)
    {
        return;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("PHONE -> LORA");
    Serial.println("================================");

    Serial.print("SOURCE      : ");
    Serial.println(MY_NAME);

    Serial.print("DESTINATION : ");
    Serial.println(DEST_NAME);

    Serial.print("MESSAGE     : ");
    Serial.println(message);

    Serial.println("================================");

    ResponseStatus status =
        e220ttl.sendFixedMessage(
            DEST_ADDH,
            DEST_ADDL,
            LORA_CHANNEL,
            message
        );

    Serial.print("LoRa status: ");

    Serial.println(
        status.getResponseDescription()
    );
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

        Serial.println(
            "BLE ADVERTISING RESTARTED"
        );
    }
};

// ============================================================
// BLE RECEIVE CALLBACK
//
// PHONE -> ESP32
//
// Only TEXT is handled.
// ============================================================

class MyRxCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(
        BLECharacteristic *characteristic
    )
    {
        // ESP32 Arduino Core 3.x
        // getValue() returns Arduino String

        String value =
            characteristic->getValue();

        if (value.length() == 0)
        {
            return;
        }

        value.trim();

        if (value.length() == 0)
        {
            return;
        }

        Serial.println();
        Serial.println("================================");
        Serial.println("TEXT RECEIVED FROM PHONE");
        Serial.println("================================");

        Serial.print("MESSAGE: ");
        Serial.println(value);

        Serial.println("================================");

        // ----------------------------------------------------
        // Check whether this is a location packet
        // ----------------------------------------------------

        if (value.startsWith("LOC,"))
        {
            Serial.println(
                "LOCATION PACKET RECEIVED"
            );

            parseLocationPacket(value);
        }
        else
        {
            Serial.println(
                "TEXT MESSAGE RECEIVED"
            );
        }

        // ----------------------------------------------------
        // Send message through LoRa
        // ----------------------------------------------------

        sendToLoRa(value);
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
    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("LOCATION PACKET");
    Serial.println("--------------------------------");

    int index1 =
        message.indexOf(',');

    int index2 =
        message.indexOf(
            ',',
            index1 + 1
        );

    int index3 =
        message.indexOf(
            ',',
            index2 + 1
        );

    int index4 =
        message.indexOf(
            ',',
            index3 + 1
        );

    int index5 =
        message.indexOf(
            ',',
            index4 + 1
        );

    if (
        index1 < 0 ||
        index2 < 0 ||
        index3 < 0 ||
        index4 < 0 ||
        index5 < 0
    )
    {
        Serial.println(
            "INVALID LOCATION PACKET"
        );

        Serial.println(
            "--------------------------------"
        );

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

    Serial.println(
        "--------------------------------"
    );
}

// ============================================================
// PROCESS TEXT RECEIVED FROM LORA
//
// Other ESP32 -> LoRa -> This ESP32
// ============================================================

void processTextFromLoRa(String message)
{
    message.trim();

    if (message.length() == 0)
    {
        return;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("TEXT RECEIVED FROM LORA");
    Serial.println("================================");

    Serial.print("MESSAGE: ");
    Serial.println(message);

    Serial.println("================================");

    // --------------------------------------------------------
    // Check location
    // --------------------------------------------------------

    if (message.startsWith("LOC,"))
    {
        Serial.println(
            "LOCATION PACKET FROM OTHER ESP32"
        );

        parseLocationPacket(message);
    }
    else
    {
        Serial.println(
            "TEXT MESSAGE FROM OTHER ESP32"
        );
    }

    // --------------------------------------------------------
    // Forward to phone
    // --------------------------------------------------------

    sendToPhone(message);
}

// ============================================================
// HANDLE E220 LORA
//
// This is the ONLY function that reads E220.
// ============================================================

void handleLoRa()
{
    if (!E220Serial.available())
    {
        return;
    }

    ResponseContainer response =
        e220ttl.receiveMessage();

    if (
        response.status.code != 1
    )
    {
        Serial.print(
            "LoRa receive error: "
        );

        Serial.println(
            response.status
                .getResponseDescription()
        );

        return;
    }

    String received =
        response.data;

    if (received.length() == 0)
    {
        return;
    }

    // --------------------------------------------------------
    // Everything received through this version is TEXT.
    // --------------------------------------------------------

    processTextFromLoRa(
        received
    );
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    // ========================================================
    // USB SERIAL
    // ========================================================

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println();

    Serial.println("================================");
    Serial.println("ESP32 LORA TRACKER");
    Serial.println("================================");

    Serial.print("Node Name : ");
    Serial.println(MY_NAME);

    Serial.print("BLE Name  : ");
    Serial.println(BLE_DEVICE_NAME);

    // ========================================================
    // E220 PINS
    // ========================================================

    pinMode(
        LORA_M0_PIN,
        OUTPUT
    );

    pinMode(
        LORA_M1_PIN,
        OUTPUT
    );

    pinMode(
        LORA_AUX_PIN,
        INPUT
    );

    // ========================================================
    // E220 NORMAL MODE
    //
    // M0 = LOW
    // M1 = LOW
    // ========================================================

    digitalWrite(
        LORA_M0_PIN,
        LOW
    );

    digitalWrite(
        LORA_M1_PIN,
        LOW
    );

    delay(100);

    // ========================================================
    // E220 UART
    // ========================================================

    E220Serial.begin(
        LORA_BAUD_RATE,
        SERIAL_8N1,
        LORA_RX_PIN,
        LORA_TX_PIN
    );

    // ========================================================
    // START E220
    //
    // No configuration is performed here.
    // ========================================================

    e220ttl.begin();

    Serial.println();
    Serial.println(
        "E220 LoRa initialized"
    );

    Serial.print(
        "LoRa RX GPIO : "
    );

    Serial.println(
        LORA_RX_PIN
    );

    Serial.print(
        "LoRa TX GPIO : "
    );

    Serial.println(
        LORA_TX_PIN
    );

    Serial.print(
        "M0 GPIO      : "
    );

    Serial.println(
        LORA_M0_PIN
    );

    Serial.print(
        "M1 GPIO      : "
    );

    Serial.println(
        LORA_M1_PIN
    );

    Serial.print(
        "AUX GPIO     : "
    );

    Serial.println(
        LORA_AUX_PIN
    );

    Serial.print(
        "Channel      : "
    );

    Serial.println(
        LORA_CHANNEL
    );

    // ========================================================
    // BLE INITIALIZATION
    // ========================================================

    Serial.println();
    Serial.println(
        "Initializing BLE..."
    );

    BLEDevice::init(
        BLE_DEVICE_NAME
    );

    // ========================================================
    // BLE SERVER
    // ========================================================

    BLEServer *server =
        BLEDevice::createServer();

    server->setCallbacks(
        new MyServerCallbacks()
    );

    // ========================================================
    // BLE SERVICE
    // ========================================================

    BLEService *service =
        server->createService(
            SERVICE_UUID
        );

    // ========================================================
    // TX CHARACTERISTIC
    //
    // ESP32 -> PHONE
    // ========================================================

    txCharacteristic =
        service->createCharacteristic(
            TX_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_NOTIFY
        );

    txCharacteristic->addDescriptor(
        new BLE2902()
    );

    // ========================================================
    // RX CHARACTERISTIC
    //
    // PHONE -> ESP32
    // ========================================================

    rxCharacteristic =
        service->createCharacteristic(
            RX_CHARACTERISTIC,
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR
        );

    rxCharacteristic->setCallbacks(
        new MyRxCallbacks()
    );

    // ========================================================
    // START BLE SERVICE
    // ========================================================

    service->start();

    // ========================================================
    // BLE ADVERTISING
    // ========================================================

    BLEAdvertising *advertising =
        BLEDevice::getAdvertising();

    advertising->addServiceUUID(
        SERVICE_UUID
    );

    advertising->setScanResponse(
        true
    );

    advertising->setMinPreferred(
        0x06
    );

    advertising->setMinPreferred(
        0x12
    );

    BLEDevice::startAdvertising();

    // ========================================================
    // READY
    // ========================================================

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 READY");
    Serial.println("================================");

    Serial.print(
        "BLE Device: "
    );

    Serial.println(
        BLE_DEVICE_NAME
    );

    Serial.print(
        "LoRa Node: "
    );

    Serial.println(
        MY_NAME
    );

    Serial.print(
        "LoRa Destination: "
    );

    Serial.println(
        DEST_NAME
    );

    Serial.println();
    Serial.println(
        "TEXT TRANSPORT ENABLED"
    );

    Serial.println(
        "LOCATION TRANSPORT ENABLED"
    );

    Serial.println();

    Serial.println(
        "PHONE"
    );

    Serial.println(
        "  |"
    );

    Serial.println(
        "  | BLE"
    );

    Serial.println(
        "  v"
    );

    Serial.println(
        "ESP32 JESS"
    );

    Serial.println(
        "  |"
    );

    Serial.println(
        "  | E220 LoRa"
    );

    Serial.println(
        "  v"
    );

    Serial.println(
        "ESP32 THANU"
    );

    Serial.println(
        "  |"
    );

    Serial.println(
        "  | BLE"
    );

    Serial.println(
        "  v"
    );

    Serial.println(
        "PHONE"
    );

    Serial.println(
        "================================"
    );
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // Receive LoRa
    handleLoRa();

    delay(5);
}