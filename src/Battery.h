#pragma once

extern uint8_t batteryCheckInterval;


#if defined(MEASURE_BATTERY_VOLTAGE)
extern float voltageIndicatorCritical;
extern float warningLowVoltage;
extern float voltageIndicatorLow;
extern float voltageIndicatorHigh;
#endif

void Battery_Init(void);
void Battery_Cyclic(void);

// Implementation specific tasks
void Battery_CyclicImpl(void);
void Battery_InitImpl(void);

float Battery_EstimateSOC(void);
float Battery_GetVoltage(void);
bool Battery_IsLow(void);
bool Battery_IsCritical(void);

void Battery_PublishMQTT(void);
void Battery_LogStatus(void);