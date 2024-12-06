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

WiFiManager wifiManager;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

TFT_eSPI tft = TFT_eSPI();

unsigned long currentTime = 0;
unsigned long previousDifference = 0;
unsigned long lastJsonUpdate = 0;
const unsigned long JSON_UPDATE_INTERVAL = 60000; // Update every minute

String jsonString = "";
String previousCountdownStr = "";
String previousNextStr = "";

const int MAX_DEPTIMES = 300;
int NUM_DEPTIMES = 0;
unsigned long departureTimes[MAX_DEPTIMES];

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long previousMillis = 0;
const long interval = 1000; // Update display every second

WiFiManagerParameter custom_gvb_line("gvb_line", "GVB Line (e.g., GVB_902_1)", DEFAULT_GVB_LINE, 40);
WiFiManagerParameter custom_city("city", "City (e.g., Amsterdam)", DEFAULT_CITY, 40);

String currentDepartureStation = "";
String currentDestinationName = "";
String currentTransportType = "";

// UI Positions & Sizes
int headerStationY = 50;
int headerTransportY = 80;
int dividerY = 100;
int countdownY = 140;
int textSizeCountdown = 8;
int textSizeNext = 2;
int nextLineY = 230;

void saveConfigCallback() {
    config.gvb_line = custom_gvb_line.getValue();
    config.city = custom_city.getValue();
    config.save();
    ESP.restart();
}

void configModeCallback(WiFiManager *myWiFiManager) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("WiFi Configuration Mode");
    tft.setCursor(10, 40);
    tft.println("Connect to 'PontKlok-Config'");
    tft.setCursor(10, 70);
    tft.println("Then visit 192.168.4.1");
}

void checkResetButton() {
    bool reading = digitalRead(BUTTON_2);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading == LOW) {
            Serial.println("Reset button pressed - clearing WiFi settings");
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
    }
    lastButtonState = reading;
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

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
    if (month == 2) return isLeapYear(year) ? 29 : 28;
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

