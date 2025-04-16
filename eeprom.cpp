#include "eeprom.h"
#include <EEPROM.h>

#define EEPROM_PRICE_ADDR 0
#define EEPROM_LITERS_ADDR 4
#define EEPROM_PRICE_TOTAL_ADDR 8
#define EEPROM_STATE_ADDR 12
#define EEPROM_MODE_ADDR 14
#define EEPROM_MODE_SELECTED_ADDR 15

void writePriceToEEPROM(uint16_t price) {
    EEPROM.put(EEPROM_PRICE_ADDR, price);
}

uint16_t readPriceFromEEPROM() {
    uint16_t price;
    EEPROM.get(EEPROM_PRICE_ADDR, price);
    return price;
}

void saveTransactionState(uint32_t liters, uint32_t price, FSMState state, FuelMode mode, bool modeSelected) {
    EEPROM.put(EEPROM_LITERS_ADDR, liters);
    EEPROM.put(EEPROM_PRICE_TOTAL_ADDR, price);
    EEPROM.put(EEPROM_STATE_ADDR, (uint8_t)state);
    EEPROM.put(EEPROM_MODE_ADDR, (uint8_t)mode);
    EEPROM.put(EEPROM_MODE_SELECTED_ADDR, (uint8_t)modeSelected);
}

bool restoreTransactionState(uint32_t* liters, uint32_t* price, FSMState* state, FuelMode* mode, bool* modeSelected) {
    uint8_t savedState;
    EEPROM.get(EEPROM_LITERS_ADDR, *liters);
    EEPROM.get(EEPROM_PRICE_TOTAL_ADDR, *price);
    EEPROM.get(EEPROM_STATE_ADDR, savedState);
    *state = (FSMState)savedState;
    uint8_t savedMode;
    EEPROM.get(EEPROM_MODE_ADDR, savedMode);
    *mode = (FuelMode)savedMode;
    uint8_t savedModeSelected;
    EEPROM.get(EEPROM_MODE_SELECTED_ADDR, savedModeSelected);
    *modeSelected = (bool)savedModeSelected;
    return (*liters != 0xFFFFFFFF && *price != 0xFFFFFFFF && savedState != 0xFF);
}