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

  Serial.println();
  Serial.println("==============================");
  Serial.println("   E220 BIDIRECTIONAL TEST");
  Serial.println("==============================");

  e220ttl.begin();

  delay(500);

  Serial.println("Ready!");
  Serial.println("Type a message and press ENTER.");
}

void loop() {

  // -----------------------------
  // RECEIVE FROM OTHER NODE
  // -----------------------------
  if (e220ttl.available() > 1) {

    ResponseContainer rc = e220ttl.receiveMessage();

    if (rc.status.code == 1) {
      Serial.print("Received: ");
      Serial.println(rc.data);
    } else {
      Serial.println("Receive failed");
    }
  }

  // -----------------------------
  // SEND TO OTHER NODE
  // -----------------------------
  if (Serial.available()) {

    String message = Serial.readStringUntil('\n');
    message.trim();

    if (message.length() > 0) {

      ResponseStatus rs = e220ttl.sendMessage(message);

      if (rs.code == 1) {
        Serial.print("Sent: ");
        Serial.println(message);
      } else {
        Serial.print("Send failed: ");
        Serial.println(rs.getResponseDescription());
      }
    }
  }
}