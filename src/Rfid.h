#pragma once

constexpr uint8_t cardIdSize = 4u;
constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

#ifndef CONFIG_PAUSE_WHEN_RFID_REMOVED
	#ifdef CONFIG_DONT_ACCEPT_SAME_RFID_TWICE      // ignore feature silently if CONFIG_PAUSE_WHEN_RFID_REMOVED is active
		#define CONFIG_DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	#endif
#endif

#ifdef CONFIG_DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
	extern char gOldRfidTagId[cardIdStringSize];
#endif

void Rfid_Init(void);
void Rfid_Cyclic(void);
void Rfid_Exit(void);
void Rfid_WakeupCheck(void);
void Rfid_PreferenceLookupHandler(void);
