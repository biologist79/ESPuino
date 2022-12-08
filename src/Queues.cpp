#include <Arduino.h>
#include "settings.h"
#include "Log.h"
#include "Rfid.h"

QueueHandle_t gVolumeQueue;
QueueHandle_t gTrackQueue;
QueueHandle_t gTrackControlQueue;
QueueHandle_t gRfidCardQueue;

void Queues_Init(void) {
	// Create queues
	gVolumeQueue = xQueueCreate(1, sizeof(int));
	if (gVolumeQueue == NULL) {
		Log_Println((char *) FPSTR(unableToCreateVolQ), LOGLEVEL_ERROR);
	}

	gRfidCardQueue = xQueueCreate(1, cardIdStringSize);
	if (gRfidCardQueue == NULL) {
		Log_Println((char *) FPSTR(unableToCreateRfidQ), LOGLEVEL_ERROR);
	}

	gTrackControlQueue = xQueueCreate(1, sizeof(uint8_t));
	if (gTrackControlQueue == NULL) {
		Log_Println((char *) FPSTR(unableToCreateMgmtQ), LOGLEVEL_ERROR);
	}

	char **playlistArray;
	gTrackQueue = xQueueCreate(1, sizeof(playlistArray));
	if (gTrackQueue == NULL) {
		Log_Println((char *) FPSTR(unableToCreatePlayQ), LOGLEVEL_ERROR);
	}
}
