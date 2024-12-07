# Amsterdam GVB Ferry Clock (PontKlok)

This project displays real-time countdowns for GVB ferries (or other public transport lines) on a LilyGO LILY Pi ESP32 device with a TFT display. It fetches data from OVAPI, which provides live departure times of public transportation in the Netherlands, and shows a countdown until the next departure. Additionally, you can configure a specific line code for ferries or other GVB lines.

## Features
- Real-time countdown to the next departure for a chosen Dutch public transport line.
- Configurable line code through a WiFiManager portal.
- Minimalistic UI with a black background and white text.
- Smooth updates: Only changing characters on the countdown and next-departure times are redrawn.
- Dynamic screen brightness:
  - Adjusts based on ambient light levels.
  - Dims to 1% brightness when no departures are available and restores full brightness when departures resume.
- Hold-to-reset functionality:
  - Press and hold the reset button for 3 seconds to reset WiFi and line code settings. 
  - Displays "HOLD TO RESET" on the screen during the hold process.

## Requirements
- **Hardware:**
    - LilyGO LILY Pi ESP32 board
- **Software:**
    - Arduino IDE (tested on version 1.8.x or later)
    - ESP32 Board Support for Arduino IDE
    - Required libraries (WiFiManager, NTPClient, ArduinoJson, TFT_eSPI, etc.)

## Getting Started

### 1. Install ESP32 Board in Arduino IDE
If you haven't already:
1. Open Arduino IDE.
2. Go to **File** > **Preferences**.
3. In the **Additional Board Manager URLs** field, add:
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
4. Go to **Tools** > **Board** > **Boards Manager**, search for "ESP32" and install the esp32 boards package.

### 2. Select the Correct Board
- In Arduino IDE, go to **Tools** > **Board** and select **ESP32 Wrover Module**.  
  The LilyGO LILY Pi board is compatible with this setting.

### 3. Install and Configure Libraries
You will need:
- **WiFiManager**: For easy WiFi configuration.
- **TFT_eSPI**: For controlling the display. Ensure `User_Setup_Select.h` is properly configured for your display.
- **NTPClient**: For time synchronization.
- **ArduinoJson**: For parsing JSON from OVAPI.
- **HTTPClient**: For fetching data over HTTP.

You can install them from the Arduino Library Manager:
- **Sketch** > **Include Library** > **Manage Libraries**...
- Search and install the required libraries.

### 4. OVAPI Info and Line Code
[OVAPI](http://v0.ovapi.nl/) is an open API that provides real-time data for public transport in the Netherlands. To confirm a specific line code exists, visit:
http://v0.ovapi.nl/line/line_code
For example: http://v0.ovapi.nl/line/GVB_15_1

If it returns a JSON with line information, it means the line is available through OVAPI.

**Choosing Your Line Code:**
- The default code in the project is something like `GVB_902_1` for a ferry line.
- If you want a different line, find your line on OVAPI and verify it by visiting the URL as shown above.
- Once the board boots, it will create a WiFi Access Point if not already configured. Connect to `PontKlok-Config` and enter your WiFi credentials and the desired line code in the portal.
- The device will save this configuration and then start displaying countdowns for that line.

### 5. Compile and Upload
1. Connect your LilyGO LILY Pi ESP32 device to your computer via USB.
2. Select the appropriate COM port in **Tools** > **Port**.
3. Click the **Upload** button in Arduino IDE.
4. After a successful upload, open the Serial Monitor (115200 baud) to debug if needed.

### 6. Using the Device
- On first startup, if WiFi is not configured, it will create a WiFi configuration AP called `PontKlok-Config`.
- Connect with your phone or PC to this AP.
- Go to `http://192.168.4.1` and enter:
    - Your WiFi SSID and password
    - The line code (e.g., `GVB_15_1`)
- Save and reboot. The device will connect to your WiFi and start showing countdowns for the specified line.

**Resetting WiFi and Line Code Settings:**
- Press and hold the reset button for **3 seconds**.
- The screen will display "HOLD TO RESET" at the bottom while the button is held.
- After 3 seconds, the device will reset the WiFi and line code settings and reboot into configuration mode.

## About OVAPI
[OVAPI](http://v0.ovapi.nl/) is a public transport API for the Netherlands. It provides:
- Real-time departure times
- Line, stop, and location data

This project leverages OVAPI to fetch departure times of a ferry or tram/bus line and display a countdown. The data is updated every minute.

## Troubleshooting
- If you don't see updates, check your line code or try opening the OVAPI line URL in a browser to ensure it returns valid data.
- Make sure your ESP32 is connected to the internet (check WiFi credentials and signal strength).
- Use the Serial Monitor for debugging and verifying that the JSON is correctly parsed and displayed.
- For screen brightness issues, check the ambient light sensor and backlight pin connections.

## License
This project is provided as-is under an open-source license. Check the LICENSE file for more details.