#ifndef CRC_H
#define CRC_H

#include <Arduino.h>

/**
 * Calculates CRC for a given data buffer.
 * @param data Buffer to calculate CRC for.
 * @param length Length of the buffer.
 * @return Calculated CRC value.
 */
byte calculateCRC(const byte* data, int length);

#endif