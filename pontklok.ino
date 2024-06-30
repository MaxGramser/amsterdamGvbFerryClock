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

#define TOUCH_SDA  21
#define TOUCH_SCL  22
#define TOUCH_INT 39
#define TOUCH_RST 26
#define TOUCH_WIDTH  480
#define TOUCH_HEIGHT 320

// URL
const String url = "http://v0.ovapi.nl/line/" + GVB_LINE;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Initialize the display
TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

// Timers
TimerHandle_t jsonUpdateTimer;
TimerHandle_t countdownTimer;

unsigned long currentTime = 0;
unsigned long previousDiffrence = 0;

String jsonString = "";
String previousTimeStr = "";

const int MAX_DEPTIMES = 300;
int NUM_DEPTIMES = 0;
unsigned long departureTimes[MAX_DEPTIMES];

// updatescreen
unsigned long previousMillis = 0;
const long interval = 50;

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Verbinding maken met WiFi...");
    }
    
    Serial.println("WiFi verbonden.");
    timeClient.begin();

    // Initialize the display
    tft.init();
    tft.setRotation(3); // Set rotation if needed
        
    // Clear the screen initially
    tft.fillScreen(TFT_BLACK);

    // Stel een timer in om elke minuut de JSON te updaten
    jsonUpdateTimer = xTimerCreate("JSONUpdateTimer", pdMS_TO_TICKS(60000), pdTRUE, (void *)0, JSONTimerCallback);
    if (jsonUpdateTimer != NULL) {
        xTimerStart(jsonUpdateTimer, 0);
        Serial.println("Timer gestart voor periodieke JSON-update.");
    } else {
        Serial.println("Fout bij het maken van JSON update timer!");
    }

    displayTime("00:00:00", "IJplein Pont", 1);
    updateJSON();
}

void loop() {
    // Jouw code hier
    timeClient.update();
    
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      
      if(jsonString != ""){
          parseJSON(jsonString);
      }
    }
}

void JSONTimerCallback(TimerHandle_t xTimer) {
    updateJSON(); // Roep updateJSON() op bij elke timer-trigger
}

void updateJSON() {
    HTTPClient http;
    Serial.println("Updating JSON");
    http.begin(String(url.c_str()));
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        jsonString = http.getString();
    } else {
        Serial.print("Fout bij het ophalen van JSON. Foutcode: ");
        Serial.println(httpResponseCode);
    }
    http.end();
    Serial.println("JSON updated");
}

void parseJSON(String jsonString) {
    StaticJsonDocument<4096> doc; // Pas de grootte aan op basis van de grootte van je JSON
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        Serial.print("Fout bij het parsen van JSON: ");
        Serial.println(error.c_str());
        return;
    }

    JsonObject actuals = doc[GVB_LINE]["Actuals"]; // Pak het JsonObject 'Actuals'
    clearDepartureTimes();

    for (JsonPair item : actuals) {
        const char* departureTime = item.value()["TargetDepartureTime"]; // Haal 'TargetDepartureTime' op
        addDepartureTime(departureTime);
        std::sort(departureTimes, departureTimes + NUM_DEPTIMES);
    }
    showFerryDepartureTime();
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

void showFerryDepartureTime() {
  if(NUM_DEPTIMES > 0){
    currentTime = timeClient.getEpochTime() + 7200;
    for(long depTime : departureTimes){
      if(depTime > currentTime){
        long diffrence = depTime - currentTime;
        if(previousDiffrence != diffrence){
          previousDiffrence = diffrence;
          displayTime(formatTime(diffrence), "IJplein Pont", 1);
        }
        return;
      }
    }
  }
}