#include <WiFiManager.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TimeLib.h>
#include <algorithm>
#include <Wire.h>
#include "string"
#include "config.h"

Config config;

#define BUTTON_2 0
#define TFT_BACKLIGHT_PIN 12 // Adjust for your display's backlight pin
#define RESET_HOLD_TIME 3000 // Time (ms) to hold reset button

WiFiManager wifiManager;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

TFT_eSPI tft = TFT_eSPI();

unsigned long currentTime = 0;
unsigned long lastJsonUpdate = 0;
const unsigned long JSON_UPDATE_INTERVAL = 60000; // Update every minute

String jsonString = "";
String previousCountdownStr = "";
String previousNextStr = "";

const int MAX_DEPTIMES = 300;
int NUM_DEPTIMES = 0;
unsigned long departureTimes[MAX_DEPTIMES];

bool lastButtonState = HIGH;
unsigned long lastButtonPressTime = 0;
bool resetInProgress = false;

unsigned long previousMillis = 0;
const long interval = 1000; // Update display every second

bool dimmed = false; // Track if the screen is dimmed

WiFiManagerParameter custom_gvb_line("gvb_line", "GVB Line (e.g., GVB_902_1)", DEFAULT_GVB_LINE, 40);

String currentDepartureStation = "";
String currentDestinationName = "";
String currentTransportType = "";
String currentHeaderLine = "";
String currentTransportTypeLine = "";

// UI Positions & Sizes
int headerStationY = 50;
int headerTransportY = 80;
int dividerY = 100;
int countdownY = 140; // Adjusted dynamically for centering
int textSizeCountdown = 7;
int textSizeNext = 2;
int nextLineY = 230;
int resetMessageY;

void saveConfigCallback() {
    config.gvb_line = custom_gvb_line.getValue();
    config.save();
    ESP.restart();
}

void configModeCallback(WiFiManager *myWiFiManager) {
    tft.fillScreen(TFT_BLACK);

    // Draw Standard WiFi Icon
    int centerX = tft.width() / 2;
    int centerY = 60; // Position of the WiFi icon
    int arcRadius1 = 25; // Outer arc
    int arcRadius2 = 15; // Inner arc
    int dotRadius = 5;   // Dot size

    // Draw the arcs
    tft.drawArc(centerX, centerY, arcRadius1, 0, 180, 360, TFT_WHITE, TFT_BLACK, true); // Outer arc
    tft.drawArc(centerX, centerY, arcRadius2, 0, 180, 360, TFT_WHITE, TFT_BLACK, true); // Inner arc

    // Draw the dot
    tft.fillCircle(centerX, centerY + arcRadius1 + 10, dotRadius, TFT_WHITE);

    // Display Header Text
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3); // Larger header
    String header = "WiFi Setup";
    int headerWidth = header.length() * 6 * 3; // Approx width of text
    int headerX = (tft.width() - headerWidth) / 2;
    int headerY = centerY + 50;
    tft.setCursor(headerX, headerY);
    tft.println(header);

    // Display Instructions
    tft.setTextSize(2); // Slightly larger instructions
    String line1 = "Connect to wifi:";
    String line2 = "'PontKlok-Config'";
    String line3 = "Then go to:";
    String line4 = "192.168.4.1";

    int textWidth = line1.length() * 6 * 2; // Approx width of text
    int x = (tft.width() - textWidth) / 2;
    tft.setCursor(x, headerY + 40);
    tft.println(line1);

    textWidth = line2.length() * 6 * 2;
    x = (tft.width() - textWidth) / 2;
    tft.setCursor(x, headerY + 70);
    tft.println(line2);

    textWidth = line3.length() * 6 * 2;
    x = (tft.width() - textWidth) / 2;
    tft.setCursor(x, headerY + 110);
    tft.println(line3);

    textWidth = line4.length() * 6 * 2;
    x = (tft.width() - textWidth) / 2;
    tft.setCursor(x, headerY + 140);
    tft.println(line4);
}

void displayResetMessage(bool show) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    if (show) {
        tft.setCursor(10, tft.height() - 30); // Bottom of the screen
        tft.println("HOLD TO RESET");
    } else {
        tft.fillRect(0, tft.height() - 30, tft.width(), 30, TFT_BLACK); // Clear the message area
    }
}

