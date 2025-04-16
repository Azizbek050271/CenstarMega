#ifndef FRAME_H
#define FRAME_H

#include <Arduino.h>

/**
 * Assembles a GasKitLink v1.2 frame.
 * @param slaveAddress 2-byte slave address.
 * @param command Command code.
 * @param payload Payload data.
 * @param payloadLength Length of payload.
 * @param frameBuffer Output buffer for frame.
 * @param frameLength Output length of frame.
 */
void assembleFrame(const byte* slaveAddress, char command, const byte* payload, int payloadLength, byte* frameBuffer, int* frameLength);

#endif