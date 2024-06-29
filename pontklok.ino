#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TimeLib.h>
#include <algorithm>
#include <Wire.h>
#include <TAMC_GT911.h>
#include "config.h"

#define TOUCH_SDA  21
#define TOUCH_SCL  22
#define TOUCH_INT 39
#define TOUCH_RST 26
#define TOUCH_WIDTH  480
#define TOUCH_HEIGHT 320

// URL
const char* url = "http://v0.ovapi.nl/line/GVB_902_1"; // jouw JSON URL

HTTPClient http;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
TAMC_GT911 tp = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

// Initialize the display
TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

// Timers
TimerHandle_t jsonUpdateTimer;
TimerHandle_t countdownTimer;

unsigned long currentTime = 0;
unsigned long previousDiffrence = 0;

String jsonString = "";
String previousTimeStr = "";

const int MAX_DEPTIMES = 200;
int NUM_DEPTIMES = 0;
unsigned long departureTimes[MAX_DEPTIMES];

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Verbinding maken met WiFi...");
    }

    timeClient.begin();
    Serial.println("WiFi verbonden.");

        tp.begin();
    tp.setRotation(ROTATION_NORMAL);

    // Initialize the display
    tft.init();
    tft.setRotation(3); // Set rotation if needed
        
    // Clear the screen initially
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    
    // Display "pont" label
    tft.setCursor(30, 30);
    tft.setTextSize(3);
    tft.println("IJplein Pont");

    // Stel een timer in om elke minuut de JSON te updaten
    jsonUpdateTimer = xTimerCreate("JSONUpdateTimer", pdMS_TO_TICKS(60000), pdTRUE, (void *)0, JSONTimerCallback);
    if (jsonUpdateTimer != NULL) {
        xTimerStart(jsonUpdateTimer, 0);
        Serial.println("Timer gestart voor periodieke JSON-update.");
    } else {
        Serial.println("Fout bij het maken van JSON update timer!");
    }

    displayTime("00:00:00");
    updateJSON();

    // Stel een timer in om elke seconde af te tellen tot het volgende vertrek
    countdownTimer = xTimerCreate("CountdownTimer", pdMS_TO_TICKS(50), pdTRUE, (void *)0, CountdownTimerCallback);
    if (countdownTimer != NULL) {
        xTimerStart(countdownTimer, 0);
        Serial.println("Timer gestart voor secondenteller.");
    } else {
        Serial.println("Fout bij het maken van countdown timer!");
    }
}

void loop() {
    // Jouw code hier
    timeClient.update();

    tp.read();
    if (tp.isTouched){
      for (int i=0; i<tp.touches; i++){
        Serial.print("Touch ");Serial.print(i+1);Serial.print(": ");;
        Serial.print("  x: ");Serial.print(tp.points[i].x);
        Serial.print("  y: ");Serial.print(tp.points[i].y);
        Serial.print("  size: ");Serial.println(tp.points[i].size);
        Serial.println(' ');
      }
    }
      
      // Add your code to handle the touch here
      
      delay(100); // Simple debounce delay
}

void JSONTimerCallback(TimerHandle_t xTimer) {
    updateJSON(); // Roep updateJSON() op bij elke timer-trigger
}

void updateJSON() {
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        jsonString = http.getString();
    } else {
        Serial.print("Fout bij het ophalen van JSON. Foutcode: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void parseJSON(String jsonString) {
    StaticJsonDocument<4096> doc; // Pas de grootte aan op basis van de grootte van je JSON
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        Serial.print("Fout bij het parsen van JSON: ");
        Serial.println(error.c_str());
        return;
    }

    JsonObject actuals = doc["GVB_902_1"]["Actuals"]; // Pak het JsonObject 'Actuals'
    clearDepartureTimes();

    for (JsonPair item : actuals) {
        const char* departureTime = item.value()["TargetDepartureTime"]; // Haal 'TargetDepartureTime' op

        addDepartureTime(departureTime);
        std::sort(departureTimes, departureTimes + NUM_DEPTIMES);
    }

    printStrings();
}

void CountdownTimerCallback(TimerHandle_t xTimer) {
    if(jsonString != ""){
        parseJSON(jsonString);
    }
}

void addDepartureTime(const char* time) {
  if (NUM_DEPTIMES < MAX_DEPTIMES) {
    departureTimes[NUM_DEPTIMES] = convertToUnixTime(time);
    NUM_DEPTIMES++;
  } else {
    // Als de array vol is, kun je hier een foutmelding of alternatieve logica toevoegen
    Serial.println("Error: Maximum number of strings reached");
  }
}

void clearDepartureTimes() {
  NUM_DEPTIMES = 0; // Zet het aantal strings terug naar nul
}

void printStrings() {
  if(NUM_DEPTIMES > 0){
    currentTime = timeClient.getEpochTime() + 7200;
    for(long depTime : departureTimes){
      if(depTime > currentTime){
        long diffrence = depTime - currentTime;
        if(previousDiffrence != diffrence){
          previousDiffrence = diffrence;
          Serial.println(formatTime(diffrence));
          displayTime(formatTime(diffrence));
        }
        return;
      }
    }
  }
}

void sortDepartureTimes() {
    for (int i = 1; i < NUM_DEPTIMES; i++) {
        unsigned long key = departureTimes[i];
        int j = i - 1;
        while (j >= 0 && departureTimes[j] > key) {
            departureTimes[j + 1] = departureTimes[j];
            j = j - 1;
        }
        departureTimes[j + 1] = key;
    }
}