// pontklok.ino
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

// Timers
TimerHandle_t jsonUpdateTimer;

unsigned long currentTime = 0;
unsigned long previousDifference = 0;

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
const long interval = 1000; // Update every second

WiFiManagerParameter custom_gvb_line("gvb_line", "GVB Line (e.g., GVB_902_1)", DEFAULT_GVB_LINE, 40);
WiFiManagerParameter custom_city("city", "City (e.g., Amsterdam)", DEFAULT_CITY, 40);

String currentDepartureStation = "";
String currentDestinationName = "";
String currentTransportType = "";

// UI Positions & Sizes (Apple-inspired minimal UI)
int headerStationY = 50;
int headerTransportY = 80;
int dividerY = 100;

// Countdown and next line positions
int countdownY = 140; 
int textSizeCountdown = 8;

int textSizeNext = 2;
int nextLineY = 230; // Will center horizontally

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

void JSONTimerCallback(TimerHandle_t xTimer) {
    updateJSON();
}

void updateJSON() {
    // Update public transport data
    HTTPClient http;
    String url = "http://v0.ovapi.nl/line/" + config.gvb_line;
    Serial.println("Updating JSON from: " + url);
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        jsonString = http.getString();
    } else {
        Serial.print("Error fetching JSON. Error code: ");
        Serial.println(httpResponseCode);
    }
    http.end();
    Serial.println("JSON updated");
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

// Generic line update function
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

// Centered line update function
void updateCenteredLine(String oldStr, String newStr, int centerY, int textSize) {
    tft.setTextSize(textSize);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    int charWidth = 6 * textSize;
    int newWidth = newStr.length() * charWidth;
    int x = (tft.width() - newWidth) / 2;

    // Overwrite old characters if needed
    int oldLen = oldStr.length();
    int newLen = newStr.length();
    int maxLen = (oldLen > newLen) ? oldLen : newLen;

    for (int i = 0; i < maxLen; i++) {
        char oldChar = (i < oldLen) ? oldStr[i] : '\0';
        char newChar = (i < newLen) ? newStr[i] : '\0';

        tft.setCursor(x + i*charWidth, centerY);
        if (newChar == '\0') {
            // Clear old char
            if (oldChar != '\0') {
                tft.print(' ');
            }
        } else {
            if (oldChar != newChar) {
                tft.print(newChar);
            }
        }
    }
    // Update reference string
    oldStr = newStr;
}

void displayCountdown(long difference) {
    String newStr;
    if (difference < 0) {
        newStr = "--:--:--";
    } else {
        newStr = formatTime(difference);
    }

    // Center the countdown horizontally
    // Calculate width
    int textSize = textSizeCountdown;
    int charWidth = 6 * textSize;
    int newWidth = newStr.length() * charWidth;
    int x = (tft.width() - newWidth) / 2;

    updateLine(previousCountdownStr, newStr, x, countdownY, textSizeCountdown);
    previousCountdownStr = newStr;
}

void displayNextDeparture(unsigned long currentTime) {
    String newStr;
    // Find first future departure
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
            newStr = "" + formatTime(diff);
        }
    }

    // Center the "Next:" line as well
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

    currentDepartureStation = "";
    for (JsonPair item : actuals) {
        currentDepartureStation = item.value()["TimingPointName"].as<String>();
        break;
    }

    for (JsonPair item : actuals) {
        const char* departureTime = item.value()["TargetDepartureTime"];
        addDepartureTime(departureTime);
    }

    std::sort(departureTimes, departureTimes + NUM_DEPTIMES);

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
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 10);
    tft.println("Loading... Please wait");

    timeClient.begin();

    jsonUpdateTimer = xTimerCreate("JSONUpdateTimer", pdMS_TO_TICKS(60000), pdTRUE, (void *)0, JSONTimerCallback);
    if (jsonUpdateTimer != NULL) {
        xTimerStart(jsonUpdateTimer, 0);
        Serial.println("JSON update timer started");
    } else {
        Serial.println("Error creating JSON update timer!");
    }

    updateJSON();
    if(jsonString != "") {
      parseJSON(jsonString);
    } else {
      tft.fillScreen(TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(10,10);
      tft.println("No data received...");
    }
}

void loop() {
    checkResetButton();
    timeClient.update();

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        if (jsonString != "") {
            currentTime = timeClient.getEpochTime() + 7200;
            bool foundNextDeparture = false;
            long nextDiff = 0;

            for (int i = 0; i < NUM_DEPTIMES; i++) {
                unsigned long depTime = departureTimes[i];
                if (depTime > currentTime) {
                    long difference = depTime - currentTime;
                    nextDiff = difference;
                    foundNextDeparture = true;
                    break;
                }
            }

            if (foundNextDeparture) {
                if (previousDifference != nextDiff) {
                    previousDifference = nextDiff;
                    displayCountdown(nextDiff);
                } else {
                    displayCountdown(nextDiff);
                }
                displayNextDeparture(currentTime);
            } else if (NUM_DEPTIMES > 0) {
                // No future departure
                displayCountdown(-1); 
                String noMore = "--:--:--";
                // Center this line as well
                int textSize = textSizeNext;
                int charWidth = 6 * textSize;
                int width = noMore.length() * charWidth;
                int x = (tft.width() - width) / 2;
                updateLine(previousNextStr, noMore, x, nextLineY, textSizeNext);
                previousNextStr = noMore;
            } else {
                // No departure info
                displayCountdown(-1);
                String noInfo = "--:--:--";
                int textSize = textSizeNext;
                int charWidth = 6 * textSize;
                int width = noInfo.length() * charWidth;
                int x = (tft.width() - width) / 2;
                updateLine(previousNextStr, noInfo, x, nextLineY, textSizeNext);
                previousNextStr = noInfo;
            }
        }
    }
}
