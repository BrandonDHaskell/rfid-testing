#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "mbedtls/md.h"

#define PN532_SCL 36                // I2C SCL pin
#define PN532_SDA 33                // I2C SDA pin
#define I2C_CLK 400000              // I2C clock rate
#define UART_CLK 115200             // UART baud rate

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

const char *hmacKey = "super_secret_key";

// Create PN532 instance using I2C
Adafruit_PN532 nfc(PN532_SCL, PN532_SDA);

// Helper function for encrypting UIDs
String computeHMAC(uint8_t *uid, uint8_t uidLength) {
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const size_t keyLength = strlen(hmacKey);
    unsigned char hmacResult[32];   // SHA-256 outputs 32 bytes

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1); // 1 for HMAC
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)hmacKey, keyLength);
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)uid, uidLength);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

  // Construct the HMAC hex string
  String hmacHexString = "";
  for (int i = 0; i < sizeof(hmacResult); i++) {
    char str[3];
    sprintf(str, "%02x", hmacResult[i]); // Convert each byte to hex and append to the string
    hmacHexString += str;
  }

  return hmacHexString;
}

void setup(void) {
    Serial.begin(UART_CLK);

    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    nfc.begin();
    Wire.setClock(I2C_CLK);

    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.print("Didn't find PN532 board");
        while (1); // halt
    }

    // Configure board to read RFID tags
    nfc.SAMConfig();
    Serial.println("Hello! Scanning for a card...");
}

void loop(void) {
    uint8_t success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };    // Buffer to store the returned UID
    uint8_t uidLength;                          // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (success) {
        String hmacHex = computeHMAC(uid, uidLength);
        Serial.print("UID HMAC Hex: ");
        Serial.println(hmacHex);

        // Display some details of the card
        // Serial.print("Found an ISO14443A card with UID: 0x");
        // for (uint8_t i = 0; i < uidLength; i++) {
        //     // Print each byte in hex format
        //     if (uid[i] < 0x10) {
        //         // Print leading zero
        //         Serial.print("0");
        //     }
        //     Serial.print(uid[i], HEX);
        // }
        // Serial.println();
        // TODO - add additional logic to get read data
    }

    delay(1000);
}