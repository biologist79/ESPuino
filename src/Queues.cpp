#include <Arduino.h>
#include "settings.h"

#include "Log.h"
#include "Playlist.h"
#include "Rfid.h"

QueueHandle_t gVolumeQueue;
QueueHandle_t gTrackQueue;
QueueHandle_t gTrackControlQueue;
QueueHandle_t gRfidCardQueue;
QueueHandle_t gEqualizerQueue;

void Queues_Init(void) {
	// Create queues
	gVolumeQueue = xQueueCreate(1, sizeof(int));
	if (gVolumeQueue == NULL) {
		Log_Println(unableToCreateVolQ, LOGLEVEL_ERROR);
	}

	gRfidCardQueue = xQueueCreate(1, cardIdStringSize);
	if (gRfidCardQueue == NULL) {
		Log_Println(unableToCreateRfidQ, LOGLEVEL_ERROR);
	}

	gTrackControlQueue = xQueueCreate(1, sizeof(uint8_t));
	if (gTrackControlQueue == NULL) {
		Log_Println(unableToCreateMgmtQ, LOGLEVEL_ERROR);
	}

	gTrackQueue = xQueueCreate(1, sizeof(Playlist *));
	if (gTrackQueue == NULL) {
		Log_Println(unableToCreatePlayQ, LOGLEVEL_ERROR);
	}

	gEqualizerQueue = xQueueCreate(1, sizeof(int8_t[3]));
	if (gEqualizerQueue == NULL) {
		Log_Println(unableToCreateEqualizerQ, LOGLEVEL_ERROR);
	}
}
