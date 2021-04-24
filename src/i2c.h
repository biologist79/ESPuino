#pragma once
#ifndef __ESPUINO_I2C_H__
#define __ESPUINO_I2C_H__

#if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE) || defined (PORT_TOUCHMPR121_ENABLE)
    #include "Wire.h"
#endif

extern TwoWire i2cBusOne;
extern TwoWire i2cBusTwo;

void I2C_Init(void);
void I2C_clear_lines(int PIN_SDA, int PIN_SCL);
#endif