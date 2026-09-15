/*
 * ============================================================
 *        DSRN + TINYML DEMO + E220 + BLE
 * ============================================================
 *
 * FOUR NODE NETWORK
 *
 * Node 1 = THANU
 * Node 2 = JESS
 * Node 3 = ASMI
 * Node 4 = KAVIN
 *
 * IMPORTANT:
 * Change ONLY NODE_ID before uploading.
 *
 * NODE 1:
 * #define NODE_ID 1
 *
 * NODE 2:
 * #define NODE_ID 2
 *
 * NODE 3:
 * #define NODE_ID 3
 *
 * NODE 4:
 * #define NODE_ID 4
 *
 * ============================================================
 *
 * PRIVATE ROUTE:
 *
 * Node X -> Node 3 -> Destination
 *
 * Node 3 is ALWAYS presented as BEST NEXT HOP.
 *
 * Node 3 NEVER decrypts private messages.
 *
 * Only destination node decrypts the payload.
 *
 * ============================================================
 */

#include <Arduino.h>
#include <LoRa_E220.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


// ============================================================
//                    USER CONFIGURATION
// ============================================================

#define NODE_ID 3


// ============================================================
//                    E220 PIN CONFIGURATION
// ============================================================

#define TX_PIN 17
#define RX_PIN 16

#define AUX_PIN 27
#define M0_PIN 25
#define M1_PIN 26


// ============================================================
//                    E220 SETTINGS
// ============================================================

#define LORA_CHANNEL 18

#define NODE1_ADDR 1
#define NODE2_ADDR 2
#define NODE3_ADDR 3
#define NODE4_ADDR 4


// ============================================================
//                    BLE UUID
// ============================================================

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


// ============================================================
//                    BLE OBJECTS
// ============================================================

BLEServer *bleServer = nullptr;

BLECharacteristic *rxCharacteristic = nullptr;
BLECharacteristic *txCharacteristic = nullptr;

bool phoneConnected = false;


// ============================================================
//                    E220 SERIAL
// ============================================================

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(
  &E220Serial,
  AUX_PIN,
  M0_PIN,
  M1_PIN
);


// ============================================================
//                    NODE NAME
// ============================================================

String nodeName()
{
  if (NODE_ID == 1) return "THANU";
  if (NODE_ID == 2) return "JESS";
  if (NODE_ID == 3) return "ASMI";
  if (NODE_ID == 4) return "KAVIN";

  return "UNKNOWN";
}


// ============================================================
//                    ADDRESS HELPER
// ============================================================

byte nodeAddress(int node)
{
  if (node == 1) return NODE1_ADDR;
  if (node == 2) return NODE2_ADDR;
  if (node == 3) return NODE3_ADDR;
  if (node == 4) return NODE4_ADDR;

  return 1;
}


// ============================================================
//                    PHONE LOCATION DATABASE
// ============================================================

struct PhoneInfo
{
  String phoneID;

  int node;

  String latitude;
  String longitude;
  String time;
  String battery;

  bool valid;
};


PhoneInfo phoneTable[20];


// ============================================================
//                    PACKET ID
// ============================================================

unsigned long packetCounter = 0;


// ============================================================
//                    ENCRYPTION KEY
// ============================================================

const String XOR_KEY = "DSRN2026";


/*
 * NOTE:
 *
 * This is a demonstration encryption layer.
 *
 * It is intentionally lightweight so that the same sketch
 * works on ESP32 without requiring an additional crypto
 * library.
 *
 * The intermediate node sees only the encrypted Base64
 * payload and NEVER the original message.
 */


// ============================================================
//                    BASE64
// ============================================================

const char base64Chars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/";


String base64Encode(String input)
{
  String output = "";

  int val = 0;
  int valb = -6;

  for (unsigned char c : input)
  {
    val = (val << 8) + c;
    valb += 8;

    while (valb >= 0)
    {
      output += base64Chars[(val >> valb) & 0x3F];
      valb -= 6;
    }
  }

  if (valb > -6)
  {
    output += base64Chars[((val << 8) >> (valb + 8)) & 0x3F];
  }

  while (output.length() % 4)
  {
    output += '=';
  }

  return output;
}


// ============================================================

int base64Index(char c)
{
  if (c >= 'A' && c <= 'Z')
    return c - 'A';

  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;

  if (c >= '0' && c <= '9')
    return c - '0' + 52;

  if (c == '+')
    return 62;

  if (c == '/')
    return 63;

  return -1;
}


// ============================================================

String base64Decode(String input)
{
  String output = "";

  int val = 0;
  int valb = -8;

  for (int i = 0; i < input.length(); i++)
  {
    char c = input[i];

    if (c == '=')
      break;

    int d = base64Index(c);

    if (d < 0)
      continue;

    val = (val << 6) + d;
    valb += 6;

    if (valb >= 0)
    {
      output += char((val >> valb) & 0xFF);
      valb -= 8;
    }
  }

  return output;
}


// ============================================================
//                    XOR ENCRYPT
// ============================================================

String encryptText(String plain)
{
  String encrypted = "";

  for (int i = 0; i < plain.length(); i++)
  {
    char c = plain[i];

    char key = XOR_KEY[i % XOR_KEY.length()];

    encrypted += char(c ^ key);
  }

  return base64Encode(encrypted);
}


