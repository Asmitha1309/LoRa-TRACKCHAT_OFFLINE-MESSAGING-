#include <Arduino.h>
#include <mbedtls/gcm.h>
#include <esp_system.h>

// =====================================================
// AES-128-GCM TEST
// ESP32 Dev Module
// =====================================================

// 128-bit = 16-byte AES key
const uint8_t AES_KEY[16] = {
  0x10, 0x23, 0x45, 0x67,
  0x89, 0xAB, 0xCD, 0xEF,
  0x12, 0x34, 0x56, 0x78,
  0x9A, 0xBC, 0xDE, 0xF0
};

// GCM normally uses a 12-byte nonce
const size_t NONCE_SIZE = 12;

// Authentication tag size
const size_t TAG_SIZE = 16;


// =====================================================
// PRINT BYTES IN HEX
// =====================================================

void printHex(const uint8_t *data, size_t length)
{
  for (size_t i = 0; i < length; i++)
  {
    if (data[i] < 0x10)
      Serial.print("0");

    Serial.print(data[i], HEX);
    Serial.print(" ");
  }

  Serial.println();
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("       ESP32 AES-128-GCM TEST");
  Serial.println("======================================");


  // ---------------------------------------------------
  // Original message
  // ---------------------------------------------------

  const char *message = "HELLO KAVIN";

  size_t messageLength = strlen(message);

  Serial.println();
  Serial.print("Original Message : ");
  Serial.println(message);


  // ---------------------------------------------------
  // Create a random nonce
  // ---------------------------------------------------

  uint8_t nonce[NONCE_SIZE];

  esp_fill_random(nonce, NONCE_SIZE);

  Serial.println();
  Serial.print("Nonce            : ");
  printHex(nonce, NONCE_SIZE);


  // ---------------------------------------------------
  // Buffers
  // ---------------------------------------------------

  uint8_t ciphertext[64];
  uint8_t decrypted[64];
  uint8_t tag[TAG_SIZE];


  // ---------------------------------------------------
  // AES-GCM context
  // ---------------------------------------------------

  mbedtls_gcm_context gcm;

  mbedtls_gcm_init(&gcm);


  // ---------------------------------------------------
  // Set AES-128 key
  // ---------------------------------------------------

  int result = mbedtls_gcm_setkey(
    &gcm,
    MBEDTLS_CIPHER_ID_AES,
    AES_KEY,
    128
  );

  if (result != 0)
  {
    Serial.println();
    Serial.println("ERROR: AES key setup failed!");

    mbedtls_gcm_free(&gcm);

    return;
  }

  Serial.println("AES Key Setup    : SUCCESS");


  // ===================================================
  // ENCRYPTION
  // ===================================================

  result = mbedtls_gcm_crypt_and_tag(
    &gcm,
    MBEDTLS_GCM_ENCRYPT,

    messageLength,

    nonce,
    NONCE_SIZE,

    NULL,
    0,

    (const uint8_t *)message,

    ciphertext,

    TAG_SIZE,
    tag
  );


  if (result != 0)
  {
    Serial.println();
    Serial.println("ERROR: AES encryption failed!");

    mbedtls_gcm_free(&gcm);

    return;
  }


  Serial.println();
  Serial.println("---------- ENCRYPTION ----------");

  Serial.println("Encryption       : SUCCESS");

  Serial.print("Ciphertext       : ");
  printHex(ciphertext, messageLength);

  Serial.print("Authentication Tag: ");
  printHex(tag, TAG_SIZE);


  // ===================================================
  // DECRYPTION
  // ===================================================

  result = mbedtls_gcm_auth_decrypt(
    &gcm,

    messageLength,

    nonce,
    NONCE_SIZE,

    NULL,
    0,

    tag,
    TAG_SIZE,

    ciphertext,

    decrypted
  );


  Serial.println();
  Serial.println("---------- DECRYPTION ----------");


  if (result != 0)
  {
    Serial.println("Authentication   : FAILED");
    Serial.println("Decryption       : FAILED");

    mbedtls_gcm_free(&gcm);

    return;
  }


  // Add string terminator
  decrypted[messageLength] = '\0';


  Serial.println("Authentication   : SUCCESS");

  Serial.print("Decrypted Message: ");
  Serial.println((char *)decrypted);


  // ===================================================
  // FINAL CHECK
  // ===================================================

  if (strcmp(message, (char *)decrypted) == 0)
  {
    Serial.println();
    Serial.println("======================================");
    Serial.println("        AES TEST SUCCESSFUL");
    Serial.println("======================================");
    Serial.println("Original  == Decrypted");
    Serial.println("AES-128-GCM is working correctly!");
  }
  else
  {
    Serial.println();
    Serial.println("======================================");
    Serial.println("        AES TEST FAILED");
    Serial.println("======================================");
  }


  // Free AES context
  mbedtls_gcm_free(&gcm);
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
}