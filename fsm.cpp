// fsm.cpp
#include "fsm.h"
#include "config.h"
#include "eeprom.h"
#include "oled.h"
#include "rs422.h"
#include "crc.h"

/* Вспомогательные функции форматирования */
static void formatLiters(uint32_t dl, char* dst, size_t dstLen) {
    uint32_t intPart = dl / 100;
    uint32_t fracPart = dl % 100;
    snprintf(dst, dstLen, "%lu.%02lu", (unsigned long)intPart, (unsigned long)fracPart);
}

static void displayFuelMode(FuelMode mode) {
    switch (mode) {
        case FUEL_BY_VOLUME:    displayMessage("Mode: Volume");     break;
        case FUEL_BY_PRICE:     displayMessage("Mode: Price");      break;
        case FUEL_BY_FULL_TANK: displayMessage("Mode: Full Tank");  break;
    }
}

static void displayTransaction(uint32_t liters, uint32_t price, const char* status, bool priceScaled) {
    char litersBuf[12];
    formatLiters(liters, litersBuf, sizeof(litersBuf));
    char displayStr[48];
    uint32_t displayPrice = priceScaled ? price * 10 : price; // Умножаем сумму на 10, если цена > 9999
    snprintf(displayStr, sizeof(displayStr), "%s\nL: %s\nP: %lu", status, litersBuf, (unsigned long)displayPrice);
    displayMessage(displayStr);
}

/* Обработка ответов ТРК */
static bool handleResponse(uint8_t* buffer, int length, int expected, FSMContext* ctx) {
    if (length >= expected) {
        ctx->waitingForResponse = false;
        ctx->errorCount = 0;
        return true;
    }
    ctx->waitingForResponse = false;
    ctx->errorCount++;
    if (ctx->errorCount >= MAX_ERROR_COUNT) {
        ctx->state = FSM_STATE_ERROR;
        ctx->stateEntryTime = millis();
        displayMessage("Pump Error");
    }
    return false;
}

/* Таблица статусов ТРК */
struct StatusAction {
    char code[2];
    FSMState nextState;
    bool resetErrorCount;
    const char* warning;
};

static const StatusAction statusActions[] = {
    {{'1', '0'}, FSM_STATE_IDLE, true, nullptr},
    {{'2', '1'}, FSM_STATE_IDLE, true, "Nozzle up! Please hang up."},
    {{'3', '1'}, FSM_STATE_TRANSACTION, false, nullptr},
    {{'4', '1'}, FSM_STATE_TRANSACTION, false, nullptr},
    {{'6', '1'}, FSM_STATE_TRANSACTION, false, nullptr},
    {{'7', '1'}, FSM_STATE_TRANSACTION_PAUSED, false, nullptr},
    {{'8', '1'}, FSM_STATE_TRANSACTION_END, true, nullptr},
    {{'9', '0'}, FSM_STATE_IDLE, true, nullptr}
};

static bool isValidStatus(uint8_t* buffer) {
    for (size_t i = 0; i < sizeof(statusActions) / sizeof(statusActions[0]); i++) {
        if (buffer[4] == statusActions[i].code[0] && buffer[5] == statusActions[i].code[1]) {
            return true;
        }
    }
    return false;
}

/* Обновление состояний FSM */
static void updateCheckStatus(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    static unsigned long lastResponseTime = 0;
    static unsigned long nozzleUpStartTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
    } else {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                nozzleUpStartTime = 0;
            } else if (respBuffer[4] == '1' && respBuffer[5] == '0') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->nozzleUpWarning = false;
                displayFuelMode(ctx->fuelMode);
                nozzleUpStartTime = 0;
            } else if (respBuffer[4] == '2' && respBuffer[5] == '1') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                ctx->nozzleUpWarning = true;
                if (nozzleUpStartTime == 0) {
                    nozzleUpStartTime = currentMillis;
                }
                if (currentMillis - nozzleUpStartTime > 60000) {
                    ctx->state = FSM_STATE_ERROR;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Nozzle up too long! Check pump");
                } else {
                    displayMessage("Nozzle up! Please hang up.");
                }
            } else if (respBuffer[4] == '7' && respBuffer[5] == '1') {
                ctx->state = FSM_STATE_TRANSACTION_PAUSED;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
            } else if (respBuffer[4] == '6' && respBuffer[5] == '1') {
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 1;
                ctx->transactionStarted = true;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
            } else {
                ctx->errorCount++;
                if (ctx->errorCount >= MAX_ERROR_COUNT) {
                    ctx->state = FSM_STATE_ERROR;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Pump Error");
                }
            }
        }
    }
}

