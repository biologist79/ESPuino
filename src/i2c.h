#pragma once
#ifndef __ESPUINO_I2C_H__
#define __ESPUINO_I2C_H__

#ifdef I2C_2_ENABLE
    #include <Wire.h>
#endif

#if (HAL == 2)
    #include "AC101.h"
#endif

extern TwoWire i2cBusOne;
extern TwoWire i2cBusTwo;
extern AC101 ac;
extern uint i2cOnCore;

// alle wesentlichen Funktionen
void i2c_Init(void);
void i2c_tsafe_execute(void (*)(void), uint waitTicks);
void i2c_clear_lines(int PIN_SDA, int PIN_SCL);
void i2c_scanExtBus(void);
#endif