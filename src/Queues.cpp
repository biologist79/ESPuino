#include <Arduino.h>
#include "settings.h"

#include "Log.h"
#include "Playlist.h"
#include "Rfid.h"

QueueHandle_t gRfidCardQueue;
QueueHandle_t gLedQueue;

void Queues_Init(void) {
	// Create queues

	gRfidCardQueue = xQueueCreate(1, cardIdStringSize);
	if (gRfidCardQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Rfid");
	}

	gLedQueue = xQueueCreate(1, sizeof(uint8_t));
	if (gLedQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Led");
	}
}
