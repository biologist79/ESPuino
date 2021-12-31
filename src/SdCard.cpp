#include <Arduino.h>
#include "settings.h"
#include "SdCard.h"
#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

#ifdef SD_MMC_1BIT_MODE
    fs::FS gFSystem = (fs::FS)SD_MMC;
#else
    SPIClass spiSD(HSPI);
    fs::FS gFSystem = (fs::FS)SD;
#endif

void SdCard_Init(void) {
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
                Log_Println((char *) FPSTR(unableToMountSd), LOGLEVEL_ERROR);
                delay(500);
    #ifdef SHUTDOWN_IF_SD_BOOT_FAILS
                if (millis() >= deepsleepTimeAfterBootFails * 1000) {
                    Log_Println((char *) FPSTR(sdBootFailedDeepsleep), LOGLEVEL_ERROR);
                    esp_deep_sleep_start();
                }
    #endif
            }
}

void SdCard_Exit(void) {
    // SD card goto idle mode
    #ifdef SD_MMC_1BIT_MODE
        SD_MMC.end();
    #endif
}

sdcard_type_t SdCard_GetType(void) {
    sdcard_type_t cardType;
    #ifdef SD_MMC_1BIT_MODE
        Log_Println((char *) FPSTR(sdMountedMmc1BitMode), LOGLEVEL_NOTICE);
        cardType = SD_MMC.cardType();
    #else
        Log_Println((char *) FPSTR(sdMountedSpiMode), LOGLEVEL_NOTICE);
        cardType = SD.cardType();
    #endif
        return cardType;
}

// Check if file-type is correct
bool fileValid(const char *_fileItem) {
    const char ch = '/';
    char *subst;
    subst = strrchr(_fileItem, ch); // Don't use files that start with .

    return (!startsWith(subst, (char *) "/.")) &&
           (endsWith(_fileItem, ".mp3") || endsWith(_fileItem, ".MP3") ||
            endsWith(_fileItem, ".aac") || endsWith(_fileItem, ".AAC") ||
            endsWith(_fileItem, ".m3u") || endsWith(_fileItem, ".M3U") ||
            endsWith(_fileItem, ".m4a") || endsWith(_fileItem, ".M4A") ||
            endsWith(_fileItem, ".wav") || endsWith(_fileItem, ".WAV") ||
            endsWith(_fileItem, ".flac") || endsWith(_fileItem, ".FLAC") ||
            endsWith(_fileItem, ".asx") || endsWith(_fileItem, ".ASX"));
}

/* Puts SD-file(s) or directory into a playlist
    First element of array always contains the number of payload-items. */
