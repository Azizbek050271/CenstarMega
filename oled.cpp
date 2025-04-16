// oled.cpp
#include "oled.h"
#include "config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void initOLED() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("OLED initialization error. Continuing without display.");
        return;
    }
    display.clearDisplay();
    display.display();
}

bool displayMessage(const char* msg) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(msg);
    display.display();
    return true;
}