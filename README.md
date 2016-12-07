# esp32-ota (ESP32 Over-the-air update)

esp32-ota is a small demo application that shows how to upload new firmware images to an ESP32 board. It consists of 2 parts:

- A bash script update_firmware.sh in the tools directory. This script reads the application image file (bin file) from the compilation process and sends it to the ESP32 via WiFi.

- Application code for the ESP32 to connect to an access point, listen for firmware update requests and write new firmware into flash memory.

More information is available on our blog: https://blog.classycode.com/over-the-air-updating-an-esp32-29f83ebbcca2#.g5nwb78eb