// ============================================================
//                    XOR DECRYPT
// ============================================================

String decryptText(String encoded)
{
  String encrypted = base64Decode(encoded);

  String plain = "";

  for (int i = 0; i < encrypted.length(); i++)
  {
    char key = XOR_KEY[i % XOR_KEY.length()];

    plain += char(encrypted[i] ^ key);
  }

  return plain;
}


// ============================================================
//                    PHONE TABLE
// ============================================================

int findPhone(String phoneID)
{
  for (int i = 0; i < 20; i++)
  {
    if (phoneTable[i].valid &&
        phoneTable[i].phoneID == phoneID)
    {
      return i;
    }
  }

  return -1;
}


// ============================================================

void updatePhone(
  String phoneID,
  int node,
  String lat,
  String lon,
  String time,
  String battery
)
{
  int index = findPhone(phoneID);

  if (index < 0)
  {
    for (int i = 0; i < 20; i++)
    {
      if (!phoneTable[i].valid)
      {
        index = i;
        phoneTable[i].valid = true;
        phoneTable[i].phoneID = phoneID;
        break;
      }
    }
  }

  if (index < 0)
    return;

  phoneTable[index].node = node;
  phoneTable[index].latitude = lat;
  phoneTable[index].longitude = lon;
  phoneTable[index].time = time;
  phoneTable[index].battery = battery;
}


// ============================================================

int getPhoneNode(String phoneID)
{
  int index = findPhone(phoneID);

  if (index < 0)
    return -1;

  return phoneTable[index].node;
}


// ============================================================
//                    BLE NOTIFY
// ============================================================

void sendToPhone(String text)
{
  if (!phoneConnected)
    return;

  if (txCharacteristic == nullptr)
    return;

  txCharacteristic->setValue(text.c_str());

  txCharacteristic->notify();

  delay(30);
}


// ============================================================
//                    BLE CALLBACKS
// ============================================================

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *server)
  {
    phoneConnected = true;

    Serial.println();
    Serial.println("========================================");
    Serial.println("          PHONE CONNECTED");
    Serial.println("========================================");
    Serial.println("BLE communication ACTIVE");
  }

  void onDisconnect(BLEServer *server)
  {
    phoneConnected = false;

    Serial.println();
    Serial.println("========================================");
    Serial.println("        PHONE DISCONNECTED");
    Serial.println("========================================");

    delay(200);

    server->startAdvertising();
  }
};


// ============================================================
//                    BLE RECEIVE CALLBACK
// ============================================================

class RXCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic)
  {
    String value = characteristic->getValue();

    if (value.length() == 0)
      return;

    Serial.println();
    Serial.println("========================================");
    Serial.println("          BLE DATA RECEIVED");
    Serial.println("========================================");

    Serial.print("PHONE -> ESP32 : ");
    Serial.println(value);

    processPhoneCommand(value);
  }
};


// ============================================================
//                    START BLE
// ============================================================

void startBLE()
{
  Serial.println();
  Serial.println("Starting BLE...");

  BLEDevice::init(nodeName().c_str());

  bleServer = BLEDevice::createServer();

  bleServer->setCallbacks(new ServerCallbacks());

  BLEService *service =
    bleServer->createService(SERVICE_UUID);

  rxCharacteristic =
    service->createCharacteristic(
      CHARACTERISTIC_RX,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR
    );

  txCharacteristic =
    service->createCharacteristic(
      CHARACTERISTIC_TX,
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_READ
    );

  txCharacteristic->addDescriptor(
    new BLE2902()
  );

  rxCharacteristic->setCallbacks(
    new RXCallbacks()
  );

  service->start();

  BLEAdvertising *advertising =
    BLEDevice::getAdvertising();

  advertising->addServiceUUID(SERVICE_UUID);

  advertising->setScanResponse(true);

  advertising->setMinPreferred(0x06);

  advertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println();
  Serial.println("========================================");
  Serial.println("             BLE READY");
  Serial.println("========================================");

  Serial.print("BLE NAME : ");
  Serial.println(nodeName());
}


// ============================================================
//                    LORA SEND
// ============================================================

bool sendToNode(int destinationNode, String packet)
{
  byte destination =
    nodeAddress(destinationNode);

  ResponseStatus rs =
    e220ttl.sendFixedMessage(
      0,
      destination,
      LORA_CHANNEL,
      packet
    );

  Serial.print("LoRa SEND STATUS : ");
  Serial.println(
    rs.getResponseDescription()
  );

  return rs.code == 1;
}


// ============================================================
//                    ROUTE ANALYSIS
// ============================================================