static void updateError(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (currentMillis - ctx->stateEntryTime >= RESPONSE_TIMEOUT) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
        ctx->stateEntryTime = currentMillis;
        displayMessage("Pump offline! Check connection");
    }
    if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
            } else if (respBuffer[4] == '1' && respBuffer[5] == '0') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->nozzleUpWarning = false;
                ctx->transactionStarted = false;
                ctx->monitorActive = false;
                ctx->monitorState = 0;
                ctx->waitingForResponse = false;
                displayFuelMode(ctx->fuelMode);
            } else if (respBuffer[4] == '2' && respBuffer[5] == '1') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                ctx->nozzleUpWarning = true;
                displayMessage("Nozzle up! Please hang up.");
            } else if (respBuffer[4] == '7' && respBuffer[5] == '1') {
                ctx->state = FSM_STATE_TRANSACTION_PAUSED;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
            } else {
                ctx->state = FSM_STATE_CHECK_STATUS;
                ctx->stateEntryTime = currentMillis;
                ctx->waitingForResponse = false;
            }
        }
    }
}

static void updateIdle(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    static unsigned long lastResponseTime = 0;
    static unsigned long nozzleUpStartTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (ctx->skipFirstStatusCheck) {
        ctx->skipFirstStatusCheck = false;
        ctx->transactionStarted = false;
        ctx->monitorActive = false;
        ctx->monitorState = 0;
        ctx->waitingForResponse = false;
        return;
    }
    if (ctx->statusPollingActive && !ctx->waitingForResponse) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
    } else if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                nozzleUpStartTime = 0;
            } else if (respBuffer[4] == '1' && respBuffer[5] == '0' || respBuffer[4] == '9' && respBuffer[5] == '0') {
                ctx->nozzleUpWarning = false;
                displayFuelMode(ctx->fuelMode);
                nozzleUpStartTime = 0;
            } else if (respBuffer[4] == '2' && respBuffer[5] == '1') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                ctx->nozzleUpWarning = true;
                if (nozzleUpStartTime == 0) {
                    nozzleUpStartTime = currentMillis;
                }
                if (currentMillis - nozzleUpStartTime > 60000) {
                    ctx->state = FSM_STATE_ERROR;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Nozzle up too long! Check pump");
                } else {
                    displayMessage("Nozzle up! Please hang up.");
                }
            } else {
                ctx->errorCount++;
                if (ctx->errorCount >= MAX_ERROR_COUNT) {
                    ctx->state = FSM_STATE_ERROR;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Pump Error");
                }
            }
        }
    }
}

static void updateWaitForPriceInput(FSMContext* ctx) {
    // Пустая реализация, так как обработка в processKeyFSM
}

static void updateViewPrice(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    if (currentMillis - ctx->stateEntryTime >= 10000) {
        ctx->state = FSM_STATE_IDLE;
        ctx->stateEntryTime = currentMillis;
        if (!ctx->nozzleUpWarning) {
            displayFuelMode(ctx->fuelMode);
        }
    }
}

static void updateTransitionPriceSet(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    if (currentMillis - ctx->stateEntryTime >= TRANSITION_TIMEOUT) {
        ctx->waitingForResponse = false;
        ctx->state = FSM_STATE_CHECK_STATUS;
        ctx->stateEntryTime = currentMillis;
    }
}

