// rs422.cpp
#include "rs422.h"
#include "frame.h"
#include "config.h"
#include "crc.h"
#include "oled.h" // Добавлено для displayMessage

static const uint8_t slaveAddress[2] = {0x00, POST_ADDRESS};
static bool isSending = false;
static bool isReceiving = false;

void log(int level, const char* msg) {
    if (level >= LOG_LEVEL) {
        Serial.println(msg);
    }
}

static void flushInput(unsigned long timeoutUs = 2000) {
    unsigned long t0 = micros();
    while (micros() - t0 < timeoutUs) {
        if (Serial1.available()) {
            uint8_t byte = Serial1.read();
            if (byte == 0x02 || byte == 0x04) t0 = micros();
        } else {
            delayMicroseconds(100);
        }
    }
}

void initRS422() {
    Serial1.begin(RS422_BAUD_RATE);
}

void rs422SendStatus() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t payload[0];
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, 'S', payload, 0, frameBuffer, &frameLength);

    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

void rs422SendTransaction(FuelMode mode, uint32_t volume, uint32_t amount, uint16_t price) {
    if (isSending || isReceiving) return;
    if (price > 9999 || (mode == FUEL_BY_VOLUME && volume > 999999) || (mode == FUEL_BY_PRICE && amount > 999999)) {
        log(LOG_LEVEL_ERROR, "Invalid transaction parameters");
        displayMessage("Invalid transaction data");
        return;
    }
    isSending = true;

    flushInput();
    uint8_t frameBuffer[32];
    int frameLength = 0;
    char payload[16];
    switch (mode) {
        case FUEL_BY_VOLUME:
            snprintf(payload, sizeof(payload), "V1;%06lu;%04u", volume, price);
            break;
        case FUEL_BY_PRICE:
            snprintf(payload, sizeof(payload), "M1;%06lu;%04u", amount, price);
            break;
        case FUEL_BY_FULL_TANK:
            snprintf(payload, sizeof(payload), "M1;999999;%04u", price);
            break;
    }

    assembleFrame(slaveAddress, payload[0], (uint8_t*)payload + 1, strlen(payload) - 1, frameBuffer, &frameLength);
    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

void rs422SendTransactionUpdate() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t payload[0];
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, 'T', payload, 0, frameBuffer, &frameLength);

    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    delayMicroseconds(500);
    isSending = false;
}

void rs422SendNozzleOff() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t payload[0];
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, 'N', payload, 0, frameBuffer, &frameLength);

    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

void rs422SendLitersMonitor() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t payload[0];
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, 'L', payload, 0, frameBuffer, &frameLength);

    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

void rs422SendRevenueStatus() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t payload[0];
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, 'R', payload, 0, frameBuffer, &frameLength);

    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

void rs422SendTotalCounter() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t frameBuffer[32];
    int frameLength = 0;
    uint8_t payload[1] = {'1'};
    assembleFrame(slaveAddress, 'C', payload, 1, frameBuffer, &frameLength);

    log(LOG_LEVEL_DEBUG, "Sending C1 command");
    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

void rs422SendPause() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t payload[0];
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, 'B', payload, 0, frameBuffer, &frameLength);

    log(LOG_LEVEL_DEBUG, "Sending pause command");
    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

void rs422SendResume() {
    if (isSending || isReceiving) return;
    isSending = true;

    flushInput();
    uint8_t payload[0];
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, 'G', payload, 0, frameBuffer, &frameLength);

    log(LOG_LEVEL_DEBUG, "Sending resume command");
    Serial1.write(frameBuffer, frameLength);
    Serial1.flush();
    isSending = false;
}

int rs422WaitForResponse(uint8_t* buffer, int expectedLength, char expectedCommand) {
    if (isReceiving) return 0;
    isReceiving = true;

    int count = 0;
    unsigned long startTime = millis();
    unsigned long lastByteTime = startTime;

    while ((millis() - startTime) < RESPONSE_TIMEOUT) {
        if (Serial1.available() > 0) {
            buffer[count++] = Serial1.read();
            lastByteTime = millis();
            if (count == expectedLength) {
                if (buffer[0] != 0x02 || buffer[1] != slaveAddress[0] || buffer[2] != slaveAddress[1] || buffer[3] != expectedCommand) {
                    log(LOG_LEVEL_ERROR, "Invalid response format or command");
                    displayMessage("Invalid response from pump");
                    count = -1;
                    break;
                }
                uint8_t calcCRC = calculateCRC(buffer, expectedLength - 1);
                if (calcCRC != buffer[expectedLength - 1]) {
                    log(LOG_LEVEL_ERROR, "CRC mismatch");
                    displayMessage("Invalid response from pump");
                    count = -1;
                }
                break;
            }
        } else if (count > 0 && (millis() - lastByteTime) >= INTERBYTE_TIMEOUT) {
            log(LOG_LEVEL_ERROR, "Incomplete response");
            break;
        }
    }

    isReceiving = false;
    return count;
}