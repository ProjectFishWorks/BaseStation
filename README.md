# BaseStation

The base station acts as the controller for the Fish Sense system. It connects to the devices via CAN Bus, and provides power to them. It then connects to the web app over WiFi and the MQTT protocol.

## Hardware

The base station is powered by the ESP32 S3 WROOM 1 module. The Microchip MCP2551 is used for the CAN Bus interface. Power is provided via a external 24V DC input. This 24V power is provided to the connected devices as well. The 24V going to the devices is switched to allow for controlled power up of the device network. It is also monitored using a current monitroing IC.

## MQTT data format



