#include <cstring> // Include this for C-string operations
#include <cstdio>  // Include for sprintf
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"

#define PN532_SCL 36                // I2C SCL pin
#define PN532_SDA 33                // I2C SDA pin
#define I2C_CLK 400000              // I2C clock rate
#define UART_CLK 115200             // UART baud rate
#define HMAC_HEX_SIZE 65            // maX HMAC hex LENGTH for SHA-256 hex + 1 for null terminator

// const char* ssid = WIFI_SSID;
// const char* password = WIFI_PASSWORD;
// const char* apiHost = API_HOST_NAME;
// const char* hostPort = API_PORT;

// Create PN532 instance using I2C
Adafruit_PN532 nfc(PN532_SCL, PN532_SDA);

// Helper function for encrypting UIDs
void computeHMAC(uint8_t *uid, uint8_t uidLength, char *hmacHexOut) {
    
    const char *hmacKey = "super_secret_key";

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const size_t keyLength = std::strlen(hmacKey);
    unsigned char hmacResult[32];   // SHA-256 outputs 32 bytes

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1); // 1 for HMAC
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)hmacKey, keyLength);
    mbedtls_md_hmac_update(&ctx, uid, uidLength);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // Convert each byte to hex and append to the string
    for (int i = 0; i < sizeof(hmacResult); i++) {
        sprintf(hmacHexOut + i * 2, "%02x", hmacResult[i]);
    }
    hmacHexOut[64] = '\0'; // Ensure the string is null-terminated
}

bool makeHttpRequest(String hmacHash) {
    bool isValid = false;
    // Ensure WiFi is still connected
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return isValid; // Early exit if WiFi is not connected
    }

    HTTPClient http;
    // Construct the URL for the request
    String url = "http://zymurgy:3001/api/" + hmacHash;
    http.begin(url); // Initialize the HTTP client with the URL
    int httpCode = http.GET(); // Perform the GET request

    if(httpCode == 200) { // Check if the request was successful
        String payload = http.getString(); // Get the response payload
        Serial.print("HTTP Response: ");
        Serial.println(payload);

        // Parse the JSON response
        DynamicJsonDocument doc(200);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return isValid;
        }

        // Extract "isValid" value
        if(doc.containsKey("isValid")) {
            isValid = doc["isValid"].as<bool>();
            Serial.println(isValid ? "True" : "False");
        } else {
            Serial.println("Error: Payload does not contain 'isValid'");
        }
    } else if(httpCode == 404) { // Check if the response is 404
        Serial.println("False"); // Handle specific case for 404
    } else {
        // Handle any other HTTP error codes
        Serial.print("Error on HTTP request: ");
        Serial.println(httpCode);
        isValid = false;
    }

    http.end(); // Close the connection
    return isValid;
}

void setup(void) {
    Serial.begin(UART_CLK);

    // Debugging calls
    Serial.print("SSID: '");
    Serial.print(WIFI_SSID);
    Serial.println("'");

    Serial.print("Password: '");
    Serial.print(WIFI_PASSWORD);
    Serial.println("'");

    Serial.println(WIFI_SSID);
    Serial.println(WIFI_PASSWORD);
    Serial.println(TEST_MACRO);

    // Initialize GPIO pin 2 as an output and set it to LOW
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    // Establish WiFi connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Initialize card reader and communication
    nfc.begin();
    Wire.setClock(I2C_CLK);

    // Check reader
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
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };        // Buffer to store the returned UID
    uint8_t uidLength;                              // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (success) {
        char hmacHex[HMAC_HEX_SIZE]; // Buffer for the HMAC hex string
        computeHMAC(uid, uidLength, hmacHex);
        Serial.print("UID HMAC Hex: ");
        Serial.println(hmacHex);
        bool isValid = makeHttpRequest(hmacHex);
        
        if (isValid) {
            Serial.println("Setting strike pin to HIGH...");
            digitalWrite(2, HIGH);                  // Set GPIO 2 high
            delay(7000);                            // Hold the pin high for 7 seconds
            Serial.println("Setting strike pin to LOW...");
            digitalWrite(2, LOW);                   // Then set it low
        }
    }

    delay(1000);
}