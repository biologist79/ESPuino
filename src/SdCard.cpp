#include <Arduino.h>
#include "settings.h"

#include "SdCard.h"

#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

#ifdef SD_MMC_1BIT_MODE
fs::FS gFSystem = (fs::FS) SD_MMC;
#else
SPIClass spiSD(HSPI);
fs::FS gFSystem = (fs::FS) SD;
#endif

void SdCard_Init(void) {
#ifdef NO_SDCARD
	// Initialize without any SD card, e.g. for webplayer only
	Log_Println("Init without SD card ", LOGLEVEL_NOTICE);
	return
#endif

#ifndef SINGLE_SPI_ENABLE
	#ifdef SD_MMC_1BIT_MODE
		pinMode(2, INPUT_PULLUP);
	while (!SD_MMC.begin("/sdcard", true)) {
	#else
		pinMode(SPISD_CS, OUTPUT);
	digitalWrite(SPISD_CS, HIGH);
	spiSD.begin(SPISD_SCK, SPISD_MISO, SPISD_MOSI, SPISD_CS);
	spiSD.setFrequency(1000000);
	while (!SD.begin(SPISD_CS, spiSD)) {
	#endif
#else
	#ifdef SD_MMC_1BIT_MODE
	pinMode(2, INPUT_PULLUP);
	while (!SD_MMC.begin("/sdcard", true)) {
	#else
	while (!SD.begin(SPISD_CS)) {
	#endif
#endif
		Log_Println(unableToMountSd, LOGLEVEL_ERROR);
		delay(500);
#ifdef SHUTDOWN_IF_SD_BOOT_FAILS
		if (millis() >= deepsleepTimeAfterBootFails * 1000) {
			Log_Println(sdBootFailedDeepsleep, LOGLEVEL_ERROR);
			esp_deep_sleep_start();
		}
#endif
	}
}

void SdCard_Exit(void) {
// SD card goto idle mode
#ifdef SINGLE_SPI_ENABLE
	Log_Println("shutdown SD card (SPI)..", LOGLEVEL_NOTICE);
	SD.end();
#endif
#ifdef SD_MMC_1BIT_MODE
	Log_Println("shutdown SD card (SD_MMC)..", LOGLEVEL_NOTICE);
	SD_MMC.end();
#endif
}

sdcard_type_t SdCard_GetType(void) {
	sdcard_type_t cardType;
#ifdef SD_MMC_1BIT_MODE
	Log_Println(sdMountedMmc1BitMode, LOGLEVEL_NOTICE);
	cardType = SD_MMC.cardType();
#else
	Log_Println(sdMountedSpiMode, LOGLEVEL_NOTICE);
	cardType = SD.cardType();
#endif
	return cardType;
}

uint64_t SdCard_GetSize() {
#ifdef SD_MMC_1BIT_MODE
	return SD_MMC.cardSize();
#else
	return SD.cardSize();
#endif
}

uint64_t SdCard_GetFreeSize() {
#ifdef SD_MMC_1BIT_MODE
	return SD_MMC.cardSize() - SD_MMC.usedBytes();
#else
	return SD.cardSize() - SD.usedBytes();
#endif
}

void SdCard_PrintInfo() {
	// show SD card type
	sdcard_type_t cardType = SdCard_GetType();
	const char *type = "UNKNOWN";
	switch (cardType) {
		case CARD_MMC:
			type = "MMC";
			break;

		case CARD_SD:
			type = "SDSC";
			break;

		case CARD_SDHC:
			type = "SDHC";
			break;

		default:
			break;
	}
	Log_Printf(LOGLEVEL_DEBUG, "SD card type: %s", type);
	// show SD card size / free space
	uint64_t cardSize = SdCard_GetSize() / (1024 * 1024);
	uint64_t freeSize = SdCard_GetFreeSize() / (1024 * 1024);
	;
	Log_Printf(LOGLEVEL_NOTICE, sdInfo, cardSize, freeSize);
}

// Check if file-type is correct
bool fileValid(const char *_fileItem) {
	// make file extension to lowercase (compare case insenstive)
	char *lFileItem;
	lFileItem = x_strdup(_fileItem);
	if (lFileItem == NULL) {
		return false;
	}
	lFileItem = strlwr(lFileItem);
	const char ch = '/';
	char *subst;
	subst = strrchr(lFileItem, ch); // Don't use files that start with .
	bool isValid = (!startsWith(subst, (char *) "/.")) && (
					   // audio file formats
					   endsWith(lFileItem, ".mp3") || endsWith(lFileItem, ".aac") || endsWith(lFileItem, ".m4a") || endsWith(lFileItem, ".wav") || endsWith(lFileItem, ".flac") || endsWith(lFileItem, ".ogg") || endsWith(lFileItem, ".oga") || endsWith(lFileItem, ".opus") ||
					   // playlist file formats
					   endsWith(lFileItem, ".m3u") || endsWith(lFileItem, ".m3u8") || endsWith(lFileItem, ".pls") || endsWith(lFileItem, ".asx"));
	free(lFileItem);
	return isValid;
}

// Takes a directory as input and returns a random subdirectory from it
char *SdCard_pickRandomSubdirectory(char *_directory) {
	uint32_t listStartTimestamp = millis();

	// Look if file/folder requested really exists. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, _directory);
		return NULL;
	}
	Log_Printf(LOGLEVEL_NOTICE, tryToPickRandomDir, _directory);

	static uint8_t allocCount = 1;
	uint16_t allocSize = psramInit() ? 65535 : 1024; // There's enough PSRAM. So we don't have to care...
	uint16_t directoryCount = 0;
	char *buffer = _directory; // input char* is reused as it's content no longer needed
	char *subdirectoryList = (char *) x_calloc(allocSize, sizeof(char));

	if (subdirectoryList == NULL) {
		Log_Println(unableToAllocateMemForLinearPlaylist, LOGLEVEL_ERROR);
		System_IndicateError();
		return NULL;
	}

	// Create linear list of subdirectories with #-delimiters
	while (true) {
		bool isDir = false;
		String MyfileName = directory.getNextFileName(&isDir);
		if (MyfileName == "") {
			break;
		}
		if (!isDir) {
			continue;
		} else {
			strncpy(buffer, MyfileName.c_str(), 255);
			// Log_Printf(LOGLEVEL_INFO, nameOfFileFound, buffer);
			if ((strlen(subdirectoryList) + strlen(buffer) + 2) >= allocCount * allocSize) {
				char *tmp = (char *) realloc(subdirectoryList, ++allocCount * allocSize);
				Log_Println(reallocCalled, LOGLEVEL_DEBUG);
				if (tmp == NULL) {
					Log_Println(unableToAllocateMemForLinearPlaylist, LOGLEVEL_ERROR);
					System_IndicateError();
					free(subdirectoryList);
					return NULL;
				}
				subdirectoryList = tmp;
			}
			strcat(subdirectoryList, stringDelimiter);
			strcat(subdirectoryList, buffer);
			directoryCount++;
		}
	}
	strcat(subdirectoryList, stringDelimiter);

	if (!directoryCount) {
		free(subdirectoryList);
		return NULL;
	}

	uint16_t randomNumber = random(directoryCount) + 1; // Create random-number with max = subdirectory-count
	uint16_t delimiterFoundCount = 0;
	uint32_t a = 0;
	uint8_t b = 0;

	// Walk through subdirectory-array and extract randomized subdirectory
	while (subdirectoryList[a] != '\0') {
		if (subdirectoryList[a] == '#') {
			delimiterFoundCount++;
		} else {
			if (delimiterFoundCount == randomNumber) { // Pick subdirectory of linear char* according to random number
				buffer[b++] = subdirectoryList[a];
			}
		}
		if (delimiterFoundCount > randomNumber || (b == 254)) { // It's over when next delimiter is found or buffer is full
			buffer[b] = '\0';
			free(subdirectoryList);
			Log_Printf(LOGLEVEL_NOTICE, pickedRandomDir, _directory);
			return buffer; // Full path of random subdirectory
		}
		a++;
	}

	free(subdirectoryList);
	Log_Printf(LOGLEVEL_DEBUG, "pick random directory from SD-card finished: %lu ms", (millis() - listStartTimestamp));
	return NULL;
}

static bool SdCard_allocAndSave(Playlist *playlist, const String &s) {
	const size_t len = s.length() + 1;
	char *entry = static_cast<char *>(x_malloc(len));
	if (!entry) {
		// OOM, free playlist and return
		Log_Println(unableToAllocateMemForLinearPlaylist, LOGLEVEL_ERROR);
		freePlaylist(playlist);
		return false;
	}
	s.toCharArray(entry, len);
	playlist->push_back(entry);
	return true;
};

static std::optional<Playlist*> SdCard_ParseM3UPlaylist(File f, bool forceExtended = false) {
	const String line = f.readStringUntil('\n');
	const bool extended = line.startsWith("#EXTM3U") || forceExtended;
	Playlist *playlist = new Playlist();

	// reserve a sane amount of memory to reduce heap fragmentation
	playlist->reserve(64);
	if (extended) {
		// extended m3u file format
		// ignore all lines starting with '#'

		while (f.available()) {
			String line = f.readStringUntil('\n');
			if (!line.startsWith("#")) {
				// this something we have to save
				line.trim();
				// save string
				if (!SdCard_allocAndSave(playlist, line)) {
					return std::nullopt;
				}
			}
		}
		// resize std::vector memory to fit our count
		playlist->shrink_to_fit();
		return playlist;
	}

	// normal m3u is just a bunch of filenames, 1 / line
	f.seek(0);
	while (f.available()) {
		String line = f.readStringUntil('\n');
		// save string
		if (!SdCard_allocAndSave(playlist, line)) {
			return std::nullopt;
		}
	}
	// resize memory to fit our count
	playlist->shrink_to_fit();
	return playlist;
}

/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
std::optional<Playlist*> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode) {
	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, fileName);
		return std::nullopt;
	}

	Log_Printf(LOGLEVEL_DEBUG, freeMemory, ESP.getFreeHeap());

	// Parse m3u-playlist and create linear-playlist out of it
	if (_playMode == LOCAL_M3U) {
		if (fileOrDirectory && !fileOrDirectory.isDirectory() && fileOrDirectory.size() > 0) {
			// function takes care of everything
			return SdCard_ParseM3UPlaylist(fileOrDirectory);
		}
	}

	// if we reach here, this was not a m3u
	Log_Println(playlistGen, LOGLEVEL_NOTICE);
	Playlist *playlist = new Playlist;

	// File-mode
	if (!fileOrDirectory.isDirectory()) {
		if (!SdCard_allocAndSave(playlist, fileOrDirectory.path())) {
			// OOM, function already took care of house cleaning
			return std::nullopt;
		}
		return playlist;
	}

	// Directory-mode (linear-playlist)
	playlist->reserve(64); // reserve a sane amount of memory to reduce the number of reallocs
	while (true) {
		bool isDir;
		const String name = fileOrDirectory.getNextFileName(&isDir);
		if (name.isEmpty()) {
			break;
		}
		if (isDir) {
			continue;
		}
		// Don't support filenames that start with "." and only allow .mp3 and other supported audio file formats
		if (fileValid(name.c_str())) {
			// save it to the vector
			if (!SdCard_allocAndSave(playlist, name)) {
				// OOM, function already took care of house cleaning
				return std::nullopt;
			}
		}
	}
	playlist->shrink_to_fit();

	Log_Printf(LOGLEVEL_NOTICE, numberOfValidFiles, playlist->size());
	return playlist;
}
