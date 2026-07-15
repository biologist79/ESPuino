#include <Arduino.h>
#include "settings.h"

#include "MediaHub.h"

#include "Log.h"
#include "System.h"

const char *const MediaHub_PathPrefix = "mediahub://";

bool MediaHub_IsMediaHubPath(const char *path) {
	return path != NULL && strncmp(path, MediaHub_PathPrefix, strlen(MediaHub_PathPrefix)) == 0;
}

void MediaHub_HandleCardTapped(const char *cardId, const char *path, uint32_t lastPlayPos, uint16_t trackLastPlayed) {
	// Phase 0 stub (mediahub-konzept.md §17): dispatch wiring only, no
	// manifest fetch or playback yet. lastPlayPos/trackLastPlayed are
	// accepted here already so later phases don't need to touch the
	// RfidCommon.cpp call site again, but they're unused for now.
	(void) lastPlayPos;
	(void) trackLastPlayed;
	Log_Printf(LOGLEVEL_INFO, "MediaHub: card %s tapped (hub=%s), not implemented yet", cardId, path);
	System_IndicateError();
}
