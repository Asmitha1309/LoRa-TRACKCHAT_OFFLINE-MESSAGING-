#include <LoRa_E220.h>

#define TX_PIN 17
#define RX_PIN 16
#define AUX_PIN 27
#define M0_PIN 25
#define M1_PIN 26

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(&E220Serial, AUX_PIN, M0_PIN, M1_PIN);

void setup() {
  Serial.begin(115200);

  E220Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  delay(1000);

  Serial.println("================================");
  Serial.println("NODE 2 - LORA RECEIVER");
  Serial.println("================================");

  e220ttl.begin();

  delay(500);
}

void loop() {

  if (e220ttl.available() > 1) {

    ResponseContainer rc = e220ttl.receiveMessage();

    if (rc.status.code == 1) {

      Serial.print("Received: ");
      Serial.println(rc.data);

    } else {

      Serial.print("Receive error: ");
      Serial.println(rc.status.getResponseDescription());
    }
  }
}