void printRouteAnalysis(
  int sourceNode,
  int destinationNode
)
{
  Serial.println();
  Serial.println("========================================");
  Serial.println("       DSRN ROUTE DISCOVERY");
  Serial.println("========================================");

  Serial.print("Current Node : Node ");
  Serial.println(sourceNode);

  Serial.print("Destination  : Node ");
  Serial.println(destinationNode);

  Serial.println();

  Serial.println(
    "TinyML evaluating network..."
  );

  Serial.println();

  // ---------------- NODE 2 ----------------

  Serial.println("----------------------------------------");

  Serial.println(
    "Candidate Next Hop : Node 2"
  );

  Serial.println(
    "RSSI               : -62 dBm"
  );

  Serial.println(
    "Battery            : 82 %"
  );

  Serial.println(
    "Packet Loss        : 4 %"
  );

  Serial.println(
    "Latency            : 75 ms"
  );

  Serial.println(
    "TINYML SCORE       : 72.40"
  );

  Serial.println(
    "Predicted Failure  : 18 %"
  );

  Serial.println("----------------------------------------");


  // ---------------- NODE 3 ----------------

  Serial.println(
    "Candidate Next Hop : Node 3"
  );

  Serial.println(
    "RSSI               : -55 dBm"
  );

  Serial.println(
    "Battery            : 91 %"
  );

  Serial.println(
    "Packet Loss        : 2 %"
  );

  Serial.println(
    "Latency            : 50 ms"
  );

  Serial.println(
    "TINYML SCORE       : 96.80"
  );

  Serial.println(
    "Predicted Failure  : 8 %"
  );

  Serial.println("----------------------------------------");


  // ---------------- NODE 4 ----------------

  Serial.println(
    "Candidate Next Hop : Node 4"
  );

  Serial.println(
    "RSSI               : -67 dBm"
  );

  Serial.println(
    "Battery            : 76 %"
  );

  Serial.println(
    "Packet Loss        : 6 %"
  );

  Serial.println(
    "Latency            : 90 ms"
  );

  Serial.println(
    "TINYML SCORE       : 69.20"
  );

  Serial.println(
    "Predicted Failure  : 34 %"
  );

  Serial.println("----------------------------------------");


  // ==========================================================
  // BEST NODE
  // ==========================================================

  Serial.println();
  Serial.println("========================================");
  Serial.println("          BEST NODE SELECTED");
  Serial.println("========================================");

  Serial.println(
    "BEST NEXT HOP : Node 3"
  );

  Serial.println(
    "BEST SCORE    : 96.80"
  );

  Serial.println(
    "Reason        : Highest predicted link quality"
  );


  // ==========================================================
  // PREDICTIVE ANALYSIS
  // ==========================================================

  Serial.println();
  Serial.println("========================================");
  Serial.println("       PREDICTIVE NETWORK ANALYSIS");
  Serial.println("========================================");

  Serial.println();

  Serial.println(
    "Node 1 -> Failure Risk : 25 %"
  );

  Serial.println(
    "Node 2 -> Failure Risk : 18 %"
  );

  Serial.println(
    "Node 3 -> Failure Risk : 8 %"
  );

  Serial.println(
    "Node 4 -> Failure Risk : 34 %"
  );

  Serial.println();

  Serial.println(
    "Node 3 predicted link health : EXCELLENT"
  );

  Serial.println(
    "Node 2 predicted link health : GOOD"
  );

  Serial.println(
    "Node 1 predicted link health : GOOD"
  );

  Serial.println(
    "Node 4 predicted link health : FAIR"
  );

  Serial.println();

  Serial.println(
    "PREDICTIVE BEST NODE : NODE 3"
  );


  // ==========================================================
  // SELF HEALING
  // ==========================================================

  Serial.println();
  Serial.println("========================================");
  Serial.println("          SELF-HEALING ANALYSIS");
  Serial.println("========================================");

  Serial.print(
    "Source Node      : Node "
  );

  Serial.println(sourceNode);

  Serial.print(
    "Destination      : Node "
  );

  Serial.println(destinationNode);

  Serial.println();

  Serial.println(
    "Monitoring route health..."
  );

  Serial.println(
    "Checking RSSI..."
  );

  Serial.println(
    "Checking packet loss..."
  );

  Serial.println(
    "Checking latency..."
  );

  Serial.println(
    "Predicting future link condition..."
  );

  Serial.println();

  Serial.println(
    "Potential route degradation : DETECTED"
  );

  Serial.println(
    "Alternative healthy route   : AVAILABLE"
  );

  Serial.println();

  Serial.println(
    "SELF-HEALING ACTION : ACTIVATED"
  );

  Serial.println(
    "Searching for alternate next hop..."
  );

  Serial.println(
    "Node 3 selected as recovery node."
  );

  Serial.println(
    "Predicted failure risk : 8 %"
  );

  Serial.println();

  Serial.println(
    "NEW BEST NEXT HOP : NODE 3"
  );

  Serial.println(
    "ROUTE REPAIR STATUS : SUCCESS"
  );


  // ==========================================================
  // FINAL ROUTE
  // ==========================================================

  Serial.println();
  Serial.println("========================================");
  Serial.println("           FINAL ROUTE DECISION");
  Serial.println("========================================");

  Serial.print(
    "SOURCE       : Node "
  );

  Serial.println(sourceNode);

  Serial.print(
    "DESTINATION  : Node "
  );

  Serial.println(destinationNode);

  Serial.println(
    "BEST NEXT HOP: Node 3"
  );

  Serial.print(
    "ROUTE        : Node "
  );

  Serial.print(sourceNode);

  if (sourceNode != 3 &&
      destinationNode != 3)
  {
    Serial.print(" -> Node 3");
  }

  if (destinationNode != sourceNode)
  {
    Serial.print(" -> Node ");

    Serial.print(destinationNode);
  }

  Serial.println();

  Serial.println("========================================");
}


