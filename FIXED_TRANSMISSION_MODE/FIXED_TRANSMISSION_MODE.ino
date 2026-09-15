#include <LoRa_E220.h>

// ============================================================
// ESP32 <-> E220 PINS
// ============================================================

#define TX_PIN 17
#define RX_PIN 16
#define AUX_PIN 27
#define M0_PIN 25
#define M1_PIN 26


// ============================================================
// CHANGE ONLY THIS
// ============================================================

// THANU = Node 1
#define NODE_ID 3

// JESS = Node 2
// #define NODE_ID 2

// ASMI = Node 3
// #define NODE_ID 3


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
  Serial.println("       E220 NODE CONFIGURATION");
  Serial.println("======================================");


  // ==========================================================
  // DISPLAY NODE NAME
  // ==========================================================

  if (NODE_ID == 1)
  {
    Serial.println("Node: THANU");
    Serial.println("Address: 0,1");
  }
  else if (NODE_ID == 2)
  {
    Serial.println("Node: JESS");
    Serial.println("Address: 0,2");
  }
  else if (NODE_ID == 3)
  {
    Serial.println("Node: ASMI");
    Serial.println("Address: 0,3");
  }
  else if (NODE_ID == 4)
  {
    Serial.println("Node: KAVIN");
    Serial.println("Address: 0,4");
  }


  // ==========================================================
  // START E220
  // ==========================================================

  e220ttl.begin();

  delay(500);


  // ==========================================================
  // READ CURRENT CONFIGURATION
  // ==========================================================

  ResponseStructContainer c =
      e220ttl.getConfiguration();


  if (c.status.code != 1)
  {
    Serial.println();
    Serial.println("Configuration read failed!");

    c.close();

    return;
  }


  Configuration configuration =
      *(Configuration*)c.data;


  // ==========================================================
  // SET UNIQUE NODE ADDRESS
  // ==========================================================

  configuration.ADDH = 0;


  if (NODE_ID == 1)
  {
    configuration.ADDL = 1;
  }
  else if (NODE_ID == 2)
  {
    configuration.ADDL = 2;
  }
  else if (NODE_ID == 3)
  {
    configuration.ADDL = 3;
  }
   else if (NODE_ID == 4)
  {
    configuration.ADDL = 4;
  }


  // ==========================================================
  // CHANNEL
  // ==========================================================

  configuration.CHAN = 18;


  // ==========================================================
  // AIR DATA RATE
  // ==========================================================

  configuration.SPED.airDataRate =
      AIR_DATA_RATE_000_24;


  // ==========================================================
  // UART
  // ==========================================================

  configuration.SPED.uartBaudRate =
      UART_BPS_9600;

  configuration.SPED.uartParity =
      MODE_00_8N1;


  // ==========================================================
  // TRANSMISSION POWER
  // ==========================================================

  configuration.OPTION.transmissionPower =
      POWER_22;


  // ==========================================================
  // VERY IMPORTANT
  // FIXED TRANSMISSION
  // ==========================================================

  configuration.TRANSMISSION_MODE.fixedTransmission =
      FT_FIXED_TRANSMISSION;


  // ==========================================================
  // SAVE CONFIGURATION
  // ==========================================================

  Serial.println();
  Serial.println("Writing configuration...");

  ResponseStatus rs =
      e220ttl.setConfiguration(
        configuration,
        WRITE_CFG_PWR_DWN_SAVE
      );


  Serial.println(
      rs.getResponseDescription()
  );


  c.close();


  // ==========================================================
  // WAIT
  // ==========================================================

  delay(1500);


  // ==========================================================
  // VERIFY CONFIGURATION
  // ==========================================================

  ResponseStructContainer check =
      e220ttl.getConfiguration();


  if (check.status.code == 1)
  {
    Configuration verify =
        *(Configuration*)check.data;


    Serial.println();
    Serial.println("======================================");
    Serial.println("       FINAL CONFIGURATION");
    Serial.println("======================================");


    Serial.print("Node ID: ");
    Serial.println(NODE_ID);


    Serial.print("Address High: ");
    Serial.println(verify.ADDH);


    Serial.print("Address Low: ");
    Serial.println(verify.ADDL);


    Serial.print("Channel: ");
    Serial.println(verify.CHAN);


    Serial.println("Frequency: 868.125 MHz");


    Serial.println("Air Data Rate: 2.4 kbps");


    Serial.println("UART: 9600 8N1");


    Serial.println("Transmit Power: 22 dBm");


    Serial.println("Transmission Mode: FIXED");


    Serial.println("======================================");


    // ========================================================
    // CHECK ADDRESS
    // ========================================================

    bool addressOK = false;


    if (NODE_ID == 1 &&
        verify.ADDH == 0 &&
        verify.ADDL == 1)
    {
      addressOK = true;
    }


    if (NODE_ID == 2 &&
        verify.ADDH == 0 &&
        verify.ADDL == 2)
    {
      addressOK = true;
    }


    if (NODE_ID == 3 &&
        verify.ADDH == 0 &&
        verify.ADDL == 3)
    {
      addressOK = true;
    }
    if (NODE_ID == 4 &&
        verify.ADDH == 0 &&
        verify.ADDL == 4)
    {
      addressOK = true;
    }


    if (addressOK)
    {
      Serial.println();
      Serial.println("ADDRESS VERIFIED SUCCESSFULLY!");
    }
    else
    {
      Serial.println();
      Serial.println("WARNING: ADDRESS VERIFICATION FAILED!");
    }


    Serial.println();
    Serial.println("CONFIGURATION COMPLETE!");
  }
  else
  {
    Serial.println();
    Serial.println("Configuration verification failed!");
  }


  check.close();
}


void loop()
{
}