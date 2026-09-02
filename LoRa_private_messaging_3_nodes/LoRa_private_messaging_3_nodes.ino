#include <LoRa_E220.h>

// ============================================================
// CHANGE ONLY NODE_ID
// ============================================================

// THANU = Node 1
#define NODE_ID 3

// JESS = Node 2
// #define NODE_ID 2

// ASMI = Node 3
// #define NODE_ID 3


// ============================================================
// ESP32 <-> E220 PINS
// ============================================================

#define TX_PIN 17
#define RX_PIN 16
#define AUX_PIN 27
#define M0_PIN 25
#define M1_PIN 26


// ============================================================
// E220 SERIAL
// ============================================================

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(
  &E220Serial,
  AUX_PIN,
  M0_PIN,
  M1_PIN
);


// ============================================================
// NODE ADDRESSES
// ============================================================

#define THANU_ADDH 0
#define THANU_ADDL 1

#define JESS_ADDH  0
#define JESS_ADDL  2

#define ASMI_ADDH  0
#define ASMI_ADDL  3

#define LORA_CHANNEL 18


// ============================================================
// VARIABLES
// ============================================================

int selectedNode = 0;
bool chatMode = false;


// ============================================================
// SETUP
// ============================================================

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

  if (NODE_ID == 1)
    Serial.println("       NODE 1 - THANU");

  else if (NODE_ID == 2)
    Serial.println("       NODE 2 - JESS");

  else if (NODE_ID == 3)
    Serial.println("       NODE 3 - ASMI");

  Serial.println("======================================");

  e220ttl.begin();

  delay(500);

  Serial.println("E220 initialized.");
  Serial.println("Private messaging ready.");
  Serial.println();

  showMenu();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // ALWAYS CHECK FOR RECEIVED MESSAGE
  // ----------------------------------------------------------

  checkReceive();


  // ----------------------------------------------------------
  // CHECK SERIAL INPUT
  // ----------------------------------------------------------

  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');

    input.trim();

    if (input.length() == 0)
      return;


    // ========================================================
    // NODE SELECTION MODE
    // ========================================================

    if (!chatMode)
    {
      int node = input.toInt();


      // Check valid node
      if (node < 1 || node > 3)
      {
        Serial.println();
        Serial.println("Invalid node number.");
        Serial.println();

        showMenu();

        return;
      }


      // Cannot select yourself
      if (node == NODE_ID)
      {
        Serial.println();
        Serial.println("You cannot select your own node.");
        Serial.println();

        showMenu();

        return;
      }


      // Save destination
      selectedNode = node;

      chatMode = true;


      Serial.println();
      Serial.println("======================================");


      if (selectedNode == 1)
        Serial.println("        CHAT WITH THANU");

      else if (selectedNode == 2)
        Serial.println("        CHAT WITH JESS");

      else if (selectedNode == 3)
        Serial.println("        CHAT WITH ASMI");


      Serial.println("======================================");

      Serial.println();
      Serial.println("Type your message and press ENTER.");
      Serial.println("Type /menu to select another node.");
      Serial.println();

      return;
    }


    // ========================================================
    // CHAT MODE
    // ========================================================

    if (chatMode)
    {
      // ------------------------------------------------------
      // RETURN TO NODE MENU
      // ------------------------------------------------------

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


      // ------------------------------------------------------
      // SEND MESSAGE
      // ------------------------------------------------------

      sendMessage(input);
    }
  }
}


// ============================================================
// SEND MESSAGE
// ============================================================

void sendMessage(String message)
{
  uint8_t destinationAddH;
  uint8_t destinationAddL;


  // ==========================================================
  // GET DESTINATION ADDRESS
  // ==========================================================

  if (selectedNode == 1)
  {
    destinationAddH = THANU_ADDH;
    destinationAddL = THANU_ADDL;
  }

  else if (selectedNode == 2)
  {
    destinationAddH = JESS_ADDH;
    destinationAddL = JESS_ADDL;
  }

  else if (selectedNode == 3)
  {
    destinationAddH = ASMI_ADDH;
    destinationAddL = ASMI_ADDL;
  }

  else
  {
    Serial.println("Invalid destination.");
    return;
  }


  // ==========================================================
  // DISPLAY SENDING INFORMATION
  // ==========================================================

  Serial.println();
  Serial.println("--------------------------------------");

  Serial.print("Sending to Node ");
  Serial.print(selectedNode);
  Serial.print(": ");

  Serial.println(message);

  Serial.println("--------------------------------------");


  // ==========================================================
  // FIXED TRANSMISSION
  // ==========================================================

  ResponseStatus rs = e220ttl.sendFixedMessage(
    destinationAddH,
    destinationAddL,
    LORA_CHANNEL,
    message
  );


  // ==========================================================
  // SEND RESULT
  // ==========================================================

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


// ============================================================
// RECEIVE MESSAGE
// ============================================================

void checkReceive()
{
  if (e220ttl.available() > 1)
  {
    ResponseContainer rc =
      e220ttl.receiveMessage();


    if (rc.status.code == 1)
    {
      Serial.println();
      Serial.println();
      Serial.println("======================================");
      Serial.println("          MESSAGE RECEIVED");
      Serial.println("======================================");


      // ------------------------------------------------------
      // DISPLAY RECEIVING NODE
      // ------------------------------------------------------

      Serial.print("Received by: ");

      if (NODE_ID == 1)
        Serial.println("THANU");

      else if (NODE_ID == 2)
        Serial.println("JESS");

      else if (NODE_ID == 3)
        Serial.println("ASMI");


      // ------------------------------------------------------
      // DISPLAY MESSAGE
      // ------------------------------------------------------

      Serial.print("Message: ");
      Serial.println(rc.data);


      Serial.println("======================================");
      Serial.println();


      // ------------------------------------------------------
      // CONTINUE CHAT
      // ------------------------------------------------------

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


// ============================================================
// NODE SELECTION MENU
// ============================================================

void showMenu()
{
  Serial.println();
  Serial.println("======================================");
  Serial.println("         SELECT DESTINATION");
  Serial.println("======================================");

  Serial.println();


  if (NODE_ID != 1)
    Serial.println("1 - THANU");

  if (NODE_ID != 2)
    Serial.println("2 - JESS");

  if (NODE_ID != 3)
    Serial.println("3 - ASMI");


  Serial.println();
  Serial.println("Enter node number:");
}