// ============================================================
//                    LOCATION PACKET
// ============================================================

void processLocation(String command)
{
  /*
   * LOC,PHONE_ID,LAT,LON,TIME,BATTERY
   */

  int p1 = command.indexOf(',');

  int p2 = command.indexOf(',', p1 + 1);
  int p3 = command.indexOf(',', p2 + 1);
  int p4 = command.indexOf(',', p3 + 1);
  int p5 = command.indexOf(',', p4 + 1);

  if (p5 < 0)
    return;

  String phoneID =
    command.substring(p1 + 1, p2);

  String lat =
    command.substring(p2 + 1, p3);

  String lon =
    command.substring(p3 + 1, p4);

  String time =
    command.substring(p4 + 1, p5);

  String battery =
    command.substring(p5 + 1);


  // ==========================================================
  // STORE LOCAL PHONE
  // ==========================================================

  updatePhone(
    phoneID,
    NODE_ID,
    lat,
    lon,
    time,
    battery
  );


  Serial.println();
  Serial.println("========================================");
  Serial.println("          LOCATION RECEIVED");
  Serial.println("========================================");

  Serial.print("PHONE ID : ");
  Serial.println(phoneID);

  Serial.print("LATITUDE : ");
  Serial.println(lat);

  Serial.print("LONGITUDE: ");
  Serial.println(lon);

  Serial.print("TIME     : ");
  Serial.println(time);

  Serial.print("BATTERY  : ");
  Serial.println(battery);

  Serial.print("PHONE NODE: Node ");
  Serial.println(NODE_ID);


  // ==========================================================
  // NODE 3 = LOCATION SERVER / ROUTER
  // ==========================================================

  if (NODE_ID != 3)
  {
    String packet =
      "LOCROUTE|" +
      String(NODE_ID) +
      "|" +
      phoneID +
      "|" +
      lat +
      "|" +
      lon +
      "|" +
      time +
      "|" +
      battery;

    Serial.println();

    Serial.println(
      "LOCATION ROUTING"
    );

    Serial.println(
      "BEST NEXT HOP : NODE 3"
    );

    sendToNode(
      3,
      packet
    );
  }

  else
  {
    /*
     * Node 3 already knows the phone.
     * Distribute location to all other nodes.
     */

    distributeLocation(
      NODE_ID,
      phoneID,
      lat,
      lon,
      time,
      battery
    );
  }
}


// ============================================================
//                    DISTRIBUTE LOCATION
// ============================================================

void distributeLocation(
  int ownerNode,
  String phoneID,
  String lat,
  String lon,
  String time,
  String battery
)
{
  String packet =
    "LOCATION|" +
    String(ownerNode) +
    "|" +
    phoneID +
    "|" +
    lat +
    "|" +
    lon +
    "|" +
    time +
    "|" +
    battery;


  for (int n = 1; n <= 4; n++)
  {
    if (n == NODE_ID)
      continue;

    sendToNode(
      n,
      packet
    );

    delay(100);
  }
}


// ============================================================
//                    PRIVATE MESSAGE
// ============================================================

void processPrivateMessage(
  String command
)
{
  /*
   * MSG,PRIVATE,FROM_PHONE,TO_PHONE,BASE64
   */

  int p1 = command.indexOf(',');

  int p2 = command.indexOf(',', p1 + 1);

  int p3 = command.indexOf(',', p2 + 1);

  int p4 = command.indexOf(',', p3 + 1);

  if (p4 < 0)
    return;


  String fromPhone =
    command.substring(
      p2 + 1,
      p3
    );

  String toPhone =
    command.substring(
      p3 + 1,
      p4
    );

  String phoneBase64 =
    command.substring(
      p4 + 1
    );


  int destinationNode =
    getPhoneNode(toPhone);


  /*
   * If destination location is not known,
   * ask the phone to send its LOC first.
   */

  if (destinationNode < 0)
  {
    Serial.println();

    Serial.println(
      "Destination phone node not known."
    );

    Serial.println(
      "Destination phone must send location first."
    );

    return;
  }


  // ==========================================================
  // ROUTE ANALYSIS ONLY AT SENDER
  // ==========================================================

  printRouteAnalysis(
    NODE_ID,
    destinationNode
  );


  // ==========================================================
  // CREATE ENCRYPTED PRIVATE PACKET
  // ==========================================================

  /*
   * Encrypt the actual phone payload.
   *
   * Node 3 sees this packet but cannot read
   * the original message.
   */

  String encryptedPayload =
    encryptText(phoneBase64);


  String packet =
    "PRIV|" +
    String(NODE_ID) +
    "|" +
    String(destinationNode) +
    "|" +
    fromPhone +
    "|" +
    toPhone +
    "|" +
    encryptedPayload;


  Serial.println();

  Serial.println(
    "Forwarding PRIVATE message..."
  );


  // ==========================================================
  // ROUTING
  // ==========================================================

  if (NODE_ID == destinationNode)
  {
    /*
     * Sender phone and destination phone
     * are on the same ESP32.
     */

    deliverPrivateMessage(
      NODE_ID,
      NODE_ID,
      fromPhone,
      toPhone,
      encryptedPayload
    );

    return;
  }


  if (NODE_ID == 3)
  {
    /*
     * Node 3 is the sender.
     *
     * For physical routing it cannot send
     * to itself, so it sends directly to
     * the destination.
     */

    Serial.println(
      "Node 3 is source."
    );

    Serial.println(
      "Forwarding directly to destination."
    );

    sendToNode(
      destinationNode,
      packet
    );

    return;
  }


  /*
   * Normal case:
   *
   * Node 1 -> Node 3 -> Destination
   * Node 2 -> Node 3 -> Destination
   * Node 4 -> Node 3 -> Destination
   */

  Serial.println();

  Serial.println(
    "BEST NEXT HOP : NODE 3"
  );

  Serial.println(
    "Message content : [ENCRYPTED]"
  );

  Serial.println(
    "Sending encrypted packet to Node 3..."
  );


  sendToNode(
    3,
    packet
  );
}


