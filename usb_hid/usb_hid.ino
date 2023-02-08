#include "PluggableUSBHID.h"
#include "USBKeyboard.h"
#include <ArduinoBLE.h>

BLEService HIDService("180A"); // BLE LED Service
// BLE LED Switch Characteristic - custom 128-bit UUID, read and writable by central
BLEByteCharacteristic switchCharacteristic("2A57", BLERead | BLEWrite);
USBKeyboard Keyboard;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // begin initialization
  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy failed!");
    while (1);
  }

  // set advertised local name and service UUID:
  BLE.setLocalName("Nano 33 BLE Sense");
  BLE.setAdvertisedService(HIDService);

  // add the characteristic to the service
  HIDService.addCharacteristic(switchCharacteristic);

  // add service
  BLE.addService(HIDService);

  // set the initial value for the characteristic:
  switchCharacteristic.writeValue(0);

  // start advertising
  BLE.advertise();

  Serial.println("BLE LED Peripheral");

}

void loop() {

  // listen for Bluetooth® Low Energy peripherals to connect:
  BLEDevice central = BLE.central();

    // if a central is connected to peripheral:
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's MAC address:
    Serial.println(central.address());
    Serial.println("Open up the Chrome Dino game to start playing!");
    // while the central is still connected to peripheral:
    while (central.connected()) {
      // if the remote device wrote to the characteristic,
      // use the value to send a keyboard command
      if (switchCharacteristic.written()) {
        switch (switchCharacteristic.value()) {   // any value other than 0
          case 01:
            // Hit space bar
            Serial.println("Spacebar");
            Keyboard.key_code(0x20, 0);
            break;
          case 02:
            // Hit the down arrow
            Serial.println("Down arrow");
            for (int i = 0; i < 300; i++)
              Keyboard.key_code(DOWN_ARROW);
            break;
          default:
            Serial.println("Unknown input. Doing nothing.");
            break;
        }
      }
    }

    // when the central disconnects, print it out:
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
  }
}
