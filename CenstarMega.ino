// CenstarMega.ino
#include <Arduino.h>
#include "config.h"
#include "fsm.h"
#include "keypad.h"
#include "oled.h"
#include "rs422.h"

static FSMContext fsmContext;
static unsigned long welcomeUntil = 0;
static bool welcomeShown = false;

void setup() {
    Serial.begin(9600); // Оставляем 9600, как в оригинале
    initOLED();
    initRS422();
    initFSM(&fsmContext);
    displayMessage("CENSTAR");
    welcomeUntil = millis() + DISPLAY_WELCOME_DURATION;
    welcomeShown = true;
}

void loop() {
    // Неблокирующее завершение приветствия
    if (welcomeShown && millis() >= welcomeUntil) {
        welcomeShown = false;
        if (fsmContext.priceValid) {
            displayMessage("Please select mode"); // Сразу после CENSTAR
        } else {
            displayMessage("Set price (0-99999)");
        }
    }

    if (!welcomeShown) {
        char key = getKeypadKey();
        if (key) {
            processKeyFSM(&fsmContext, key);
        }
        updateFSM(&fsmContext);
    }
}