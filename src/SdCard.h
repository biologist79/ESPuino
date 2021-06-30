#pragma once
#include "settings.h"
#ifdef SD_MMC_1BIT_MODE
#include "SD_MMC.h"
#else
#include "SD.h"
#endif

extern fs::FS gFSystem;

void SdCard_Init(void);
void SdCard_Exit(void);
sdcard_type_t SdCard_GetType(void);
char **SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode);
