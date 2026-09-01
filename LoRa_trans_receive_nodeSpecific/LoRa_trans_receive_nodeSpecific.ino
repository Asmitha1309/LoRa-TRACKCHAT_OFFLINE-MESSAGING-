#include <LoRa_E220.h>

// ==================================================
// ESP32 <-> E220 PINS
// ==================================================

#define TX_PIN 17
#define RX_PIN 16
#define AUX_PIN 27
#define M0_PIN 25
#define M1_PIN 26

// ==================================================
// CHANGE ONLY THIS
// ==================================================

// NODE 1
#define NODE_ID 1

// For Node 2 use:
// #define NODE_ID 2

// For Node 3 use:
// #define NODE_ID 3


// ==================================================
// E220 SERIAL
// ==================================================

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(
  &E220Serial,
  AUX_PIN,
  M0_PIN,
  M1_PIN
);


// ==================================================
// VARIABLES
// ==================================================

int selectedNode = 0;
bool chatMode = false;


// ==================================================
// SETUP
// ==================================================

void setup()
{
  Serial.begin(115200);

  E220Serial.begin(
    9600,
    SERIAL_8N1,
    RX_PIN,
    TX_PIN
  );

  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.print("        Thanu ");
  Serial.println(NODE_ID);
  Serial.println("======================================");

  e220ttl.begin();

  delay(500);

  Serial.println("E220 initialized.");
  Serial.println("Waiting for messages...");
  Serial.println();

  showMenu();
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop()
{
  // =================================================
  // ALWAYS CHECK FOR RECEIVED MESSAGE
  // =================================================

  checkReceive();


  // =================================================
  // CHECK KEYBOARD INPUT
  // =================================================

  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');

    input.trim();

    if (input.length() == 0)
    {
      return;
    }


    // ===============================================
    // NODE SELECTION
    // ===============================================

    if (!chatMode)
    {
      int node = input.toInt();

      if (node < 1 || node > 3)
      {
        Serial.println();
        Serial.println("Invalid node.");
        showMenu();
        return;
      }


      // Cannot select itself

      if (node == NODE_ID)
      {
        Serial.println();
        Serial.println("You cannot select your own node.");
        Serial.println();

        showMenu();
        return;
      }


      selectedNode = node;
      chatMode = true;

      Serial.println();
      Serial.println("======================================");
      Serial.print("        CHAT WITH NODE ");
      Serial.println(selectedNode);
      Serial.println("======================================");

      Serial.println();
      Serial.println("Type message and press ENTER.");
      Serial.println("Type /menu to select another node.");
      Serial.println();

      return;
    }


    // ===============================================
    // CHAT MODE
    // ===============================================

    if (chatMode)
    {
      // Return to node selection

      if (input.equalsIgnoreCase("/menu"))
      {
        chatMode = false;
        selectedNode = 0;

        Serial.println();
        Serial.println("Returning to node selection...");
        Serial.println();

        showMenu();

        return;
      }


      // Send message

      sendMessage(input);
    }
  }
}


// ==================================================
// RECEIVE FUNCTION
// ==================================================

void checkReceive()
{
  if (e220ttl.available() > 1)
  {
    ResponseContainer rc = e220ttl.receiveMessage();

    if (rc.status.code == 1)
    {
      Serial.println();
      Serial.println();
      Serial.println("======================================");
      Serial.println("          MESSAGE RECEIVED");
      Serial.println("======================================");

      Serial.print("From another node: ");
      Serial.println(rc.data);

      Serial.println("======================================");
      Serial.println();


      // After receiving, continue chatting

      if (chatMode)
      {
        Serial.print("Chat with Node ");
        Serial.println(selectedNode);

        Serial.println("Type message:");
      }
      else
      {
        showMenu();
      }
    }
    else
    {
      Serial.print("Receive error: ");
      Serial.println(
        rc.status.getResponseDescription()
      );
    }
  }
}


// ==================================================
// SEND FUNCTION
// ==================================================

void sendMessage(String message)
{
  Serial.println();
  Serial.println("--------------------------------------");

  Serial.print("Sending to Node ");
  Serial.print(selectedNode);
  Serial.print(": ");

  Serial.println(message);

  Serial.println("--------------------------------------");


  // ================================================
  // FIXED TRANSMISSION
  //
  // ADDH       = 0
  // ADDL       = selected node
  // CHANNEL    = 18
  // ================================================

  ResponseStatus rs = e220ttl.sendFixedMessage(
    0,
    selectedNode,
    18,
    message
  );


  if (rs.code == 1)
  {
    Serial.println("Message sent successfully!");
  }
  else
  {
    Serial.print("Send error: ");
    Serial.println(
      rs.getResponseDescription()
    );
  }

  Serial.println();
}


// ==================================================
// MENU
// ==================================================

void showMenu()
{
  Serial.println("======================================");
  Serial.println("         SELECT DESTINATION");
  Serial.println("======================================");

  Serial.println();

  if (NODE_ID != 1)
    Serial.println("1 - Node 1");

  if (NODE_ID != 2)
    Serial.println("2 - Node 2");

  if (NODE_ID != 3)
    Serial.println("3 - Node 3");

  Serial.println();

  Serial.println("Enter node number:");
}