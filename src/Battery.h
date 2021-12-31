#pragma once

extern float warningLowVoltage;
extern uint8_t voltageCheckInterval;
extern float voltageIndicatorLow;
extern float voltageIndicatorHigh;

void Battery_Init(void);
void Battery_Cyclic(void);
float Battery_GetVoltage(void);
