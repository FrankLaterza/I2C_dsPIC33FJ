#ifndef I2C_H
#define I2C_H

#include <xc.h> // include processor files - each processor file is guarded.
#include <stdbool.h>
/*
 * main clock speed = 40MHz -> 25ns 
 * i2c clock speed = 100kHz -> 10000ns
 * slices = i2c clock / main clock
 * 
 * timeout will be 3 i2c clock pulses
 * 3 i2c pulses is 30us
 * 
 * for sending a byte the max allotted time
 * will be 30*8 = 240
 */
#define TIMEOUT 240

void I2cInit(void);
void I2cWriteByte(uint8_t c);
void I2cReadByte(void);
void I2cWriteAddress(uint8_t select, bool page, bool op);
void I2cWriteReg(uint8_t select, uint16_t reg, uint8_t data);
uint8_t I2cReadReg(uint8_t select, uint16_t reg);

#endif /* I2C_H */