char **SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode) {
    static char **files;
    char *serializedPlaylist;
    char fileNameBuf[255];
    char cacheFileNameBuf[275];
    bool readFromCacheFile = false;
    bool enablePlaylistCaching = false;
    bool enablePlaylistFromM3u = false;

    // Look if file/folder requested really exists. If not => break.
    File fileOrDirectory = gFSystem.open(fileName);
    if (!fileOrDirectory) {
        Log_Println((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
        return NULL;
    }

    // Create linear playlist of caching-file
    #ifdef CACHED_PLAYLIST_ENABLE
        strncpy(cacheFileNameBuf, fileName, sizeof(cacheFileNameBuf));
        strcat(cacheFileNameBuf, "/");
        strcat(cacheFileNameBuf, (const char*) FPSTR(playlistCacheFile));       // Build absolute path of cacheFile

        // Decide if to use cacheFile. It needs to exist first...
        if (gFSystem.exists(cacheFileNameBuf)) {     // Check if cacheFile (already) exists
            readFromCacheFile = true;
        }

        // ...and playmode has to be != random/single (as random along with caching doesn't make sense at all)
        if (_playMode == SINGLE_TRACK ||
            _playMode == SINGLE_TRACK_LOOP) {
                readFromCacheFile = false;
        } else {
            enablePlaylistCaching = true;
        }

        // Read linear playlist (csv with #-delimiter) from cachefile (faster!)
        if (readFromCacheFile) {
            File cacheFile = gFSystem.open(cacheFileNameBuf);
            if (cacheFile) {
                uint32_t cacheFileSize = cacheFile.size();

                if (!(cacheFileSize >= 1)) {        // Make sure it's greater than 0 bytes
                    Log_Println((char *) FPSTR(playlistCacheFoundBut0), LOGLEVEL_ERROR);
                    readFromCacheFile = false;
                } else {
                    Log_Println((char *) FPSTR(playlistGenModeCached), LOGLEVEL_NOTICE);
                    serializedPlaylist = (char *) x_calloc(cacheFileSize+10, sizeof(char));

                    char buf;
                    uint32_t fPos = 0;
                    while (cacheFile.available() > 0) {
                        buf = cacheFile.read();
                        serializedPlaylist[fPos++] = buf;
                    }
                }
                cacheFile.close();
            }
        }
    #endif

    snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeMemory), ESP.getFreeHeap());
    Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

    if (files != NULL) { // If **ptr already exists, de-allocate its memory
        Log_Println((char *) FPSTR(releaseMemoryOfOldPlaylist), LOGLEVEL_DEBUG);
        --files;
        freeMultiCharArray(files, strtoul(*files, NULL, 10));
        snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeMemoryAfterFree), ESP.getFreeHeap());
        Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
    }

    // Parse m3u-playlist and create linear-playlist out of it
    if (_playMode == LOCAL_M3U) {
        if (fileOrDirectory && !fileOrDirectory.isDirectory() && fileOrDirectory.size() >= 0) {
            enablePlaylistFromM3u = true;
            uint16_t allocCount = 1;
            uint16_t allocSize = 1024;
            if (psramInit()) {
                allocSize = 65535; // There's enough PSRAM. So we don't have to care...
            }

            serializedPlaylist = (char *) x_calloc(allocSize, sizeof(char));
            if (serializedPlaylist == NULL) {
                Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
                System_IndicateError();
                return files;
            }
            char buf;
            char lastBuf = '#';
            uint32_t fPos = 1;

            serializedPlaylist[0] = '#';
            while (fileOrDirectory.available() > 0) {
                buf = fileOrDirectory.read();
                if (fPos+1 >= allocCount * allocSize) {
                    serializedPlaylist = (char *) realloc(serializedPlaylist, ++allocCount * allocSize);
                    Log_Println((char *) FPSTR(reallocCalled), LOGLEVEL_DEBUG);
                    if (serializedPlaylist == NULL) {
                        Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
                        System_IndicateError();
                        free(serializedPlaylist);
                        return files;
                    }
                }

                if (buf != '\n' && buf != '\r') {
                    serializedPlaylist[fPos++] = buf;
                    lastBuf = buf;
                } else {
                    if (lastBuf != '#') {   // Strip empty lines from m3u
                        serializedPlaylist[fPos++] = '#';
                        lastBuf = '#';
                    }
                }
            }
            if (serializedPlaylist[fPos-1] == '#') {    // Remove trailing delimiter if set
                serializedPlaylist[fPos-1] = '\0';
            }
        } else {
            return files;
        }
    }

    // Don't read from cachefile or m3u-file. Means: read filenames from SD and make playlist of it
    if (!readFromCacheFile && !enablePlaylistFromM3u) {
        Log_Println((char *) FPSTR(playlistGenModeUncached), LOGLEVEL_NOTICE);
        // File-mode
        if (!fileOrDirectory.isDirectory()) {
            files = (char **) x_malloc(sizeof(char *) * 2);
            if (files == NULL) {
                Log_Println((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
                System_IndicateError();
                return NULL;
            }
            Log_Println((char *) FPSTR(fileModeDetected), LOGLEVEL_INFO);
            strncpy(fileNameBuf, (char *) fileOrDirectory.name(), sizeof(fileNameBuf) / sizeof(fileNameBuf[0]));
            if (fileValid(fileNameBuf)) {
                files = (char **) x_malloc(sizeof(char *) * 2);
                files[1] = x_strdup(fileNameBuf);
            }
            files[0] = x_strdup("1"); // Number of files is always 1 in file-mode

            return ++files;
        }

        // Directory-mode (linear-playlist)
        uint16_t allocCount = 1;
        uint16_t allocSize = 4096;
        if (psramInit()) {
            allocSize = 65535; // There's enough PSRAM. So we don't have to care...
        }

        serializedPlaylist = (char *) x_calloc(allocSize, sizeof(char));
        File cacheFile;
        if (enablePlaylistCaching) {
            cacheFile = gFSystem.open(cacheFileNameBuf, FILE_WRITE);
        }

        while (true) {
            File fileItem = fileOrDirectory.openNextFile();
            if (!fileItem) {
                break;
            }
            if (fileItem.isDirectory()) {
                continue;
            } else {
                strncpy(fileNameBuf, (char *) fileItem.name(), sizeof(fileNameBuf) / sizeof(fileNameBuf[0]));

                // Don't support filenames that start with "." and only allow .mp3
                if (fileValid(fileNameBuf)) {
                    /*snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(nameOfFileFound), fileNameBuf);
                    Log_Println(Log_Buffer, LOGLEVEL_INFO);*/
                    if ((strlen(serializedPlaylist) + strlen(fileNameBuf) + 2) >= allocCount * allocSize) {
                        serializedPlaylist = (char *) realloc(serializedPlaylist, ++allocCount * allocSize);
                        Log_Println((char *) FPSTR(reallocCalled), LOGLEVEL_DEBUG);
                        if (serializedPlaylist == NULL) {
                            Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
                            System_IndicateError();
                            return files;
                        }
                    }
                    strcat(serializedPlaylist, stringDelimiter);
                    strcat(serializedPlaylist, fileNameBuf);
                    if (cacheFile && enablePlaylistCaching) {
                        cacheFile.print(stringDelimiter);
                        cacheFile.print(fileNameBuf);       // Write linear playlist to cacheFile
                    }
                }
            }
        }

        if (cacheFile && enablePlaylistCaching) {
            cacheFile.close();
        }
    }

    // Get number of elements out of serialized playlist
    uint32_t cnt = 0;
    for (uint32_t k = 0; k < (strlen(serializedPlaylist)); k++) {
        if (serializedPlaylist[k] == '#') {
            cnt++;
        }
    }

    // Alloc only necessary number of playlist-pointers
    files = (char **) x_malloc(sizeof(char *) * cnt + 1);

    if (files == NULL) {
        Log_Println((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
        System_IndicateError();
        free(serializedPlaylist);
        return NULL;
    }

    // Extract elements out of serialized playlist and copy to playlist
    char *token;
    token = strtok(serializedPlaylist, stringDelimiter);
    uint32_t pos = 1;
    while (token != NULL) {
        files[pos++] = x_strdup(token);
        token = strtok(NULL, stringDelimiter);
    }

    free(serializedPlaylist);

    files[0] = (char *) x_malloc(sizeof(char) * 5);

    if (files[0] == NULL) {
        Log_Println((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
        System_IndicateError();
        return NULL;
    }
    sprintf(files[0], "%u", cnt);
    snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(numberOfValidFiles), cnt);
    Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

    return ++files; // return ptr+1 (starting at 1st payload-item); ptr+0 contains number of items
}
