#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32S3
	#define MAX_GPIO 47
#else
	#define MAX_GPIO 39
#endif

void Port_Init(void);
void Port_Cyclic(void);
bool Port_Read(const uint8_t _channel);
void Port_Write(const uint8_t _channel, const bool _newState, const bool _initGpio);
void Port_Exit(void);
