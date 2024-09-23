#pragma once
#include "settings.h"
#ifdef SD_MMC_1BIT_MODE
	#include "SD_MMC.h"
#else
	#include "SD.h"
#endif

extern fs::FS gFSystem;

#include "Playlist.h"

#include <optional>

void SdCard_Init(void);
void SdCard_Exit(void);
sdcard_type_t SdCard_GetType(void);
uint64_t SdCard_GetSize();
uint64_t SdCard_GetFreeSize();
void SdCard_PrintInfo();
std::optional<Playlist *> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode);
const String SdCard_pickRandomSubdirectory(const char *_directory);
