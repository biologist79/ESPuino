#pragma once
#ifndef __ESPUINO_I2C_H__
#define __ESPUINO_I2C_H__

#if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE) || defined (PORT_TOUCHMPR121_ENABLE)
    #include "Wire.h"
#endif

#if (HAL == 2)
    #include "AC101.h"
#endif

extern TwoWire i2cBusOne;
extern TwoWire i2cBusTwo;
extern AC101 ac;

void i2c_init(void);
void i2c_clear_lines(int PIN_SDA, int PIN_SCL);
#endif