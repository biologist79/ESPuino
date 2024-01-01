#pragma once
#include <Arduino.h>
#include <cstdint>
#include "settings.h"
	#ifdef RFID_READER_TYPE_MFRC522_SPI
		#include <MFRC522.h>
	#endif
	#if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE)
		#include "Wire.h"
	#endif
	#ifdef RFID_READER_TYPE_MFRC522_I2C
		#include <MFRC522_I2C.h>
	#endif
            
typedef struct { // Mifare Data Holder Pos: Sector/Block/BytePosition
    uint8_t cardType;      // cardType - Pos: 0/1/8
    uint8_t playMode;      // playMode / CMD - Pos: 0/1/9
    uint8_t trackNr;       // trackNr for PlayMode single title  - Pos: 0/1/10
    uint8_t uuid[16];         // Uuid : Unique ID for this Web-Card use as folder name (hex) - Pos: 0/2 , Size: 1 Block
    char uuid64[23];         // Uuid : base64url encoded
    char uri[241];         // Uri : File or Webaddress, Pos: 1/0 , Size 4 Sectors 1-5: 3 Blocks * 16 bytes * 4 Sectors 
} cardmanData;

/*
cardType (dec / hex)
  0 / 0x00 = Device Only / Saved in old Tagfile
  1 / 0x01 = Reuseable / Old Style
 17 / 0x11 = Web Card / Single File 
 18 / 0x12 = Web Card / Catalog File 
173 / 0xAD = Admin Card (playMode = Cmd)

*/

/* 
Admin - CMDs

100 / 0x64 = LOCK_BUTTONS                   // Locks all buttons and rotary encoder
101 / 0x65 = SLEEP_TIMER_MOD_15             // Puts uC into deepsleep after 15 minutes + LED-DIMM
102 / 0x66 = SLEEP_TIMER_MOD_30             // Puts uC into deepsleep after 30 minutes + LED-DIMM
103 / 0x67 = SLEEP_TIMER_MOD_60             // Puts uC into deepsleep after 60 minutes + LED-DIMM
104 / 0x68 = SLEEP_TIMER_MOD_120            // Puts uC into deepsleep after 120 minutes + LED-DIMM
105 / 0x69 = SLEEP_AFTER_END_OF_TRACK       // Puts uC into deepsleep after track is finished + LED-DIMM
106 / 0x6A = SLEEP_AFTER_END_OF_PLAYLIST    // Puts uC into deepsleep after playlist is finished + LED-DIMM
107 / 0x6B = SLEEP_AFTER_5_TRACKS           // Puts uC into deepsleep after five tracks
110 / 0x6E = REPEAT_PLAYLIST                // Changes active playmode to endless-loop (for a playlist)
111 / 0x6F = REPEAT_TRACK                   // Changes active playmode to endless-loop (for a single track)
120 / 0x78 = DIMM_LEDS_NIGHTMODE            // Changes LED-brightness
130 / 0x82 = WIFI_STATUS_TOGGLE             // Toggles WiFi-status
131 / 0x83 = WIFI_STATUS_ENABLE             // Enable WiFi
132 / 0x84 = TOGGLE_WIFI_DISABLE            // Disable WiFi

140 / 0x8C = PAUSE_PLAY                     // Pause / Play Button
141 / 0x8D = PREV_TRACK                     // Prev Button
142 / 0x8E = NEXT_TRACK                     // Next Button

*/
extern cardmanData cardData;
void CardMan_Init();
int CardMan_filldata(MFRC522 *frc, cardmanData *data);
String CardMan_Handle(cardmanData *data);