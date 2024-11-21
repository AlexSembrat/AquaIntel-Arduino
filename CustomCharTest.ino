//This file uses a custom characteristic and sends a byte of data every second.

#include <ArduinoBLE.h>
#include <string>

// Define a custom service and characteristic UUID
#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "abcdef01-1234-5678-1234-56789abcdef0"

BLEService customService(SERVICE_UUID); // Create the custom service
BLECharacteristic customCharacteristic(
    CHARACTERISTIC_UUID,
    BLERead | BLEWrite | BLENotify, // Add BLENotify to enable notifications
    20                              // Max size of the characteristic value
);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  // Set the local name and the advertised service
  BLE.setLocalName("NANO33");
  BLE.setDeviceName("NANO33");
  BLE.setAdvertisedService(customService);

  // Add the characteristic to the service
  customService.addCharacteristic(customCharacteristic);
  uint8_t value = 0;  // Example numeric value
  customCharacteristic.writeValue(&value, 1);

  // Add the service
  BLE.addService(customService);

  // Start advertising
  BLE.advertise();
  Serial.println("BLE device is now advertising...");
}

void loop() {
  // Listen for BLE connections
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());

    // While connected
    while (central.connected()) {
      if (customCharacteristic.written()) {
        // Get the raw data and its length
        const uint8_t* data = customCharacteristic.value();
        size_t length = customCharacteristic.valueLength();

        // Print the received value
        Serial.print("Received: ");
        Serial.println(*data);
      }

      // Check if there is Serial input
      // if (Serial.available()) {
      //   String input = Serial.readStringUntil('\n'); // Read a line from Serial
      //   if (input.length() > 0) {
      //     customCharacteristic.writeValue(input.c_str()); // Write to BLE characteristic
      //     Serial.print("Sent to BLE: ");
      //     Serial.println(input);
      //   }
      // }

      static uint16_t counter = 0;
      delay(1000);  // Wait 1 second
      counter++;
      customCharacteristic.writeValue(&counter, 1);  // Update characteristic value
      Serial.print("Notified value: ");
      Serial.println(counter);
    }

    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}
