#include <LoRa_E220.h>

#define TX_PIN 17
#define RX_PIN 16
#define AUX_PIN 27
#define M0_PIN 25
#define M1_PIN 26

// CHANGE THIS ON EACH ESP32
#define NODE_NAME "NODE 1"

HardwareSerial E220Serial(2);

LoRa_E220 e220ttl(&E220Serial, AUX_PIN, M0_PIN, M1_PIN);

void setup() {

  Serial.begin(115200);

  E220Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println(NODE_NAME);
  Serial.println("E220 THREE NODE CHAT");
  Serial.println("================================");

  e220ttl.begin();

  delay(500);

  Serial.println("LoRa Ready!");
  Serial.println("Type message and press ENTER.");
  Serial.println();
}

void loop() {

  // =====================================
  // SEND
  // =====================================

  if (Serial.available()) {

    String message = Serial.readStringUntil('\n');
    message.trim();

    if (message.length() > 0) {

      String packet = String(NODE_NAME) + ": " + message;

      ResponseStatus rs = e220ttl.sendMessage(packet);

      Serial.print("You: ");
      Serial.println(message);

      if (rs.code == 1) {
        Serial.println("Message sent!");
      } else {
        Serial.print("Send error: ");
        Serial.println(rs.getResponseDescription());
      }
    }
  }


  // =====================================
  // RECEIVE
  // =====================================

  if (e220ttl.available() > 1) {

    ResponseContainer rc = e220ttl.receiveMessage();

    if (rc.status.code == 1) {

      Serial.print("Received: ");
      Serial.println(rc.data);

    } else {

      Serial.print("Receive error: ");
      Serial.println(
        rc.status.getResponseDescription()
      );
    }
  }
}