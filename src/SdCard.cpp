#include <Arduino.h>
#include "settings.h"

#include "SdCard.h"

#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

#include "playlists/CacheFilePlaylist.hpp"
#include "playlists/FolderPlaylist.hpp"
#include "playlists/WebstreamPlaylist.hpp"

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
std::optional<const String> SdCard_pickRandomSubdirectory(const char *_directory) {
	uint32_t listStartTimestamp = millis();

	// Look if file/folder requested really exists. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory) {
		// does not exists
		Log_Println(dirOrFileDoesNotExist, LOGLEVEL_ERROR);
		return std::nullopt;
	}
	Log_Printf(LOGLEVEL_NOTICE, tryToPickRandomDir, _directory);

	size_t dirCount = 0;
	while(true) {
		bool isDir;
		#if defined(HAS_FILEEXPLORER_SPEEDUP) || (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 8))
			const String path = directory.getNextFileName(&isDir);
			if(path.isEmpty()) {
				break;
			}
		#else
			File fileItem = directory.openNextFile();
			if(!fileItem) {
				break;
			}
			isDir = fileItem.isDirectory();
		#endif
		if(isDir) {
			dirCount++;
		}
	}
	if(!dirCount) {
		// no paths in folder
		return std::nullopt;
	}

	const uint32_t randomNumber = esp_random() % dirCount;
	String path;
	for(size_t i=0;i<randomNumber;) {
		bool isDir;
		#if defined(HAS_FILEEXPLORER_SPEEDUP) || (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 8))
			path = directory.getNextFileName(&isDir);
		#else
			File fileItem = directory.openNextFile();
			if(!fileItem) {
				path = "";
			} else {
				path = getPath(fileItem);
				isDir = fileItem.isDirectory();
			}
		#endif
		if(path.isEmpty()) {
			// we reached the end before finding the correct dir!
			return std::nullopt;
		}
		if(isDir) {
			i++;
		}
	}
	Log_Printf(LOGLEVEL_NOTICE, pickedRandomDir, path.c_str());	
	Log_Printf(LOGLEVEL_DEBUG, "pick random directory from SD-card finished: %lu ms", (millis() - listStartTimestamp));
	return path;
}

static std::optional<Playlist*> SdCard_ParseM3UPlaylist(File f, bool forceExtended = false) {
	const String line = f.readStringUntil('\n');
	bool extended = line.startsWith("#EXTM3U") || forceExtended;
	FolderPlaylist *playlist = new FolderPlaylist();

	if(extended) {
        // extended m3u file format
        // ignore all lines starting with '#'

        while(f.available()) {
            String line = f.readStringUntil('\n');
            if(!line.startsWith("#")){
                // this something we have to save
                line.trim(); 
                if(!playlist->push_back(line)) {
					delete playlist;
                    return std::nullopt;
                }
            }
        }
        // resize memory to fit our count
		playlist->compress();
        return playlist;
	}

	// normal m3u is just a bunch of filenames, 1 / line
	f.seek(0);
	while(f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if(!playlist->push_back(line)) {
			delete playlist;
			return std::nullopt;
		}
	}
	// resize memory to fit our count
	playlist->compress();
	return playlist;
}

/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
std::optional<Playlist*> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode) {
	bool rebuildCacheFile = false;

	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Println(dirOrFileDoesNotExist, LOGLEVEL_ERROR);
		return std::nullopt;
	}

	// Create linear playlist of caching-file
	#ifdef CACHED_PLAYLIST_ENABLE
		// Build absolute path of cacheFile
		auto cacheFilePath = CacheFilePlaylist::getCachefilePath(fileOrDirectory);

		// Decide if to use cacheFile. It needs to exist first check if cacheFile (already) exists
		// ...and playmode has to be != random/single (as random along with caching doesn't make sense at all)
		if (cacheFilePath && gFSystem.exists(cacheFilePath.value()) && _playMode != SINGLE_TRACK && _playMode != SINGLE_TRACK_LOOP) {
			// Read linear playlist (csv with #-delimiter) from cachefile (faster!)

			File cacheFile = gFSystem.open(cacheFilePath.value());
			if (cacheFile && cacheFile.size()) {
				CacheFilePlaylist *cachePlaylist = new CacheFilePlaylist();
				
				bool success = cachePlaylist->deserialize(cacheFile);
				if(success) {
					// always first assume a current playlist format
					return cachePlaylist;
				}
				// we had some error reading the cache file, wait for the other to rebuild it
				// we do not need the class anymore, so destroy it
				delete cachePlaylist;
			}
			// we failed to read the cache file... set the flag to rebuild it
			rebuildCacheFile = true;
		}
	#endif

	Log_Printf(LOGLEVEL_DEBUG, freeMemory, ESP.getFreeHeap());

	// Parse m3u-playlist and create linear-playlist out of it
	if (_playMode == LOCAL_M3U) {
		if (fileOrDirectory && !fileOrDirectory.isDirectory() && fileOrDirectory.size()) {
			// create a m3u playlist and parse the file
			return SdCard_ParseM3UPlaylist(fileOrDirectory);
		}
		// if we reach here, we failed
		return std::nullopt;
	}

	// If we reached here, we did not read a cache file nor an m3u file. Means: read filenames from SD and make playlist of it
	Log_Println(playlistGenModeUncached, LOGLEVEL_NOTICE);

	// File-mode
	if (!fileOrDirectory.isDirectory()) {
		Log_Println(fileModeDetected, LOGLEVEL_INFO);
		const char *path = getPath(fileOrDirectory);
		if (fileValid(path)) {
			return new WebstreamPlaylist(path);
		}
	}

	// Folder mode
	FolderPlaylist *playlist = new FolderPlaylist();
	playlist->createFromFolder(fileOrDirectory);
	if (!playlist->isValid()) {
		// something went wrong
		Log_Println(unableToAllocateMemForLinearPlaylist, LOGLEVEL_ERROR);
		delete playlist;
		return std::nullopt;
	}

	#ifdef CACHED_PLAYLIST_ENABLE
		if(cacheFilePath && rebuildCacheFile) {
			File cacheFile = gFSystem.open(cacheFilePath.value(), FILE_WRITE);
			if(cacheFile) {
				CacheFilePlaylist::serialize(cacheFile, *playlist);
			}
			cacheFile.close();
		}
	#endif

	// we are finished
	Log_Printf(LOGLEVEL_NOTICE, numberOfValidFiles, playlist->size());
	return playlist;
}
