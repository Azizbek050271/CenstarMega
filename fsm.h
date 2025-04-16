// fsm.h
#ifndef FSM_H
#define FSM_H

#include <Arduino.h>
#include "config.h"

typedef enum {
    FUEL_BY_VOLUME,
    FUEL_BY_PRICE,
    FUEL_BY_FULL_TANK
} FuelMode;

typedef enum {
    FSM_STATE_CHECK_STATUS,
    FSM_STATE_IDLE,
    FSM_STATE_WAIT_FOR_PRICE_INPUT,
    FSM_STATE_VIEW_PRICE,
    FSM_STATE_TRANSITION_PRICE_SET,
    FSM_STATE_EDIT_PRICE,
    FSM_STATE_TRANSITION_EDIT_PRICE,
    FSM_STATE_ERROR,
    FSM_STATE_TRANSACTION,
    FSM_STATE_TRANSACTION_END,
    FSM_STATE_TOTAL_COUNTER,
    FSM_STATE_TRANSACTION_PAUSED,
    FSM_STATE_CONFIRM_TRANSACTION
} FSMState;

struct FSMContext {
    FSMState state;
    FuelMode fuelMode;
    uint16_t price;
    bool priceValid;
    uint32_t transactionVolume;
    uint32_t transactionAmount;
    bool transactionStarted;
    bool waitingForResponse;
    int errorCount;
    int c0RetryCount;
    bool statusPollingActive;
    int monitorState;
    bool monitorActive;
    uint32_t currentLiters_dL;
    uint32_t finalLiters_dL;
    uint32_t currentPriceTotal;
    uint32_t finalPriceTotal;
    bool nozzleUpWarning;
    unsigned long stateEntryTime;
    unsigned long lastKeyTime;
    unsigned long lastC0SendTime;
    bool skipFirstStatusCheck;
    char priceInput[PRICE_FORMAT_LENGTH + 1];
    bool modeSelected; // Новый флаг
};

void initFSM(FSMContext* ctx);
void updateFSM(FSMContext* ctx);
void processKeyFSM(FSMContext* ctx, char key);
FSMState getCurrentState(const FSMContext* ctx);
FuelMode getCurrentFuelMode(const FSMContext* ctx);

#endif