static void updateTransitionEditPrice(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    if (currentMillis - ctx->stateEntryTime >= TRANSITION_TIMEOUT) {
        ctx->waitingForResponse = false;
        ctx->state = FSM_STATE_IDLE;
        ctx->stateEntryTime = currentMillis;
        if (!ctx->nozzleUpWarning) {
            displayFuelMode(ctx->fuelMode);
        }
    }
}

static void updateEditPrice(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    if (currentMillis - ctx->stateEntryTime >= EDIT_TIMEOUT) {
        ctx->state = FSM_STATE_IDLE;
        ctx->stateEntryTime = currentMillis;
        if (!ctx->nozzleUpWarning) {
            displayFuelMode(ctx->fuelMode);
        }
    }
}

static void updateConfirmTransaction(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    if (currentMillis - ctx->stateEntryTime >= 10000) {
        ctx->state = FSM_STATE_IDLE;
        ctx->stateEntryTime = currentMillis;
        if (!ctx->nozzleUpWarning) {
            displayFuelMode(ctx->fuelMode);
        }
    }
}

static void updateTransaction(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse) {
        if (!ctx->transactionStarted) {
            rs422SendStatus();
            ctx->waitingForResponse = true;
        } else {
            if (!ctx->monitorActive) {
                rs422SendStatus();
                ctx->waitingForResponse = true;
            } else {
                switch (ctx->monitorState) {
                    case 0: rs422SendStatus(); break;
                    case 1: rs422SendLitersMonitor(); break;
                    case 2: rs422SendRevenueStatus(); break;
                }
                ctx->waitingForResponse = true;
            }
        }
    } else {
        uint8_t respBuffer[32] = {0};
        int expectedLength = ctx->monitorActive ? (ctx->monitorState == 0 ? STATUS_RESPONSE_LENGTH : MONITOR_RESPONSE_LENGTH) : STATUS_RESPONSE_LENGTH;
        char expectedCommand = ctx->monitorState == 0 ? 'S' : (ctx->monitorState == 1 ? 'L' : 'R');
        int respLength = rs422WaitForResponse(respBuffer, expectedLength, expectedCommand);
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (!ctx->transactionStarted && respBuffer[4] == '1' && respBuffer[5] == '0') {
                ctx->waitingForResponse = false;
                displayMessage("Lift nozzle to start!");
                return;
            }
            if (!ctx->transactionStarted && (respBuffer[4] == '2' || respBuffer[4] == '1')) {
                uint16_t protocolPrice = ctx->price > 9999 ? ctx->price / 10 : ctx->price;
                rs422SendTransaction(ctx->fuelMode, ctx->transactionVolume, ctx->transactionAmount, protocolPrice);
                ctx->waitingForResponse = true;
                ctx->transactionStarted = true;
                ctx->currentLiters_dL = 0;
                ctx->currentPriceTotal = 0;
                ctx->errorCount = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
            } else if (ctx->monitorState == 0) {
                if (isValidStatus(respBuffer)) {
                    for (size_t i = 0; i < sizeof(statusActions) / sizeof(statusActions[0]); i++) {
                        if (respBuffer[4] == statusActions[i].code[0] && respBuffer[5] == statusActions[i].code[1]) {
                            ctx->state = statusActions[i].nextState;
                            ctx->stateEntryTime = currentMillis;
                            if (statusActions[i].resetErrorCount) ctx->errorCount = 0;
                            if (statusActions[i].nextState == FSM_STATE_TRANSACTION_END) {
                                rs422SendTransactionUpdate();
                                ctx->waitingForResponse = true;
                                if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                                    rs422SendNozzleOff();
                                }
                                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Transaction stopped", ctx->price > 9999);
                            } else if (statusActions[i].nextState == FSM_STATE_TRANSACTION_PAUSED) {
                                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
                            } else if (statusActions[i].nextState == FSM_STATE_TRANSACTION && respBuffer[4] == '6' && respBuffer[5] == '1') {
                                ctx->monitorActive = true;
                                ctx->monitorState = 1;
                            }
                            break;
                        }
                    }
                }
            } else if (ctx->monitorActive) {
                if (ctx->monitorState == 1 && respBuffer[3] == 'L' && respBuffer[4] == '1') {
                    char litersStr[7] = {0};
                    if (respLength >= 14) {
                        memcpy(litersStr, respBuffer + 8, 6);
                        bool valid = true;
                        for (int i = 0; i < 6; i++) {
                            if (litersStr[i] < '0' || litersStr[i] > '9') {
                                valid = false;
                                break;
                            }
                        }
                        ctx->currentLiters_dL = valid ? atol(litersStr) : 0;
                        displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
                        saveTransactionState(ctx->currentLiters_dL, ctx->currentPriceTotal, ctx->state);
                    } else {
                        ctx->currentLiters_dL = 0;
                    }
                    ctx->monitorState = 2;
                } else if (ctx->monitorState == 2 && respBuffer[3] == 'R' && respBuffer[4] == '1') {
                    char priceStr[7] = {0};
                    if (respLength >= 14) {
                        memcpy(priceStr, respBuffer + 8, 6);
                        bool valid = true;
                        for (int i = 0; i < 6; i++) {
                            if (priceStr[i] < '0' || priceStr[i] > '9') {
                                valid = false;
                                break;
                            }
                        }
                        ctx->currentPriceTotal = valid ? atol(priceStr) : 0;
                        displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
                        saveTransactionState(ctx->currentLiters_dL, ctx->currentPriceTotal, ctx->state);
                    } else {
                        ctx->currentPriceTotal = 0;
                    }
                    ctx->monitorState = 0;
                }
            }
        }
    }
}