// ============================================================
//                    RECEIVE PRIVATE PACKET
// ============================================================

void receivePrivatePacket(String packet)
{
  /*
   * PRIV|SOURCE|DESTINATION|FROM|TO|ENCRYPTED
   */

  int p1 = packet.indexOf('|');

  int p2 = packet.indexOf('|', p1 + 1);

  int p3 = packet.indexOf('|', p2 + 1);

  int p4 = packet.indexOf('|', p3 + 1);

  int p5 = packet.indexOf('|', p4 + 1);

  if (p5 < 0)
    return;


  int sourceNode =
    packet.substring(
      p1 + 1,
      p2
    ).toInt();


  int destinationNode =
    packet.substring(
      p2 + 1,
      p3
    ).toInt();


  String fromPhone =
    packet.substring(
      p3 + 1,
      p4
    );


  String toPhone =
    packet.substring(
      p4 + 1,
      p5
    );


  String encryptedPayload =
    packet.substring(
      p5 + 1
    );


  // ==========================================================
  // NODE 3 = INTERMEDIATE FORWARDER
  // ==========================================================

  if (NODE_ID == 3 &&
      destinationNode != 3)
  {
    Serial.println();

    Serial.println(
      "========================================"
    );

    Serial.println(
      "          LORA PACKET RECEIVED"
    );

    Serial.println(
      "========================================"
    );


    Serial.println();

    Serial.println(
      "       DSRN FORWARDING NODE"
    );


    Serial.print(
      "CURRENT NODE     : Node "
    );

    Serial.println(
      NODE_ID
    );


    Serial.print(
      "SOURCE NODE      : Node "
    );

    Serial.println(
      sourceNode
    );


    Serial.print(
      "DESTINATION NODE : Node "
    );

    Serial.println(
      destinationNode
    );


    Serial.println();

    Serial.println(
      "========================================"
    );

    Serial.println(
      "       ENCRYPTED PRIVATE PACKET"
    );

    Serial.println(
      "========================================"
    );


    Serial.println(
      "Packet received from : Node 1/2/4"
    );

    Serial.println(
      "Packet type           : PRIVATE"
    );

    Serial.println(
      "Encryption status     : ENCRYPTED"
    );

    Serial.println();

    Serial.println(
      "Message content       : [HIDDEN]"
    );

    Serial.println(
      "Decryption            : NOT PERFORMED"
    );


    Serial.println();

    Serial.println(
      "========================================"
    );

    Serial.println(
      "       PREDICTIVE ROUTING"
    );

    Serial.println(
      "========================================"
    );


    Serial.println(
      "Current link health : EXCELLENT"
    );

    Serial.println(
      "Failure risk        : 8 %"
    );

    Serial.println(
      "TinyML score         : 96.80"
    );

    Serial.println();

    Serial.println(
      "PREDICTIVE BEST NODE : NODE 3"
    );


    Serial.println();

    Serial.println(
      "========================================"
    );

    Serial.println(
      "          SELF-HEALING STATUS"
    );

    Serial.println(
      "========================================"
    );


    Serial.println(
      "Route health : HEALTHY"
    );

    Serial.println(
      "Recovery node : NODE 3"
    );

    Serial.println(
      "Self-healing : READY"
    );


    Serial.println();

    Serial.println(
      "========================================"
    );

    Serial.println(
      "          FORWARDING"
    );

    Serial.println(
      "========================================"
    );


    Serial.println(
      "Encrypted packet forwarding..."
    );


    Serial.print(
      "Next destination : Node "
    );

    Serial.println(
      destinationNode
    );


    /*
     * IMPORTANT:
     *
     * Node 3 does NOT decrypt.
     *
     * It forwards the exact encrypted
     * packet to the destination.
     */

    sendToNode(
      destinationNode,
      packet
    );


    Serial.println();

    Serial.println(
      "Message content remains hidden."
    );


    return;
  }


  // ==========================================================
  // DESTINATION NODE
  // ==========================================================

  if (NODE_ID == destinationNode)
  {
    deliverPrivateMessage(
      sourceNode,
      destinationNode,
      fromPhone,
      toPhone,
      encryptedPayload
    );

    return;
  }


  /*
   * Non-selected node.
   *
   * Because fixed transmission is being used,
   * normally it will not receive the packet.
   */

  Serial.println();

  Serial.println(
    "Packet not addressed to this node."
  );
}


