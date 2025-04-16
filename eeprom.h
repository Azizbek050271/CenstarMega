#ifndef EEPROM_H
#define EEPROM_H

#include <Arduino.h>
#include "fsm.h"

void writePriceToEEPROM(uint16_t price);
uint16_t readPriceFromEEPROM();
void saveTransactionState(uint32_t liters, uint32_t price, FSMState state, FuelMode mode, bool modeSelected);
bool restoreTransactionState(uint32_t* liters, uint32_t* price, FSMState* state, FuelMode* mode, bool* modeSelected);

#endif