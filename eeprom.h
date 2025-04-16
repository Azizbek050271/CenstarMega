// eeprom.h
#ifndef EEPROM_H
#define EEPROM_H

#include <Arduino.h>
#include "fsm.h" // Добавлено для FSMState

uint16_t readPriceFromEEPROM();
void writePriceToEEPROM(uint16_t price);
void saveTransactionState(uint32_t liters, uint32_t price, FSMState state);
bool restoreTransactionState(uint32_t* liters, uint32_t* price, FSMState* state);

#endif