static void updateTransactionPaused(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (currentMillis - ctx->stateEntryTime > 30000) {
        ctx->finalLiters_dL = ctx->currentLiters_dL;
        ctx->finalPriceTotal = ctx->currentPriceTotal;
        rs422SendTransactionUpdate();
        ctx->waitingForResponse = true;
        ctx->state = FSM_STATE_TRANSACTION_END;
        ctx->stateEntryTime = currentMillis;
        displayMessage("Nozzle returned! Transaction ended");
        return;
    }

    if (!ctx->waitingForResponse) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
    } else {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                ctx->finalLiters_dL = ctx->currentLiters_dL;
                ctx->finalPriceTotal = ctx->currentPriceTotal;
                rs422SendTransactionUpdate();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION_END;
                ctx->stateEntryTime = currentMillis;
            } else if (respBuffer[4] != '7' || respBuffer[5] != '1') {
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
            }
        }
    }
}

static void updateTransactionEnd(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    static unsigned long lastResponseTime = 0;
    static bool dataReceived = false;
    static uint8_t retryCount = 0;

    if (ctx->stateEntryTime == currentMillis) {
        dataReceived = false;
        retryCount = 0;
    }

    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse && !dataReceived && retryCount < 3) {
        rs422SendTransactionUpdate();
        ctx->waitingForResponse = true;
        retryCount++;
    } else if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, TRANSACTION_END_RESPONSE_LENGTH, 'T');
        if (respLength >= 18) {
            ctx->waitingForResponse = false;
            ctx->errorCount = 0;
            if (respBuffer[3] == 'T' && respBuffer[4] == '1') {
                char priceStr[7] = {0};
                char litersStr[7] = {0};
                int offset = (respBuffer[5] == 'u') ? 10 : 8;
                memcpy(priceStr, respBuffer + offset, 6);
                memcpy(litersStr, respBuffer + offset + 7, 6);
                bool valid = true;
                for (int i = 0; i < 6; i++) {
                    if (litersStr[i] < '0' || litersStr[i] > '9' || priceStr[i] < '0' || priceStr[i] > '9') {
                        valid = false;
                        break;
                    }
                }
                ctx->finalLiters_dL = valid ? atol(litersStr) : ctx->finalLiters_dL;
                ctx->finalPriceTotal = valid ? atol(priceStr) : ctx->finalPriceTotal;

                displayTransaction(ctx->finalLiters_dL, ctx->finalPriceTotal, "Filling end:", ctx->price > 9999);
                rs422SendNozzleOff();
                ctx->waitingForResponse = false;
                dataReceived = true;
                retryCount = 0;
                saveTransactionState(ctx->finalLiters_dL, ctx->finalPriceTotal, ctx->state);
            }
        } else {
            ctx->waitingForResponse = false;
            ctx->errorCount++;
            retryCount++;
            if (retryCount >= 3) {
                ctx->state = FSM_STATE_ERROR;
                ctx->stateEntryTime = currentMillis;
                displayMessage("Transaction data error! Check pump");
            }
        }
    }
}

