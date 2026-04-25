#include <Arduino.h>
#include "Rfid.h"
#include "RfidConfig.h"
#include "settings.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"

extern void RfidMfrc522_Init(uint8_t readerType);
extern void RfidMfrc522_Cyclic(void);
extern void RfidMfrc522_Exit(void);
extern void RfidMfrc522_TaskReset(void);
extern void RfidMfrc522_WakeupCheck(void);

extern void RfidPn5180_Init(void);
extern void RfidPn5180_Cyclic(void);
extern void RfidPn5180_TaskReset(void);
extern void RfidPn5180_WakeupCheck(void);

TaskHandle_t rfidTaskHandle = NULL;

void Rfid_Init(void) {
	RfidConfig_Init();
	RfidReaderType readerType = RfidConfig_GetReaderType();
	if ((readerType == RfidReaderType::TYPE_MFRC522_SPI) || (readerType == RfidReaderType::TYPE_MFRC522_I2C)) {
		RfidMfrc522_Init(readerType);
	} else {
		RfidPn5180_Init();
	}
}

void Rfid_Cyclic(void) {
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_Cyclic();
	} else {
		RfidMfrc522_Cyclic();
	}
}

// Rfid_Exit, Rfid_TaskPause, and Rfid_TaskResume are implemented in RfidCommon.cpp

void Rfid_TaskReset(void) {
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_TaskReset();
	} else {
		RfidMfrc522_TaskReset();
	}
}

void Rfid_WakeupCheck(void) {
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_WakeupCheck();
	} else {
		RfidMfrc522_WakeupCheck();
	}
}
