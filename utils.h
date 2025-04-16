#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

/**
 * Converts an integer to a string with leading zeros.
 * @param value Integer value.
 * @param digits Number of digits.
 * @param buffer Output buffer.
 */
void intToString(uint16_t value, uint8_t digits, char* buffer);

#endif