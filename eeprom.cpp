// eeprom.cpp
#include "eeprom.h"
#include <EEPROM.h>
#include "config.h"

#define PRICE_EEPROM_ADDRESS 0
#define TRANSACTION_LITERS_ADDRESS 4
#define TRANSACTION_PRICE_ADDRESS 8
#define TRANSACTION_STATE_ADDRESS 12

uint16_t readPriceFromEEPROM() {
    uint16_t price = 0;
    price = EEPROM.read(PRICE_EEPROM_ADDRESS);
    price |= ((uint16_t)EEPROM.read(PRICE_EEPROM_ADDRESS + 1)) << 8;
    if (price > 99999) price = 0; // Ограничение до 99999
    return price;
}

void writePriceToEEPROM(uint16_t price) {
    EEPROM.write(PRICE_EEPROM_ADDRESS, lowByte(price));
    EEPROM.write(PRICE_EEPROM_ADDRESS + 1, highByte(price));
}

void saveTransactionState(uint32_t liters, uint32_t price, FSMState state) {
    EEPROM.put(TRANSACTION_LITERS_ADDRESS, liters);
    EEPROM.put(TRANSACTION_PRICE_ADDRESS, price);
    EEPROM.write(TRANSACTION_STATE_ADDRESS, (uint8_t)state);
}

bool restoreTransactionState(uint32_t* liters, uint32_t* price, FSMState* state) {
    uint8_t savedState;
    EEPROM.get(TRANSACTION_LITERS_ADDRESS, *liters);
    EEPROM.get(TRANSACTION_PRICE_ADDRESS, *price);
    savedState = EEPROM.read(TRANSACTION_STATE_ADDRESS);
    if (savedState <= FSM_STATE_TOTAL_COUNTER) {
        *state = (FSMState)savedState;
        return true;
    }
    return false;
}