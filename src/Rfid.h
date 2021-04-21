#pragma once

constexpr uint8_t cardIdSize = 4u;
constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

extern char *gCurrentRfidTagId;

void Rfid_Init(void);
void Rfid_Cyclic(void);
void Rfid_Exit(void);
void Rfid_WakeupCheck(void);
void Rfid_PreferenceLookupHandler(void);
