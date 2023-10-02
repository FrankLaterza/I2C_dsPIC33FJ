#include "i2c.h"
#include <libpic30.h>
#include <stdbool.h>
#include <xc.h>

/**
 * as per the data sheet the follow equation can
 * be used to calculate the serial clock rate where
 * FCY = instruction clock
 * FSCL = desired clock
 *
 * I2C1BRG = ((FCY / FSCL) - FCY / 1e7) - 1
 *
 * using my desired numbers i got
 * ((40,000,000/100,000)-(40,000,000/1,111,111))-1 = 363
 */
#define FFRAM_BAUD 363
#define FFRAM_ADDRESS 67
#define DEVICE_ID 10 // 1010
#define DEVICE_SELECT 0 // defined by pins A1-A2 (both are zero)


void I2cTimeoutHandle(volatile uint16_t* i2cReg, uint16_t mask) {
    // PrintString( "STARTING TIMEOUT\r\n");
    for (uint8_t m = 0; m <= TIMEOUT; m++) {
        // wait for change
        if ((*i2cReg & mask) == 0) {
            // bit changed success
            return; // end
        }
        // wait 1 microsecond
        __delay32(1);
    }
    // timed out HANDLE ERROR HERE
}

// sets up the i2c
void I2cInit(void) {
    I2C1BRG = FFRAM_BAUD; // @100kHz
    I2C1ADD = FFRAM_ADDRESS; // address of chip
    I2C1CONbits.I2CEN = 0; // disable I2C
    I2C1CONbits.DISSLW = 1; // disable slew rate control
    I2C1CONbits.A10M = 0; // 7-bit slave address
    I2C1CONbits.SCLREL = 1; // SCL release control
    I2C1CONbits.I2CEN = 1; // enable I2C

    TRISBbits.TRISB9 = 1; // SDA as input
    TRISBbits.TRISB8 = 1; // SCL as input

    // interrupt, not sure if we need them.
    IEC1bits.MI2C1IE = 0; // master I2C interrupt
    IFS1bits.MI2C1IF = 0; // MI2C flag
}

void I2cStart(void) {
    I2C1CONbits.ACKDT = 0; // reset any ACK
    I2C1CONbits.SEN = 1; // start
    // wait for timeout
    I2cTimeoutHandle(&I2C1CON, _I2C1CON_SEN_MASK);
}

// sets ack to 1
void I2cAck(void) {
    I2C1CONbits.ACKDT = 0; // send ACK
    I2C1CONbits.ACKEN = 1; // initiate acknowledge and transmit ACKDT
    // wait for timeout
    return I2cTimeoutHandle(&I2C1CON, _I2C1CON_ACKEN_MASK);
}

// not ack
void I2cNack(void) {
    I2C1CONbits.ACKDT = 1; // send NACK
    I2C1CONbits.ACKEN = 1; // initiate acknowledge and transmit ACKDT
    // wait for timeout
    I2cTimeoutHandle(&I2C1CON, _I2C1CON_ACKEN_MASK);
}

// set stop register
void I2cStop(void) {
    I2C1CONbits.RCEN = 0; // receive mode not in progress
    I2C1CONbits.PEN = 1; // stop condition
    // wait for timeout
    I2cTimeoutHandle(&I2C1CON, _I2C1CON_PEN_MASK);
}

// should reset with the same start conditions
void I2cRestart(void) {
    I2C1CONbits.RSEN = 1; // repeated start condition
    // wait for timeout
    I2cTimeoutHandle(&I2C1CON, _I2C1CON_RSEN_MASK);
    I2C1CONbits.ACKDT = 0; // send ACK
    I2C1STATbits.TBF = 0; // I2C1TRN is empty
}

// waits until everything is sent
void I2cIdle(void) {
    I2cTimeoutHandle(&I2C1STAT, _I2C1STAT_TRSTAT_MASK);
}

// sets byte register and sends it off
void I2cWriteByte(uint8_t byte) {
    I2CTRN = byte; // transmit buffer
    // wait for timeout
    I2cTimeoutHandle(&I2C1STAT, _I2C1STAT_TBF_MASK);
}

// enable receive and receive
void I2cReadByte(void) {
    I2CCONbits.RCEN = 1;
    // I2cTimeoutHandle(I2C1STATbits.TBF);
}

/**
 * @brief this is the first handshake with the i2c. starts with the
 * device id, then sets the device select, then page select, then the
 * operations. for the operation the read is a 1 and the write is a 0.
 * the binary has to be built then it gets sent off.
 *
 * @param select // address of device
 * @param page // page select
 * @param op // operation: r = 1. w = 0
 */
void I2cWriteAddress(uint8_t select, bool page, bool op) {
    /**
     * example to start a read operation at
     * address 1010 with 00 device select and
     * 0 page select.
     * binary: 1010 00 0 1
     * 8765 43 2 1
     *
     * bits 8-5 are the address
     * bits 4-3 are device select
     * bit 2 is page select
     * bit 1 is read or write. r = 1. w = 0
     */

    // start it at the address
    uint8_t res = (DEVICE_ID << 4 | select << 2 | page << 1 | op);
    // write the start byte
    I2cWriteByte(res);
}

/**
 * @brief write the data to the register
 *
 * @param addr // address of device
 * @param reg // address of register
 * @param data // byte to be written
 */
void I2cWriteReg(uint8_t select, uint16_t reg, uint8_t data) {
    // stop if start failed (no chip)
    I2cStart();
    // start with flash chip address
    I2cWriteAddress(select, 0, 0); // go to function to see more
    // idle
    I2cIdle();
    // set the last half of flash register
    I2cWriteByte((uint8_t)(reg >> 8));
    // idle
    I2cIdle();
    // set the first half of flash register
    I2cWriteByte((uint8_t)(reg % 256));
    // idle
    I2cIdle();
    // write the data
    I2cWriteByte(data);
    // idle
    I2cIdle();
    // stop
    I2cStop();
}

/**
 * @brief writes the desired address then sends a read request
 *
 * @param addr // address of device
 * @param reg // address of register
 * @return uint8_t // returns the byte at the register
 */
uint8_t I2cReadReg(uint8_t select, uint16_t reg) {
    // the value of what gets read
    uint8_t res;
    // start
    I2cStart();
    // start with flash chip address
    I2cWriteAddress(select, 0, 0); // see function to see more
    // idle
    I2cIdle();
    // set the last half of flash register
    I2cWriteByte((uint8_t)(reg >> 8));
    // idle
    I2cIdle();
    // set the first half of flash register
    I2cWriteByte((uint8_t)(reg % 256));
    // idle
    I2cIdle();
    // restart (restarts with start condition)
    I2cRestart();
    // start a read
    I2cWriteAddress(select, 0, 1); // see function to see more
    // idle
    I2cIdle();
    // read the value
    I2cReadByte();
    // we are done reading
    I2cNack();
    // stop
    I2cStop();
    // get the read value
    res = I2C1RCV;
    // return the goods
    return res;
}
