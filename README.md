# ESP32-Tivo-Remote

**ESP32-Tivo-Remote** is an open-source project that transforms a Waveshare ESP32-S3 microcontroller with a 1.14" ST7789 display into a Wi-Fi-enabled USB remote control for TiVo Streamers and similar Android TV devices.

## Features

- Acts as a programmable USB keyboard remote control for TiVo and compatible Android TV devices.
- Displays connection status and last pressed key on the built-in TFT display.
- Provides a web interface accessible over Wi-Fi to control the remote from any browser on the same network or via VPN.
- Supports D-pad navigation (up, down, left, right), OK, Back, and Home (mapped to TiVo menu).

## Hardware Requirements

- Waveshare ESP32-S3 GEEK 1.14” ST7789 display module (or compatible ESP32-S3 board with USB OTG and TFT)
- USB connection from ESP32-S3 to TiVo Streamer

## Software Requirements

- Install Arduino IDE with support for ESP32-S3
  - Download Arduino IDE: [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)  
  - Open **Arduino IDE**
  - Go to **File → Preferences**
  - In the **"Additional Board Manager URLs"** field, paste: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  - Click **OK**
  - Go to **Tools → Board → Board Manager**
  - Search for **ESP32**
  - Install **"esp32 by Espressif Systems"**
  - Hold **Boot** button of ESP32-S3 board and connect to USB port of computer
  - In Arduino IDE, go to **Tools → Port**, select the ESP32 COM/TTY port (USB)
  - Set **Tools → Board** to **ESP32S3 Dev Module**
  - Set **Tools → USB CDC on Boot** to **Enabled**
  - Set **Tools → USB Flash Size** to **16 MB**
  - Set **Tools → USB PSRAM** to **OPI PSRAM**
  - Set **Tools → USB Mode** to **USB OTG (Tiny USB)**

- Install Arduino libraries:
  - WiFi
  - WebServer
  - Adafruit_GFX
  - Adafruit_ST7789
  - USB
  - USBHIDKeyboard

## Setup Instructions

1. Clone or download the repository.
2. Open the `ESP32-Tivo-Remote.ino` file in Arduino IDE.
3. Modify the `WIFI_SSID` and `WIFI_PASSWORD` constants in the code to match your Wi-Fi network credentials.
4. Compile and upload the sketch to your ESP32-S3 microcontroller board.
5. Connect the ESP32-S3 USB to your TiVo device.
6. After boot, the TFT will display Wi-Fi connection status and IP address.
7. Access the web interface by entering the displayed IP address into a browser on the same Wi-Fi network or via VPN, e.g. http://192.168.50.233
8. Use your web browser to fully control your TiVo device remotely.
<p float="left">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/b2650494-143f-4182-bdde-8e6ecc2a7b37" width="320px"/>
    <img src="https://github.com/user-attachments/assets/b2650494-143f-4182-bdde-8e6ecc2a7b37" width="320px"/>
  </picture>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/a9a0fa57-b187-4563-afc2-b8eaf78ea35d" width="320px"/>
    <img src="https://github.com/user-attachments/assets/a9a0fa57-b187-4563-afc2-b8eaf78ea35d" width="320px"/>
  </picture>
</p>

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Author

Dirk Tennie [dirk29]
Contact: [dtennie@gmail.com]

---

Enjoy your smart TiVo remote experience with ESP32!

