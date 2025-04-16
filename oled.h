#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <U8g2lib.h>

void initOLED();
bool displayMessage(const char* msg);

#endif
