#include <Arduino.h>
#include "settings.h"

#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "Rfid.h"
#include "RfidConfig.h"

extern void RfidMfrc522_Init(uint8_t readerType);
extern void RfidMfrc522_Cyclic(void);
extern void RfidMfrc522_Exit(void);
extern void RfidMfrc522_TaskReset(void);
extern void RfidMfrc522_WakeupCheck(void);

extern void RfidPn5180_Init(void);
extern void RfidPn5180_WakeupHandling(void);
extern void RfidPn5180_StartTask(void);
extern void RfidPn5180_Cyclic(void);
extern void RfidPn5180_Exit(void);
extern void RfidPn5180_TaskReset(void);
extern void RfidPn5180_WakeupCheck(void);

TaskHandle_t rfidTaskHandle = NULL;

void Rfid_Init(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	RfidConfig_Init();
	RfidReaderType readerType = RfidConfig_GetReaderType();
	if ((readerType == RfidReaderType::TYPE_MFRC522_SPI) || (readerType == RfidReaderType::TYPE_MFRC522_I2C)) {
		RfidMfrc522_Init(readerType);
	} else {
		RfidPn5180_Init();
	}
#endif
}

// Only meaningful for a PN5180 with LPCD enabled: handles a possible deep-sleep wakeup via
// card-detection and releases GPIO pin holds from a previous LPCD-armed deep-sleep. Must be
// called early (before other peripherals are powered) so a wakeup without a known card can
// send the device straight back to sleep. Does not start the RFID scanning task; call
// Rfid_StartTask() for that once peripherals are up.
void Rfid_WakeupHandling(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	RfidConfig_Init();
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_WakeupHandling();
	}
#endif
}

// Starts the RFID scanning task. For PN5180 with LPCD enabled, this is called separately
// from Rfid_WakeupHandling() (see there for why); for all other cases, Rfid_Init() already
// starts the task itself and this is a no-op.
void Rfid_StartTask(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_StartTask();
	}
#endif
}

void Rfid_Cyclic(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_Cyclic();
	} else {
		RfidMfrc522_Cyclic();
	}
#endif
}

void Rfid_Exit(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	Log_Println("shutdown rfid-reader..", LOGLEVEL_NOTICE);
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_Exit();
	} else {
		RfidMfrc522_Exit();
	}
#endif
}

// Rfid_TaskPause and Rfid_TaskResume are implemented in RfidCommon.cpp

void Rfid_TaskReset(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_TaskReset();
	} else {
		RfidMfrc522_TaskReset();
	}
#endif
}

void Rfid_WakeupCheck(void) {
#if defined(RFID_READER_TYPE_RUNTIME)
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_PN5180) {
		RfidPn5180_WakeupCheck();
	} else {
		RfidMfrc522_WakeupCheck();
	}
#endif
}
