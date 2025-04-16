// rs422.h
#ifndef RS422_H
#define RS422_H

#include <stdint.h>
#include "fsm.h"

void initRS422();
void rs422SendStatus();
void rs422SendTransaction(FuelMode mode, uint32_t volume, uint32_t amount, uint16_t price);
void rs422SendTransactionUpdate();
void rs422SendNozzleOff();
void rs422SendLitersMonitor();
void rs422SendRevenueStatus();
void rs422SendTotalCounter();
void rs422SendPause();
void rs422SendResume();
int rs422WaitForResponse(uint8_t* buffer, int expectedLength, char expectedCommand); // Обновлено
void log(int level, const char* msg);

#endif