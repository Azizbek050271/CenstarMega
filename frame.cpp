#include "frame.h"
#include "crc.h"
#include "config.h"

void assembleFrame(const byte* slaveAddress, char command, const byte* payload, int payloadLength, byte* frameBuffer, int* frameLength) {
    if (payloadLength > MAX_FRAME_PAYLOAD) return;
    int index = 0;
    frameBuffer[index++] = 0x02;
    frameBuffer[index++] = slaveAddress[0];
    frameBuffer[index++] = slaveAddress[1];
    frameBuffer[index++] = (byte)command;
    for (int i = 0; i < payloadLength; i++) {
        frameBuffer[index++] = payload[i];
    }
    byte crc = calculateCRC(frameBuffer, index);
    frameBuffer[index++] = crc;
    *frameLength = index;
}