#include <LoRa_E220.h>

#define RX_PIN 16
#define TX_PIN 17

#define AUX_PIN 27
#define M0_PIN 25
#define M1_PIN 26

HardwareSerial E220Serial(2);

LoRa_E220 e220(
  &E220Serial,
  AUX_PIN,
  M0_PIN,
  M1_PIN
);

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

  e220.begin();
  e220.setMode(MODE_0_NORMAL);

  Serial.println("NODE C - RECEIVER");
  Serial.println("Waiting for messages...");
}

void loop()
{
  if (e220.available() > 0)
  {
    ResponseContainer rs = e220.receiveMessage();

    if (rs.status.code == E220_SUCCESS)
    {
      Serial.print("RX: ");
      Serial.println(rs.data);
    }
  }
}