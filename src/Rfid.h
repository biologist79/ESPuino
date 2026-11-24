#pragma once

constexpr uint8_t cardIdSize = 4u;
constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

void Rfid_ResetOldRfid(void);
void Rfid_ResetLastTag(void);
// Reader-task internal: returns true once after Rfid_ResetLastTag() was called.
bool Rfid_ConsumeLastTagReset(void);
void Rfid_Init(void);
void Rfid_WakeupHandling(void);
void Rfid_StartTask(void);
void Rfid_Cyclic(void);
void Rfid_Exit(void);
void Rfid_TaskPause(void);
void Rfid_TaskResume(void);
void Rfid_TaskReset(void);
// Reader-task internal: parks the calling reader task while a pause/stop is requested.
void Rfid_TaskParkIfRequested(void);
// Asks the reader task to park and waits up to timeoutMs for it. Returns true if it is parked.
bool Rfid_TaskStop(uint32_t timeoutMs);
void Rfid_WakeupCheck(void);
void Rfid_PreferenceLookupHandler(void);
// Writes one card assignment to NVS, see RfidCommon.cpp.
bool Rfid_SaveAssignment(const char *cardId, const char *fileOrUrl, uint8_t playModeOrModId,
	uint32_t lastPlayPos = 0, uint16_t trackLastPlayed = 0);
