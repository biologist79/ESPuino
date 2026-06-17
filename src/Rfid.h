#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint8_t cardIdSize = 4u;
constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

void Rfid_ResetOldRfid(void);
void Rfid_Init(void);
void Rfid_Cyclic(void);
void Rfid_Exit(void);
void Rfid_TaskPause(void);
void Rfid_TaskResume(void);
void Rfid_TaskReset(void);
void Rfid_WakeupCheck(void);
bool Rfid_FormatCardId(const uint8_t *cardId, size_t cardIdLen, char *target, size_t targetLen);
void Rfid_PreferenceLookupHandler(void);