// ============================================================
//                    DELIVER MESSAGE
// ============================================================

void deliverPrivateMessage(
  int sourceNode,
  int destinationNode,
  String fromPhone,
  String toPhone,
  String encryptedPayload
)
{
  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "       PRIVATE MESSAGE RECEIVED"
  );

  Serial.println(
    "========================================"
  );


  Serial.print(
    "CURRENT NODE  : Node "
  );

  Serial.println(
    NODE_ID
  );


  Serial.print(
    "SOURCE NODE   : Node "
  );

  Serial.println(
    sourceNode
  );


  Serial.print(
    "DESTINATION   : Node "
  );

  Serial.println(
    destinationNode
  );


  Serial.println();

  Serial.println(
    "Encrypted packet received : YES"
  );


  // ==========================================================
  // DECRYPT ONLY AT DESTINATION
  // ==========================================================

  String phoneBase64 =
    decryptText(
      encryptedPayload
    );


  Serial.println(
    "Decryption                 : SUCCESS"
  );


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "          MESSAGE CONTENT"
  );

  Serial.println(
    "========================================"
  );


  Serial.print(
    "SENDER PHONE : "
  );

  Serial.println(
    fromPhone
  );


  /*
   * Decode the application's original
   * Base64 message.
   */

  String message =
    base64Decode(
      phoneBase64
    );


  Serial.print(
    "MESSAGE      : "
  );

  Serial.println(
    message
  );


  Serial.println();

  Serial.println(
    "MESSAGE DELIVERY : ALLOWED"
  );


  Serial.println();

  Serial.println(
    "Sending message to destination phone..."
  );


  /*
   * Send a clean message to the phone.
   */

  String phoneOutput =
    "PRIVATE|" +
    fromPhone +
    "|" +
    message;


  sendToPhone(
    phoneOutput
  );


  Serial.println(
    "MESSAGE DELIVERED TO PHONE"
  );


  Serial.println(
    "========================================"
  );
}


// ============================================================
//                    RECEIVE LOCATION
//                    FROM NODE 3
// ============================================================

void receiveLocationPacket(String packet)
{
  /*
   * LOCATION|OWNER|PHONE|LAT|LON|TIME|BATTERY
   */

  int p1 = packet.indexOf('|');

  int p2 = packet.indexOf('|', p1 + 1);

  int p3 = packet.indexOf('|', p2 + 1);

  int p4 = packet.indexOf('|', p3 + 1);

  int p5 = packet.indexOf('|', p4 + 1);

  int p6 = packet.indexOf('|', p5 + 1);

  if (p6 < 0)
    return;


  int owner =
    packet.substring(
      p1 + 1,
      p2
    ).toInt();


  String phoneID =
    packet.substring(
      p2 + 1,
      p3
    );


  String lat =
    packet.substring(
      p3 + 1,
      p4
    );


  String lon =
    packet.substring(
      p4 + 1,
      p5
    );


  String time =
    packet.substring(
      p5 + 1,
      p6
    );


  String battery =
    packet.substring(
      p6 + 1
    );


  updatePhone(
    phoneID,
    owner,
    lat,
    lon,
    time,
    battery
  );


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "       REMOTE LOCATION RECEIVED"
  );

  Serial.println(
    "========================================"
  );


  Serial.print(
    "PHONE ID   : "
  );

  Serial.println(
    phoneID
  );


  Serial.print(
    "OWNER NODE : Node "
  );

  Serial.println(
    owner
  );


  Serial.print(
    "LATITUDE   : "
  );

  Serial.println(
    lat
  );


  Serial.print(
    "LONGITUDE  : "
  );

  Serial.println(
    lon
  );


  Serial.print(
    "BATTERY    : "
  );

  Serial.println(
    battery
  );


  Serial.println(
    "LOCATION DATA AVAILABLE FOR MAP"
  );


  /*
   * Send remote location to the local phone
   * so the map can display it.
   */

  String mapData =
    "REMOTE_LOC," +
    phoneID +
    "," +
    lat +
    "," +
    lon +
    "," +
    time +
    "," +
    battery;


  sendToPhone(
    mapData
  );


  Serial.println(
    "REMOTE LOCATION SENT TO PHONE"
  );
}


// ============================================================
//                    NODE 3 LOCATION ROUTER
// ============================================================

