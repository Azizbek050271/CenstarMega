#include <Arduino.h>
#include "config.h"
#include "fsm.h"
#include "keypad.h"
#include "oled.h"
#include "rs422.h"

static FSMContext fsmContext;

void setup() {
    Serial.begin(9600);
    initOLED();
    initRS422();
    initFSM(&fsmContext);
    displayMessage("CENSTAR");
    unsigned long welcomeStart = millis();
    while (millis() - welcomeStart < DISPLAY_WELCOME_DURATION) {
    }
}

void loop() {
    char key = getKeypadKey();
    if (key) {
        processKeyFSM(&fsmContext, key);
    }
    updateFSM(&fsmContext);
}