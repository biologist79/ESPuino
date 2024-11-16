#include <Arduino.h>
#include "settings.h"

#include "SdCard.h"

#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

#include <esp_random.h>

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
	// clang-format off
	// all supported extension
	constexpr std::array audioFileSufix = {
		".mp3",
		".aac",
		".m4a",
		".wav",
		".flac",
		".ogg",
		".oga",
		".opus",
		// playlists
		".m3u",
		".m3u8",
		".pls",
		".asx"
	};
	// clang-format on
	constexpr size_t maxExtLen = strlen(*std::max_element(audioFileSufix.begin(), audioFileSufix.end(), [](const char *a, const char *b) {
		return strlen(a) < strlen(b);
	}));

	if (!_fileItem || !strlen(_fileItem)) {
		// invalid entry
		return false;
	}

	// check for streams
	if (strncmp(_fileItem, "http://", strlen("http://")) == 0 || strncmp(_fileItem, "https://", strlen("https://")) == 0) {
		// this is a stream
		return true;
	}

	// check for files which start with "/."
	const char *lastSlashPtr = strrchr(_fileItem, '/');
	if (lastSlashPtr == nullptr) {
		// we have a relative filename without any slashes...
		// set the pointer so that it points to the first character AFTER a +1
		lastSlashPtr = _fileItem - 1;
	}
	if (*(lastSlashPtr + 1) == '.') {
		// we have a hidden file
		// Log_Printf(LOGLEVEL_DEBUG, "File is hidden: %s", _fileItem);
		return false;
	}

	// extract the file extension
	const char *extStartPtr = strrchr(_fileItem, '.');
	if (extStartPtr == nullptr) {
		// no extension found
		// Log_Printf(LOGLEVEL_DEBUG, "File has no extension: %s", _fileItem);
		return false;
	}
	const size_t extLen = strlen(extStartPtr);
	if (extLen > maxExtLen) {
		// extension too long, we do not care anymore
		// Log_Printf(LOGLEVEL_DEBUG, "File not supported (extension to long): %s", _fileItem);
		return false;
	}
	char extBuffer[maxExtLen + 1] = {0};
	memcpy(extBuffer, extStartPtr, extLen);

	// make the extension lower case (without using non standard C functions)
	for (size_t i = 0; i < extLen; i++) {
		extBuffer[i] = tolower(extBuffer[i]);
	}

	// check extension against all supported values
	for (const auto &e : audioFileSufix) {
		if (strcmp(extBuffer, e) == 0) {
			// hit we found the extension
			return true;
		}
	}
	// miss, we did not find the extension
	// Log_Printf(LOGLEVEL_DEBUG, "File not supported: %s", _fileItem);
	return false;
}

// Takes a directory as input and returns a random subdirectory from it
const String SdCard_pickRandomSubdirectory(const char *_directory) {
	// Look if folder requested really exists and is a folder. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory || !directory.isDirectory()) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, _directory);
		return String();
	}
	Log_Printf(LOGLEVEL_NOTICE, tryToPickRandomDir, _directory);

	// iterate through and count all dirs
	size_t dirCount = 0;
	while (1) {
		bool isDir;
		const String name = directory.getNextFileName(&isDir);
		if (name.isEmpty()) {
			break;
		}
		if (isDir) {
			dirCount++;
		}
	}
	if (!dirCount) {
		// no paths in folder
		return String();
	}

	const uint32_t randomNumber = esp_random() % dirCount;
	directory.rewindDirectory();
	dirCount = 0;
	while (1) {
		bool isDir;
		const String name = directory.getNextFileName(&isDir);
		if (name.isEmpty()) {
			break;
		}
		if (isDir) {
			if (dirCount == randomNumber) {
				return name;
			}
			dirCount++;
		}
	}

	// if we reached here, something went wrong
	return String();
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

static std::optional<Playlist *> SdCard_ParseM3UPlaylist(File file) {
	Playlist *playlist = new Playlist();

	// reserve a sane amount of memory to reduce heap fragmentation
	playlist->reserve(64);
	// normal m3u is just a bunch of filenames, 1 / line
	// extended m3u file format can also include comments or special directives, prefaced by the "#" character
	// -> ignore all lines starting with '#'

	while (file.available()) {
		String line = file.readStringUntil('\n');
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

/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
std::optional<Playlist *> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode) {
	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, fileName);
		return std::nullopt;
	}

	Log_Printf(LOGLEVEL_DEBUG, freeMemory, ESP.getFreeHeap());

	// Parse m3u-playlist and create linear-playlist out of it
	if (_playMode == LOCAL_M3U) {
		if (!fileOrDirectory.isDirectory() && fileOrDirectory.size() > 0) {
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
	size_t hiddenFiles = 0;
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
		} else {
			hiddenFiles++;
		}
	}
	playlist->shrink_to_fit();

	Log_Printf(LOGLEVEL_NOTICE, numberOfValidFiles, playlist->size());
	Log_Printf(LOGLEVEL_DEBUG, "Hidden files: %u", hiddenFiles);
	return playlist;
}