static void updateTotalCounter(FSMContext* ctx) {
    unsigned long currentMillis = millis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse && ctx->c0RetryCount < MAX_ERROR_COUNT && (currentMillis - ctx->lastC0SendTime) >= RESPONSE_TIMEOUT) {
        rs422SendTotalCounter();
        ctx->waitingForResponse = true;
        ctx->lastC0SendTime = currentMillis;
        ctx->c0RetryCount++;
    } else if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, TOTAL_COUNTER_RESPONSE_LENGTH, 'C');
        if (handleResponse(respBuffer, respLength, TOTAL_COUNTER_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[3] == 'C' && respBuffer[4] == '1') {
                char totalStr[10] = {0};
                memcpy(totalStr, respBuffer + 6, 9);

                bool valid = true;
                for (int i = 0; i < 9; i++) {
                    if (totalStr[i] < '0' || totalStr[i] > '9') {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    uint32_t totalLiters_mL = atol(totalStr);
                    char litersBuf[12];
                    formatLiters(totalLiters_mL / 10, litersBuf, sizeof(litersBuf));
                    char displayStr[32];
                    snprintf(displayStr, sizeof(displayStr), "TOTAL:\n%s", litersBuf);
                    displayMessage(displayStr);
                } else {
                    displayMessage("TOTAL:\nError");
                }
                ctx->waitingForResponse = false;
                ctx->c0RetryCount = MAX_ERROR_COUNT;
            } else {
                if (ctx->c0RetryCount >= MAX_ERROR_COUNT) {
                    displayMessage("TOTAL:\nError");
                }
            }
        }
    }
}

/* Инициализация FSM */
void initFSM(FSMContext* ctx) {
    rs422SendNozzleOff();
    ctx->price = readPriceFromEEPROM();
    ctx->priceValid = ctx->price > 0;
    ctx->state = ctx->priceValid ? FSM_STATE_CHECK_STATUS : FSM_STATE_WAIT_FOR_PRICE_INPUT;
    ctx->fuelMode = FUEL_BY_VOLUME;
    ctx->stateEntryTime = millis();
    ctx->waitingForResponse = false;
    ctx->errorCount = 0;
    ctx->c0RetryCount = 0;
    ctx->lastC0SendTime = 0;
    ctx->statusPollingActive = true;
    ctx->transactionVolume = 0;
    ctx->transactionAmount = 0;
    ctx->transactionStarted = false;
    ctx->monitorState = 0;
    ctx->monitorActive = false;
    ctx->currentLiters_dL = 0;
    ctx->finalLiters_dL = 0;
    ctx->currentPriceTotal = 0;
    ctx->finalPriceTotal = 0;
    ctx->nozzleUpWarning = false;
    ctx->skipFirstStatusCheck = false;
    ctx->lastKeyTime = 0;
    ctx->priceInput[0] = '\0';
    if (!ctx->priceValid) {
        displayMessage("Set price (0-99999):");
    } else {
        uint32_t savedLiters, savedPrice;
        FSMState savedState;
        if (restoreTransactionState(&savedLiters, &savedPrice, &savedState)) {
            ctx->currentLiters_dL = savedLiters;
            ctx->currentPriceTotal = savedPrice;
            ctx->state = savedState;
            displayMessage("Restoring last transaction...");
        }
    }
}

