#include <Arduino.h>
#include "settings.h"

#include "SdCard.h"

#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

#include <esp_random.h>
#include <esp_vfs_fat.h>

#ifdef SD_MMC_1BIT_MODE
fs::FS gFSystem = (fs::FS) SD_MMC;
#else
SPIClass spiSD(HSPI);
fs::FS gFSystem = (fs::FS) SD;
#endif

uint8_t maxRecursionDepth;

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

	// Used when building recursive playlists
	maxRecursionDepth = gPrefsSettings.getUInt("nvsRecDepth", 255);
	if (maxRecursionDepth == 255) {
		gPrefsSettings.putUInt("nvsRecDepth", 2);
		maxRecursionDepth = 2;
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

uint8_t SdCard_GetMaxRecursionDepth(void) {
	return maxRecursionDepth;
}

// Returns recursion depth that's used then playlists are generated for recursive playmodes
size_t SdCard_SetMaxRecursionDepth(uint8_t _maxRecursionDepth) {
	maxRecursionDepth = _maxRecursionDepth;
	return gPrefsSettings.putUInt("nvsRecDepth", SdCard_GetMaxRecursionDepth());
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
	Playlist *playlist = allocatePlaylist();

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
std::optional<Playlist *> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode, const uint8_t _maxRecursionDepth, bool _recursionMode) {
	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, fileName);
		return std::nullopt;
	}

	// Parse m3u-playlist and create linear-playlist out of it
	if (_playMode == LOCAL_M3U) {
		if (!fileOrDirectory.isDirectory() && fileOrDirectory.size() > 0) {
			// function takes care of everything
			return SdCard_ParseM3UPlaylist(fileOrDirectory);
		}
	}

	// if we reach this code, it was not a m3u

	static Playlist *playlist = nullptr; // static because of possible recursion
	if (_recursionMode == false) {
		Log_Printf(LOGLEVEL_DEBUG, freeMemory, ESP.getFreeHeap());
		playlist = allocatePlaylist();
		Log_Printf(LOGLEVEL_NOTICE, playlistRecDepth, _maxRecursionDepth);
	}

	static uint8_t currentRecDepth = 0;

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
			//  Jump into directory if recursion is allowed
			if (currentRecDepth < _maxRecursionDepth) {
				currentRecDepth++;
				// Log_Printf(LOGLEVEL_DEBUG, "Added folder: %s, depth of recursion: %d\n", name.c_str(), currentRecDepth);
				SdCard_ReturnPlaylist(name.c_str(), _playMode, _maxRecursionDepth, true);
				currentRecDepth--;
			} else {
				continue;
			}
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

	// Only show sum up at last run (when no recursion is active)
	if (!_recursionMode) {
		Log_Printf(LOGLEVEL_NOTICE, numberOfValidFiles, playlist->size());
		Log_Printf(LOGLEVEL_DEBUG, "Hidden files: %u", hiddenFiles);
	}

	return playlist;
}

// Extracts basepath out of a given filepath
std::string_view SdCard_Basepath(const char *filepath) {
	if (!filepath) {
		return std::string_view();
	}
	std::string_view str(filepath);
	auto pos = str.find_last_of('/');
	if (pos == std::string::npos) {
		return std::string_view();
	}
	return str.substr(0, pos + 1);
}

// Used for recursive playmodes. Allows to jump forwards and backwards between folders using
// CMD_PREVFOLDER (backwards) and CMD_NEXTFOLDER (forwards) to previous / next folder in playlist.
// Returns -1 if no prev or next folder was found or no playlist is available
// Returns >=0 if folderjump is possible. Number represents the index of the current playlist's track to jump to.
int16_t SdCard_findNextOrPrevDirectoryTrack(const Playlist &_playlist, size_t currentTrackIndexInPlaylist, SearchDirection direction) {
	// Look if index requested is out of bounds
	if (currentTrackIndexInPlaylist >= _playlist.size()) {
		return -1;
	}

	std::string_view basepathOfCurrentTrack = SdCard_Basepath(_playlist[currentTrackIndexInPlaylist]); // Get basepath of current track

	// Look forwards
	if (direction == SearchDirection::Forward) {
		if (_playlist[currentTrackIndexInPlaylist] != nullptr) {
			for (uint16_t i = (currentTrackIndexInPlaylist + 1); i < _playlist.size(); ++i) { // Iterate through playlist and start with current track +1
				std::string_view basepathOfTrackToLookUp = SdCard_Basepath(_playlist[i]);
				if (basepathOfTrackToLookUp != basepathOfCurrentTrack) {
					Log_Printf(LOGLEVEL_DEBUG, jumpForwardsToFolder, basepathOfTrackToLookUp.data(), "\n");
					return i; // Return first track after basepath change
				}
			}
		} else {
			return -1;
		}

		// Look backwards
	} else if (direction == SearchDirection::Backward) {
		//  Go back as long as we don't hit 0
		if (!currentTrackIndexInPlaylist) {
			return currentTrackIndexInPlaylist;
		}

		if (_playlist[currentTrackIndexInPlaylist] != nullptr) {
			for (uint16_t i = (currentTrackIndexInPlaylist - 1); i > 0; i--) {
				std::string_view basepathOfTrackToLookUp = SdCard_Basepath(_playlist[i]);
				if (basepathOfTrackToLookUp != basepathOfCurrentTrack) { // Look for the 1st basepath change...
					for (uint16_t j = i - 1; j > 0; j--) {
						std::string_view basepathOfTrackToLookUpInner = SdCard_Basepath(_playlist[j]);
						if (basepathOfTrackToLookUpInner != basepathOfTrackToLookUp) { // ...but keep on looking for the 2nd change...
							Log_Printf(LOGLEVEL_DEBUG, jumpBackwardsToFolder, basepathOfTrackToLookUpInner.data(), "\n");
							return j + 1; // ...just to add +1 to get the previous element before the 2nd change
						}
					}
				}
			}
		} else {
			return -1;
		}
		// If index 0 (first track) was hit meanwhile -> return it!
		return 0;
	}

	// If no jump possible, return -1
	return -1;
}

const String SdCard_GetVolumeLabel() {
#if FF_USE_LABEL
	char label[24];
	memset(label, 0, sizeof(label));

	DWORD vsn = 0;
	FRESULT res = f_getlabel("", label, &vsn);

	if (res == FR_OK && strlen(label) > 0) {
		return String(label);
	}
#endif
	return String("/");
}