unsigned long convertToUnixTime(const char* dateTime) {
    int year, month, day, hour, minute, second;
    sscanf(dateTime, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

    unsigned long days = 0;
    for (int y = 1970; y < year; y++) {
        days += isLeapYear(y) ? 366 : 365;
    }
    for (int m = 1; m < month; m++) {
        days += daysInMonth(year, m);
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

void clearDepartureTimes() {
    NUM_DEPTIMES = 0;
}

void addDepartureTime(const char* time) {
    if (NUM_DEPTIMES < MAX_DEPTIMES) {
        departureTimes[NUM_DEPTIMES] = convertToUnixTime(time);
        NUM_DEPTIMES++;
    } else {
        Serial.println("Error: Maximum number of departure times reached");
    }
}

void updateLine(String oldStr, String newStr, int x, int y, int textSize) {
    tft.setTextSize(textSize);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    int charWidth = 6 * textSize;
    int oldLen = oldStr.length();
    int newLen = newStr.length();
    int maxLen = (oldLen > newLen) ? oldLen : newLen;

    for (int i = 0; i < maxLen; i++) {
        char oldChar = (i < oldLen) ? oldStr[i] : '\0';
        char newChar = (i < newLen) ? newStr[i] : '\0';

        if (newChar == '\0') {
            if (oldChar != '\0') {
                tft.setCursor(x + i*charWidth, y);
                tft.print(' ');
            }
        } else {
            if (oldChar != newChar) {
                tft.setCursor(x + i*charWidth, y);
                tft.print(newChar);
            }
        }
    }
}

void updateCenteredLine(String oldStr, String newStr, int centerY, int textSize) {
    tft.setTextSize(textSize);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    int charWidth = 6 * textSize;
    int newWidth = newStr.length() * charWidth;
    int x = (tft.width() - newWidth) / 2;

    int oldLen = oldStr.length();
    int newLen = newStr.length();
    int maxLen = (oldLen > newLen) ? oldLen : newLen;

    for (int i = 0; i < maxLen; i++) {
        char oldChar = (i < oldLen) ? oldStr[i] : '\0';
        char newChar = (i < newLen) ? newStr[i] : '\0';

        tft.setCursor(x + i*charWidth, centerY);
        if (newChar == '\0') {
            if (oldChar != '\0') {
                tft.print(' ');
            }
        } else {
            if (oldChar != newChar) {
                tft.print(newChar);
            }
        }
    }
}

void displayCountdown(long difference) {
    String newStr;
    if (difference < 0) {
        newStr = "--:--:--";
    } else {
        newStr = formatTime(difference);
    }

    int textSize = textSizeCountdown;
    int charWidth = 6 * textSize;
    int newWidth = newStr.length() * charWidth;
    int x = (tft.width() - newWidth) / 2;

    updateLine(previousCountdownStr, newStr, x, countdownY, textSizeCountdown);
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

    if (firstFutureIndex == -1 || firstFutureIndex+1 >= NUM_DEPTIMES) {
        newStr = "--:--:--";
    } else {
        unsigned long depTime = departureTimes[firstFutureIndex + 1];
        long diff = (long)depTime - (long)currentTime;
        if (diff <= 0) {
            newStr = "--:--:--";
        } else {
            newStr = formatTime(diff);
        }
    }

    int textSize = textSizeNext;
    int charWidth = 6 * textSize;
    int newWidth = newStr.length() * charWidth;
    int x = (tft.width() - newWidth) / 2;

    updateLine(previousNextStr, newStr, x, nextLineY, textSizeNext);
    previousNextStr = newStr;
}

void displayHeader() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Station -> Destination
    tft.setTextSize(2);
    String headerLine = currentDepartureStation + " -> " + currentDestinationName;
    int textSize = 2;
    int charWidth = 6 * textSize;
    int headerWidth = headerLine.length() * charWidth;
    int xCenter = (tft.width() - headerWidth) / 2;
    tft.setCursor(xCenter, headerStationY);
    tft.println(headerLine);

    // Transport Type
    tft.setTextSize(1);
    textSize = 1;
    charWidth = 6 * textSize;
    int transportWidth = currentTransportType.length() * charWidth;
    xCenter = (tft.width() - transportWidth) / 2;
    tft.setCursor(xCenter, headerTransportY);
    tft.println(currentTransportType);

    // Divider line
    tft.drawLine(0, dividerY, tft.width(), dividerY, TFT_WHITE);
}

void showFerryDepartureTime() {
    displayHeader();
    previousCountdownStr = "";
    previousNextStr = "";
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
    clearDepartureTimes();

    // First loop: just get the departure station from the first actual
    for (JsonPair item : actuals) {
        currentDepartureStation = item.value()["TimingPointName"].as<String>();
        break;
    }

    // Second loop: add all departure times
    for (JsonPair item : actuals) {
        const char* departureTime = item.value()["TargetDepartureTime"];
        if (departureTime && strlen(departureTime) > 0) {
            addDepartureTime(departureTime);
        }
    }

    if (NUM_DEPTIMES > 0) {
        std::sort(departureTimes, departureTimes + NUM_DEPTIMES);
        Serial.println("Parsed departure times:");
        for (int i = 0; i < NUM_DEPTIMES; i++) {
            Serial.println(departureTimes[i]);
        }
    } else {
        Serial.println("No departure times found in JSON");
    }

    showFerryDepartureTime();
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_2, INPUT_PULLUP);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
    }
    config.load();

    wifiManager.addParameter(&custom_gvb_line);
    wifiManager.addParameter(&custom_city);
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
    if (jsonString != "") {
        parseJSON(jsonString);
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.println("No data received...");
    }
}

void loop() {
    checkResetButton();
    timeClient.update();
    
    unsigned long currentMillis = millis();
    
    // Check if it's time to update JSON
    if (currentMillis - lastJsonUpdate >= JSON_UPDATE_INTERVAL) {
        lastJsonUpdate = currentMillis;
        updateJSON();
    }
    
    // Update display
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        if (jsonString != "") {
            currentTime = timeClient.getEpochTime() + 7200; // Adding 2 hours offset
            bool foundNextDeparture = false;
            long nextDiff = 0;
            
            // Debug current time
            Serial.print("Current time: ");
            Serial.println(currentTime);
            
            for (int i = 0; i < NUM_DEPTIMES; i++) {
                unsigned long depTime = departureTimes[i];
                // Debug departure time comparison
                Serial.print("Checking departure time: ");
                Serial.print(depTime);
                Serial.print(" Difference: ");
                Serial.println(depTime - currentTime);
                
                if (depTime > currentTime) {
                    nextDiff = depTime - currentTime;
                    foundNextDeparture = true;
                    break;
                }
            }
            
            if (foundNextDeparture) {
                displayCountdown(nextDiff);
                displayNextDeparture(currentTime);
            } else {
                // Check if we need to update JSON due to no future departures
                updateJSON();
                displayCountdown(-1);
                displayNextDeparture(currentTime);
            }
        }
    }
}