void checkResetButton() {
    bool reading = digitalRead(BUTTON_2);

    if (reading == LOW) { // Button pressed
        if (!resetInProgress) {
            lastButtonPressTime = millis();
            resetInProgress = true;
            displayResetMessage(true);
        } else if (millis() - lastButtonPressTime >= RESET_HOLD_TIME) {
            Serial.println("Reset button held for 3 seconds - clearing WiFi settings");
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("WiFi settings cleared");
            tft.setCursor(10, 40);
            tft.println("Rebooting...");
            delay(2000);
            wifiManager.resetSettings();
            ESP.restart();
        }
    } else { // Button released
        if (resetInProgress) {
            resetInProgress = false;
            displayResetMessage(false); // Clear reset message
        }
    }
}

void setBrightness(int brightness) {
    analogWrite(TFT_BACKLIGHT_PIN, brightness);
}

void dimScreen() {
    if (!dimmed) {
        setBrightness(2); // Set brightness to 1% (approx.)
        dimmed = true;
        Serial.println("Screen dimmed due to no departures.");
    }
}

void brightenScreen() {
    if (dimmed) {
        setBrightness(255); // Set brightness to full
        dimmed = false;
        Serial.println("Screen brightness restored.");
    }
}

void updateJSON() {
    HTTPClient http;
    String url = "http://v0.ovapi.nl/line/" + config.gvb_line;
    Serial.println("Updating JSON from: " + url);
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        jsonString = http.getString();
        Serial.println("JSON updated successfully");
        parseJSON(jsonString);
    } else {
        Serial.print("Error fetching JSON. Error code: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void displayCountdown(long difference) {
    String newStr = (difference < 0) ? "--:--:--" : formatTime(difference);

    // Calculate text dimensions
    int charWidth = 6 * textSizeCountdown;  // Width of a single character
    int charHeight = 8 * textSizeCountdown; // Height of a single character
    int totalWidth = newStr.length() * charWidth; // Total width of the text

    // Center horizontally and vertically
    int xStart = (tft.width() - totalWidth) / 2; // Starting x-coordinate for text
    int y = (tft.height() - charHeight) / 2;     // Center vertically

    // Only update characters that changed
    tft.setTextSize(textSizeCountdown);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    for (int i = 0; i < newStr.length(); i++) {
        // If the character has changed, update it
        if (i >= previousCountdownStr.length() || newStr[i] != previousCountdownStr[i]) {
            int x = xStart + (i * charWidth); // Calculate position of the character
            tft.fillRect(x, y, charWidth, charHeight, TFT_BLACK); // Clear only the area for this character
            tft.setCursor(x, y);
            tft.print(newStr[i]); // Print the new character
        }
    }

    // Update the previous string
    previousCountdownStr = newStr;
}

void displayNextDeparture(unsigned long currentTime) {
    String newStr;
    int firstFutureIndex = -1;
    
    for (int i = 0; i < NUM_DEPTIMES; i++) {
        if (departureTimes[i] > currentTime) {
            firstFutureIndex = i;
            break;
        }
    }

    if (firstFutureIndex == -1 || firstFutureIndex + 1 >= NUM_DEPTIMES) {
        newStr = "--:--:--";
    } else {
        unsigned long depTime = departureTimes[firstFutureIndex + 1];
        long diff = (long)depTime - (long)currentTime;
        newStr = ((diff <= 0) ? "--:--:--" : formatTime(diff));
    }

    // Calculate text dimensions
    int charWidth = 6 * textSizeNext;        // Width of a single character
    int charHeight = 8 * textSizeNext;      // Height of a single character
    int totalWidth = newStr.length() * charWidth; // Total width of the text

    // Center horizontally
    int xStart = (tft.width() - totalWidth) / 2; // Starting x-coordinate
    int y = nextLineY;                           // Fixed y-coordinate for next departure

    // Only update characters that changed
    tft.setTextSize(textSizeNext);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    for (int i = 0; i < newStr.length(); i++) {
        // If the character has changed, update it
        if (i >= previousNextStr.length() || newStr[i] != previousNextStr[i]) {
            int x = xStart + (i * charWidth); // Calculate position of the character
            tft.fillRect(x, y, charWidth, charHeight, TFT_BLACK); // Clear only the area for this character
            tft.setCursor(x, y);
            tft.print(newStr[i]); // Print the new character
        }
    }

    // Update the previous string
    previousNextStr = newStr;
}

unsigned long convertToUnixTime(const char* dateTime) {
    int year, month, day, hour, minute, second;
    sscanf(dateTime, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

    unsigned long days = 0;
    for (int y = 1970; y < year; y++) {
        days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }
    const int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    for (int m = 1; m < month; m++) {
        days += daysInMonth[m - 1];
        if (m == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
            days++;
        }
    }
    days += day - 1;

    unsigned long seconds = days * 24UL * 3600UL;
    seconds += hour * 3600UL;
    seconds += minute * 60UL;
    seconds += second;

    return seconds;
}

String formatTime(long seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    char formattedTime[9];
    snprintf(formattedTime, sizeof(formattedTime), "%02d:%02d:%02d", hours, minutes, secs);
    return String(formattedTime);
}

void parseJSON(String jsonString) {
    StaticJsonDocument<8192> doc;
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        Serial.print("JSON parsing error: ");
        Serial.println(error.c_str());
        return;
    }

    JsonObject line = doc[config.gvb_line]["Line"];
    currentTransportType = line["TransportType"].as<String>();
    currentDestinationName = line["DestinationName50"].as<String>();

    JsonObject actuals = doc[config.gvb_line]["Actuals"];
    NUM_DEPTIMES = 0;

    currentDepartureStation = "";
    for (JsonPair item : actuals) {
        currentDepartureStation = item.value()["TimingPointName"].as<String>();
        const char* departureTime = item.value()["TargetDepartureTime"];
        if (departureTime && strlen(departureTime) > 0) {
            departureTimes[NUM_DEPTIMES++] = convertToUnixTime(departureTime);
        }
    }

    if (NUM_DEPTIMES > 0) {
        std::sort(departureTimes, departureTimes + NUM_DEPTIMES);
    }

    String newHeaderLine = currentDepartureStation + " -> " + currentDestinationName;
    displayHeader(newHeaderLine, currentTransportType);
}

void displayHeader(String newHeaderLine, String newTransportTypeLine) {
    if (newHeaderLine != currentHeaderLine || newTransportTypeLine != currentTransportTypeLine) {
        currentHeaderLine = newHeaderLine;
        currentTransportTypeLine = newTransportTypeLine;

        tft.fillRect(0, 0, tft.width(), dividerY, TFT_BLACK);

        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        tft.setTextSize(2);
        int headerWidth = newHeaderLine.length() * 6 * 2;
        int xCenter = (tft.width() - headerWidth) / 2;
        tft.setCursor(xCenter, headerStationY);
        tft.println(newHeaderLine);

        tft.setTextSize(1);
        int transportWidth = newTransportTypeLine.length() * 6;
        xCenter = (tft.width() - transportWidth) / 2;
        tft.setCursor(xCenter, headerTransportY);
        tft.println(newTransportTypeLine);

        tft.drawLine(0, dividerY, tft.width(), dividerY, TFT_WHITE);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_2, INPUT_PULLUP);
    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    setBrightness(100);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
    }
    config.load();

    wifiManager.addParameter(&custom_gvb_line);
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setConfigPortalTimeout(180);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Connecting to WiFi...");

    if (!wifiManager.autoConnect("PontKlok-Config")) {
        Serial.println("Failed to connect and hit timeout");
        tft.println("WiFi connection failed");
        delay(3000);
        ESP.restart();
    }

    Serial.println("WiFi connected");

    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Loading... Please wait");

    timeClient.begin();

    // Initial update
    updateJSON();
}

void loop() {
    checkResetButton();
    timeClient.update();

    unsigned long currentMillis = millis();

    if (currentMillis - lastJsonUpdate >= JSON_UPDATE_INTERVAL) {
        lastJsonUpdate = currentMillis;
        updateJSON();
    }

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        if (jsonString != "") {
            currentTime = timeClient.getEpochTime() + 7200; // Adding 2 hours offset
            bool foundNextDeparture = false;
            long nextDiff = 0;

            for (int i = 0; i < NUM_DEPTIMES; i++) {
                unsigned long depTime = departureTimes[i];
                if (depTime > currentTime) {
                    nextDiff = depTime - currentTime;
                    foundNextDeparture = true;
                    break;
                }
            }

            if (foundNextDeparture) {
                brightenScreen();
                displayCountdown(nextDiff);
                displayNextDeparture(currentTime);
            } else {
                dimScreen();
                displayCountdown(-1);
                displayNextDeparture(currentTime);
            }
        }
    }
}