/* Основной цикл FSM */
void updateFSM(FSMContext* ctx) {
    switch (ctx->state) {
        case FSM_STATE_CHECK_STATUS:        updateCheckStatus(ctx); break;
        case FSM_STATE_ERROR:               updateError(ctx); break;
        case FSM_STATE_IDLE:                updateIdle(ctx); break;
        case FSM_STATE_WAIT_FOR_PRICE_INPUT: updateWaitForPriceInput(ctx); break;
        case FSM_STATE_VIEW_PRICE:          updateViewPrice(ctx); break;
        case FSM_STATE_TRANSITION_PRICE_SET: updateTransitionPriceSet(ctx); break;
        case FSM_STATE_TRANSITION_EDIT_PRICE: updateTransitionEditPrice(ctx); break;
        case FSM_STATE_EDIT_PRICE:          updateEditPrice(ctx); break;
        case FSM_STATE_CONFIRM_TRANSACTION: updateConfirmTransaction(ctx); break;
        case FSM_STATE_TRANSACTION:         updateTransaction(ctx); break;
        case FSM_STATE_TRANSACTION_PAUSED:  updateTransactionPaused(ctx); break;
        case FSM_STATE_TRANSACTION_END:     updateTransactionEnd(ctx); break;
        case FSM_STATE_TOTAL_COUNTER:       updateTotalCounter(ctx); break;
        default: break;
    }
}

