#include "crc.h"

byte calculateCRC(const byte* data, int length) {
    if (length < 2) return 0;
    byte crc = data[1];
    for (int i = 2; i < length; i++) {
        crc ^= data[i];
    }
    return crc;
}