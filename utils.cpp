#include "utils.h"

void intToString(uint16_t value, uint8_t digits, char* buffer) {
    snprintf(buffer, digits + 1, "%0*u", digits, value);
}