/* Управление вводом клавиш */
void processKeyFSM(FSMContext* ctx, char key) {
    unsigned long currentMillis = millis();
    if (currentMillis - ctx->lastKeyTime < KEY_DEBOUNCE_MS) {
        displayMessage("Slow down! Wait a moment");
        return;
    }
    ctx->lastKeyTime = currentMillis;

    // Вывод нажатой кнопки в Serial Monitor
    Serial.print("Key pressed: ");
    Serial.println(key);

    switch (ctx->state) {
        case FSM_STATE_WAIT_FOR_PRICE_INPUT: {
            if (key >= '0' && key <= '9') {
                size_t len = strlen(ctx->priceInput);
                if (len < PRICE_FORMAT_LENGTH) {
                    ctx->priceInput[len] = key;
                    ctx->priceInput[len + 1] = '\0';
                    char displayStr[32];
                    snprintf(displayStr, sizeof(displayStr), "%s: %s",
                             ctx->fuelMode == FUEL_BY_VOLUME ? "Volume" : "Amount", ctx->priceInput);
                    displayMessage(displayStr);
                }
                ctx->stateEntryTime = currentMillis;
            } else if (key == 'E') {
                if (strlen(ctx->priceInput) == 0) {
                    ctx->state = FSM_STATE_IDLE;
                    ctx->stateEntryTime = currentMillis;
                    if (!ctx->nozzleUpWarning) {
                        displayFuelMode(ctx->fuelMode);
                    }
                } else {
                    ctx->priceInput[0] = '\0';
                    displayMessage("Cleared");
                    ctx->stateEntryTime = currentMillis;
                }
            } else if (key == 'K') {
                if (strlen(ctx->priceInput) > 0) {
                    uint32_t value = atol(ctx->priceInput);
                    if (value > 0) {
                        if (ctx->fuelMode == FUEL_BY_VOLUME) {
                            ctx->transactionVolume = value * 100;
                            ctx->transactionAmount = 0;
                        } else {
                            ctx->transactionVolume = 0;
                            ctx->transactionAmount = value;
                        }
                        ctx->state = FSM_STATE_CONFIRM_TRANSACTION;
                        ctx->stateEntryTime = currentMillis;
                        ctx->priceInput[0] = '\0';
                        displayMessage("Confirm transaction? Press K");
                    } else {
                        displayMessage("Invalid value!");
                        ctx->priceInput[0] = '\0';
                        ctx->stateEntryTime = currentMillis;
                    }
                }
            }
            break;
        }
        case FSM_STATE_IDLE: {
            if (ctx->nozzleUpWarning && key == 'K') {
                displayMessage("Nozzle up! Please hang up.");
                ctx->stateEntryTime = currentMillis;
            } else if (key == 'G') {
                ctx->state = FSM_STATE_VIEW_PRICE;
                ctx->stateEntryTime = currentMillis;
                char priceStr[16];
                snprintf(priceStr, sizeof(priceStr), "Price: %u", ctx->price);
                displayMessage(priceStr);
            } else if (key == 'E') {
                ctx->statusPollingActive = true;
                if (!ctx->nozzleUpWarning) {
                    displayFuelMode(ctx->fuelMode);
                }
                ctx->stateEntryTime = currentMillis;
            } else if (key == 'C') {
                ctx->fuelMode = (FuelMode)((ctx->fuelMode + 1) % 3);
                displayFuelMode(ctx->fuelMode);
                ctx->stateEntryTime = currentMillis;
            } else if (key == 'K' && !ctx->nozzleUpWarning) {
                if (ctx->fuelMode == FUEL_BY_VOLUME || ctx->fuelMode == FUEL_BY_PRICE) {
                    ctx->priceInput[0] = '\0';
                    ctx->state = FSM_STATE_WAIT_FOR_PRICE_INPUT;
                    displayMessage(ctx->fuelMode == FUEL_BY_VOLUME ? "Enter Volume:" : "Enter Amount:");
                } else {
                    ctx->transactionVolume = 0;
                    ctx->transactionAmount = 999999;
                    ctx->state = FSM_STATE_CONFIRM_TRANSACTION;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Confirm transaction? Press K");
                }
            } else if (key == 'A') {
                ctx->statusPollingActive = false;
                ctx->state = FSM_STATE_TOTAL_COUNTER;
                ctx->stateEntryTime = currentMillis;
                ctx->errorCount = 0;
                ctx->c0RetryCount = 0;
                ctx->waitingForResponse = true;
                ctx->lastC0SendTime = currentMillis;
                rs422SendTotalCounter();
                displayMessage("TOTAL:\nWaiting...");
            }
            break;
        }
        case FSM_STATE_VIEW_PRICE: {
            if (key == 'G') {
                ctx->state = FSM_STATE_EDIT_PRICE;
                ctx->stateEntryTime = currentMillis;
                ctx->priceInput[0] = '\0';
                displayMessage("Editing Price:");
            } else if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                if (!ctx->nozzleUpWarning) {
                    displayFuelMode(ctx->fuelMode);
                }
            }
            break;
        }
        case FSM_STATE_EDIT_PRICE: {
            ctx->stateEntryTime = currentMillis;
            if (key >= '0' && key <= '9') {
                size_t len = strlen(ctx->priceInput);
                if (len < PRICE_FORMAT_LENGTH) {
                    ctx->priceInput[len] = key;
                    ctx->priceInput[len + 1] = '\0';
                    char displayStr[32];
                    snprintf(displayStr, sizeof(displayStr), "New Price: %s", ctx->priceInput);
                    displayMessage(displayStr);
                }
            } else if (key == 'E') {
                ctx->priceInput[0] = '\0';
                displayMessage("Cleared new price");
            } else if (key == 'K') {
                if (strlen(ctx->priceInput) > 0) {
                    uint16_t newPrice = atol(ctx->priceInput);
                    if (newPrice >= PRICE_MIN && newPrice <= 99999) {
                        ctx->price = newPrice;
                        writePriceToEEPROM(ctx->price);
                        displayMessage("Price updated!");
                        ctx->state = FSM_STATE_TRANSITION_EDIT_PRICE;
                        ctx->stateEntryTime = currentMillis;
                        ctx->priceInput[0] = '\0';
                    } else {
                        displayMessage("Price too high! Max 99999");
                        ctx->priceInput[0] = '\0';
                    }
                } else {
                    ctx->state = FSM_STATE_IDLE;
                    if (!ctx->nozzleUpWarning) {
                        displayFuelMode(ctx->fuelMode);
                    }
                    ctx->stateEntryTime = currentMillis;
                }
            }
            break;
        }
        case FSM_STATE_TRANSITION_PRICE_SET:
        case FSM_STATE_TRANSITION_EDIT_PRICE: {
            if (currentMillis - ctx->stateEntryTime >= TRANSITION_TIMEOUT) {
                ctx->waitingForResponse = false;
                ctx->state = (ctx->state == FSM_STATE_TRANSITION_PRICE_SET) ? FSM_STATE_CHECK_STATUS : FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                if (ctx->state == FSM_STATE_IDLE && !ctx->nozzleUpWarning) {
                    displayFuelMode(ctx->fuelMode);
                }
            }
            break;
        }
        case FSM_STATE_CONFIRM_TRANSACTION: {
            if (key == 'K') {
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                displayMessage(ctx->fuelMode == FUEL_BY_VOLUME ? "Transaction: Volume" : ctx->fuelMode == FUEL_BY_PRICE ? "Transaction: Price" : "Transaction: Full Tank");
            } else if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                if (!ctx->nozzleUpWarning) {
                    displayFuelMode(ctx->fuelMode);
                }
            }
            break;
        }
        case FSM_STATE_TRANSACTION: {
            if (key == 'E' && !ctx->transactionStarted) {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->transactionStarted = false;
                ctx->monitorState = 0;
                ctx->monitorActive = false;
                ctx->currentLiters_dL = 0;
                ctx->currentPriceTotal = 0;
                ctx->finalLiters_dL = 0;
                ctx->finalPriceTotal = 0;
                ctx->errorCount = 0;
                if (!ctx->nozzleUpWarning) {
                    displayFuelMode(ctx->fuelMode);
                }
            } else if (key == 'E') {
                rs422SendPause();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION_PAUSED;
                ctx->stateEntryTime = currentMillis;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
            }
            break;
        }
        case FSM_STATE_TRANSACTION_PAUSED: {
            if (key == 'K') {
                rs422SendResume();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
            } else if (key == 'E') {
                ctx->finalLiters_dL = ctx->currentLiters_dL;
                ctx->finalPriceTotal = ctx->currentPriceTotal;
                rs422SendTransactionUpdate();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION_END;
                ctx->stateEntryTime = currentMillis;
            }
            break;
        }
        case FSM_STATE_TRANSACTION_END: {
            if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->transactionStarted = false;
                ctx->monitorState = 0;
                ctx->monitorActive = false;
                ctx->waitingForResponse = false;
                ctx->currentLiters_dL = 0;
                ctx->currentPriceTotal = 0;
                ctx->finalLiters_dL = 0;
                ctx->finalPriceTotal = 0;
                ctx->errorCount = 0;
                ctx->skipFirstStatusCheck = true;
                if (!ctx->nozzleUpWarning) {
                    displayFuelMode(ctx->fuelMode);
                }
            }
            break;
        }
        case FSM_STATE_TOTAL_COUNTER: {
            if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->transactionStarted = false;
                ctx->monitorState = 0;
                ctx->monitorActive = false;
                ctx->waitingForResponse = false;
                ctx->errorCount = 0;
                ctx->c0RetryCount = 0;
                if (!ctx->nozzleUpWarning) {
                    displayFuelMode(ctx->fuelMode);
                }
            }
            break;
        }
        default: break;
    }
}

/* Получение состояния FSM */
FSMState getCurrentState(const FSMContext* ctx) {
    return ctx->state;
}

FuelMode getCurrentFuelMode(const FSMContext* ctx) {
    return ctx->fuelMode;
}