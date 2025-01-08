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
QueueHandle_t gLedQueue;

void Queues_Init(void) {
	// Create queues
	gVolumeQueue = xQueueCreate(1, sizeof(int));
	if (gVolumeQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Volume");
	}

	gRfidCardQueue = xQueueCreate(1, cardIdStringSize);
	if (gRfidCardQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Rfid");
	}

	gTrackControlQueue = xQueueCreate(1, sizeof(uint8_t));
	if (gTrackControlQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Track-control");
	}

	gTrackQueue = xQueueCreate(1, sizeof(Playlist *));
	if (gTrackQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Playlist");
	}

	gEqualizerQueue = xQueueCreate(1, sizeof(int8_t[3]));
	if (gEqualizerQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Equalizer");
	}

	gLedQueue = xQueueCreate(1, sizeof(uint8_t));
	if (gLedQueue == NULL) {
		Log_Printf(LOGLEVEL_ERROR, unableToCreateQueue, "Led");
	}
}