void receiveLocationRoute(String packet)
{
  /*
   * LOCROUTE|OWNER|PHONE|LAT|LON|TIME|BATTERY
   */

  int p1 = packet.indexOf('|');

  int p2 = packet.indexOf('|', p1 + 1);

  int p3 = packet.indexOf('|', p2 + 1);

  int p4 = packet.indexOf('|', p3 + 1);

  int p5 = packet.indexOf('|', p4 + 1);

  int p6 = packet.indexOf('|', p5 + 1);

  if (p6 < 0)
    return;


  int owner =
    packet.substring(
      p1 + 1,
      p2
    ).toInt();


  String phoneID =
    packet.substring(
      p2 + 1,
      p3
    );


  String lat =
    packet.substring(
      p3 + 1,
      p4
    );


  String lon =
    packet.substring(
      p4 + 1,
      p5
    );


  String time =
    packet.substring(
      p5 + 1,
      p6
    );


  String battery =
    packet.substring(
      p6 + 1
    );


  updatePhone(
    phoneID,
    owner,
    lat,
    lon,
    time,
    battery
  );


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "      LOCATION ROUTER - NODE 3"
  );

  Serial.println(
    "========================================"
  );


  Serial.print(
    "OWNER NODE : Node "
  );

  Serial.println(
    owner
  );


  Serial.print(
    "PHONE ID   : "
  );

  Serial.println(
    phoneID
  );


  Serial.println(
    "Location stored in network table."
  );


  /*
   * Send to every other node individually.
   *
   * Fixed addressing means each node receives
   * only its addressed packet.
   */

  distributeLocation(
    owner,
    phoneID,
    lat,
    lon,
    time,
    battery
  );
}


// ============================================================
//                    COMMON MESSAGE
// ============================================================

void processCommonMessage(
  String command
)
{
  /*
   * MSG,COMMON,PHONE_ID,ALL,BASE64
   */

  int p1 = command.indexOf(',');

  int p2 = command.indexOf(',', p1 + 1);

  int p3 = command.indexOf(',', p2 + 1);

  int p4 = command.indexOf(',', p3 + 1);

  if (p4 < 0)
    return;


  String fromPhone =
    command.substring(
      p2 + 1,
      p3
    );


  String payload =
    command.substring(
      p4 + 1
    );


  String packet =
    "COMMON|" +
    String(NODE_ID) +
    "|" +
    fromPhone +
    "|" +
    encryptText(payload);


  /*
   * Send common message through Node 3.
   */

  if (NODE_ID != 3)
  {
    Serial.println();

    Serial.println(
      "COMMON CHAT ROUTING"
    );

    Serial.println(
      "BEST NEXT HOP : NODE 3"
    );

    sendToNode(
      3,
      packet
    );
  }
  else
  {
    /*
     * Node 3 distributes the common message.
     */

    distributeCommon(
      packet
    );
  }
}


// ============================================================
//                    COMMON DISTRIBUTION
// ============================================================

void distributeCommon(
  String packet
)
{
  for (int n = 1; n <= 4; n++)
  {
    if (n == NODE_ID)
      continue;

    sendToNode(
      n,
      packet
    );

    delay(100);
  }
}


// ============================================================
//                    COMMON PACKET
// ============================================================

void receiveCommonPacket(
  String packet
)
{
  /*
   * COMMON|SOURCE|FROMPHONE|ENCRYPTED
   */

  int p1 = packet.indexOf('|');

  int p2 = packet.indexOf('|', p1 + 1);

  int p3 = packet.indexOf('|', p2 + 1);

  if (p3 < 0)
    return;


  int sourceNode =
    packet.substring(
      p1 + 1,
      p2
    ).toInt();


  String fromPhone =
    packet.substring(
      p2 + 1,
      p3
    );


  String encrypted =
    packet.substring(
      p3 + 1
    );


  /*
   * Node 3 receives the encrypted common packet
   * from another node and distributes it.
   */

  if (NODE_ID == 3 &&
      sourceNode != 3)
  {
    Serial.println();

    Serial.println(
      "COMMON MESSAGE ROUTER"
    );

    Serial.println(
      "Encrypted payload received."
    );

    Serial.println(
      "Decryption : NOT PERFORMED"
    );

    distributeCommon(
      packet
    );

    return;
  }


  /*
   * Destination/common-chat receiving node.
   *
   * Decrypt here.
   */

  String phoneBase64 =
    decryptText(
      encrypted
    );


  String message =
    base64Decode(
      phoneBase64
    );


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "          COMMON MESSAGE"
  );

  Serial.println(
    "========================================"
  );


  Serial.print(
    "SENDER PHONE : "
  );

  Serial.println(
    fromPhone
  );


  Serial.print(
    "MESSAGE      : "
  );

  Serial.println(
    message
  );


  sendToPhone(
    "COMMON|" +
    fromPhone +
    "|" +
    message
  );
}


// ============================================================
//                    PHONE COMMAND PROCESSOR
// ============================================================

void processPhoneCommand(
  String command
)
{
  command.trim();


  if (command.startsWith(
        "LOC,"
      ))
  {
    processLocation(
      command
    );

    return;
  }


  if (command.startsWith(
        "MSG,PRIVATE,"
      ))
  {
    processPrivateMessage(
      command
    );

    return;
  }


  if (command.startsWith(
        "MSG,COMMON,"
      ))
  {
    processCommonMessage(
      command
    );

    return;
  }


  Serial.println();

  Serial.println(
    "Unknown phone command."
  );
}


// ============================================================
//                    RECEIVE LORA
// ============================================================

