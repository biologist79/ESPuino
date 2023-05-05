#pragma once

#ifdef INVERT_POWER
	#define POWER_ON  LOW
	#define POWER_OFF HIGH
#else
	#define POWER_ON  HIGH
	#define POWER_OFF LOW
#endif

void Power_Init(void);
void Power_PeripheralOn(void);
void Power_PeripheralOff(void);
