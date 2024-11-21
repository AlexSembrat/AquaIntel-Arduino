//This file uses a custom service and characteristic and send 2 bytes of data every 1 second.

#include <ArduinoBLE.h>
#include <string>

// Define a custom service and characteristic UUID
#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "abcdef01-1234-5678-1234-56789abcdef0"

BLEService customService(SERVICE_UUID); // Create the custom service
BLECharacteristic customCharacteristic(
    CHARACTERISTIC_UUID,
    BLERead | BLENotify,  // Allow reading and notifying
    2                     // 2 bytes for uint16_t
);

uint16_t value = 0; // Initialize the value

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
  customCharacteristic.writeValue((uint8_t *)&value, 2); // Write initial value

  // Add the service
  BLE.addService(customService);

  // Start advertising
  BLE.advertise();
  Serial.println("BLE device is now advertising...");
}

void loop() {
  BLEDevice central = BLE.central(); // Listen for BLE connections

  if (central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());

    // While connected
    while (central.connected()) {
      delay(1000); // Increment every second
      value++; // Increment the uint16_t value
      customCharacteristic.writeValue((uint8_t *)&value, 2); // Write updated value
      Serial.print("Value sent: ");
      Serial.println(value);
    }

    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}