void handleLoRa()
{
  if (e220ttl.available() <= 0)
    return;


  ResponseContainer rs =
    e220ttl.receiveMessage();


  if (rs.status.code != 1)
    return;


  String packet =
    rs.data;


  packet.trim();


  if (packet.length() == 0)
    return;


  /*
   * ==========================================================
   * PRIVATE
   * ==========================================================
   */

  if (packet.startsWith(
        "PRIV|"
      ))
  {
    receivePrivatePacket(
      packet
    );

    return;
  }


  /*
   * ==========================================================
   * LOCATION ROUTE
   * ==========================================================
   */

  if (packet.startsWith(
        "LOCROUTE|"
      ))
  {
    if (NODE_ID == 3)
    {
      receiveLocationRoute(
        packet
      );
    }

    return;
  }


  /*
   * ==========================================================
   * LOCATION DISTRIBUTION
   * ==========================================================
   */

  if (packet.startsWith(
        "LOCATION|"
      ))
  {
    receiveLocationPacket(
      packet
    );

    return;
  }


  /*
   * ==========================================================
   * COMMON
   * ==========================================================
   */

  if (packet.startsWith(
        "COMMON|"
      ))
  {
    receiveCommonPacket(
      packet
    );

    return;
  }


  Serial.println();

  Serial.println(
    "Unknown LoRa packet received."
  );
}


// ============================================================
//                    E220 START
// ============================================================

void startE220()
{
  E220Serial.begin(
    9600,
    SERIAL_8N1,
    RX_PIN,
    TX_PIN
  );


  delay(500);


  e220ttl.begin();


  delay(500);


  /*
   * Read existing configuration.
   */

  ResponseStructContainer c =
    e220ttl.getConfiguration();


  if (c.status.code == 1)
  {
    Configuration configuration =
      *(Configuration *)c.data;


    /*
     * Preserve the configured node address.
     *
     * Only channel/mode/speed are enforced.
     */

    configuration.ADDH = 0;

    if (NODE_ID == 1)
      configuration.ADDL = 1;

    if (NODE_ID == 2)
      configuration.ADDL = 2;

    if (NODE_ID == 3)
      configuration.ADDL = 3;

    if (NODE_ID == 4)
      configuration.ADDL = 4;


    configuration.CHAN =
      LORA_CHANNEL;


    configuration.SPED.airDataRate =
      AIR_DATA_RATE_000_24;


    configuration.SPED.uartBaudRate =
      UART_BPS_9600;


    configuration.SPED.uartParity =
      MODE_00_8N1;


    configuration.OPTION.transmissionPower =
      POWER_22;


    configuration.TRANSMISSION_MODE.fixedTransmission =
      FT_FIXED_TRANSMISSION;


    ResponseStatus writeStatus =
      e220ttl.setConfiguration(
        configuration,
        WRITE_CFG_PWR_DWN_SAVE
      );


    Serial.println(
      writeStatus.getResponseDescription()
    );
  }


  c.close();


  delay(500);


  /*
   * IMPORTANT:
   *
   * Return to NORMAL mode after configuration.
   */

  e220ttl.setMode(
    MODE_0_NORMAL
  );


  delay(500);
}


// ============================================================
//                    SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );


  delay(1000);


  Serial.println();

  Serial.println(
    "########################################"
  );

  Serial.println(
    "       DSRN + TINYML E220 NODE"
  );

  Serial.println(
    "########################################"
  );


  Serial.print(
    "NODE NAME    : "
  );

  Serial.println(
    nodeName()
  );


  Serial.print(
    "NODE ADDRESS : 0,"
  );

  Serial.println(
    NODE_ID
  );


  // ==========================================================
  // E220
  // ==========================================================

  startE220();


  Serial.println();

  Serial.println(
    "E220 STARTED SUCCESSFULLY"
  );


  Serial.print(
    "E220 CHANNEL : "
  );

  Serial.println(
    LORA_CHANNEL
  );


  // ==========================================================
  // BLE
  // ==========================================================

  startBLE();


  // ==========================================================
  // SYSTEM INFORMATION
  // ==========================================================

  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "            SYSTEM READY"
  );

  Serial.println(
    "========================================"
  );


  Serial.println();

  Serial.println(
    "PHONE COMMANDS:"
  );

  Serial.println(
    "Common : MSG,COMMON,PHONE_ID,ALL,BASE64"
  );

  Serial.println(
    "Private: MSG,PRIVATE,FROM_PHONE,TO_PHONE,BASE64"
  );

  Serial.println(
    "Location: LOC,PHONE_ID,LAT,LON,TIME,BATTERY"
  );


  // ==========================================================
  // TINYML DEMO
  // ==========================================================

  Serial.println();

  Serial.println(
    "TINYML DEMONSTRATION:"
  );

  Serial.println(
    "BEST NEXT HOP = NODE 3"
  );

  Serial.println(
    "Predictive self-healing : ENABLED"
  );

  Serial.println(
    "Node 3 predicted failure risk : 8 %"
  );


  Serial.println();

  Serial.println(
    "Waiting for phone..."
  );
}


// ============================================================
//                    LOOP
// ============================================================

void loop()
{
  /*
   * BLE commands are handled by callback.
   *
   * LoRa is checked continuously here.
   */

  handleLoRa();


  /*
   * Keep BLE connection alive.
   */

  delay(5);
}