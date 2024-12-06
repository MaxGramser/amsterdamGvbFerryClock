// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <FS.h>
#include <SPIFFS.h>

// Default GVB line and city if not configured
#define DEFAULT_GVB_LINE "GVB_902_1"
#define DEFAULT_CITY "Amsterdam"

struct Config {
    String gvb_line;
    String city; // Add this line

    void load() {
        if (SPIFFS.begin(true)) {
            if (SPIFFS.exists("/config.txt")) {
                File configFile = SPIFFS.open("/config.txt", "r");
                if (configFile) {
                    gvb_line = configFile.readStringUntil('\n');
                    gvb_line.trim();
                    if (configFile.available()) {
                        city = configFile.readStringUntil('\n');
                        city.trim();
                    } else {
                        city = DEFAULT_CITY;
                    }
                    configFile.close();
                }
            } else {
                gvb_line = DEFAULT_GVB_LINE;
                city = DEFAULT_CITY;
                save();
            }
        }
    }

    void save() {
        File configFile = SPIFFS.open("/config.txt", "w");
        if (configFile) {
            configFile.println(gvb_line);
            configFile.println(city);
            configFile.close();
        }
    }
};

extern Config config;

#endif
