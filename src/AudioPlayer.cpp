#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"

#include "Audio.h"
#include "Bluetooth.h"
#include "Cmd.h"
#include "Common.h"
#include "EnumUtils.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Port.h"
#include "Queues.h"
#include "Rfid.h"
#include "RotaryEncoder.h"
#include "SdCard.h"
#include "System.h"
#include "Web.h"
#include "Wlan.h"
#include "main.h"
#include "strnatcmp.h"

#include <esp_task_wdt.h>
#include <freertos/task.h>
#include <random>

#define AUDIOPLAYER_VOLUME_MAX	21u
#define AUDIOPLAYER_VOLUME_MIN	0u
#define AUDIOPLAYER_VOLUME_INIT 3u

playProps gPlayProperties;
TaskHandle_t AudioTaskHandle;
// uint32_t cnt123 = 0;

// Playlist
static playlistSortMode AudioPlayer_PlaylistSortMode = AUDIOPLAYER_PLAYLIST_SORT_MODE_DEFAULT;

// Volume
static uint8_t AudioPlayer_CurrentVolume = AUDIOPLAYER_VOLUME_INIT;
static uint8_t AudioPlayer_MaxVolume = AUDIOPLAYER_VOLUME_MAX;
static uint8_t AudioPlayer_MaxVolumeSpeaker = AUDIOPLAYER_VOLUME_MAX;
static uint8_t AudioPlayer_MinVolume = AUDIOPLAYER_VOLUME_MIN;
static uint8_t AudioPlayer_InitVolume = AUDIOPLAYER_VOLUME_INIT;

// current playtime
uint32_t AudioPlayer_CurrentTime;
uint32_t AudioPlayer_FileDuration;

// Playtime stats
time_t playTimeSecTotal = 0;
time_t playTimeSecSinceStart = 0;

// current station logo url
static String AudioPlayer_StationLogoUrl;

#ifdef HEADPHONE_ADJUST_ENABLE
static bool AudioPlayer_HeadphoneLastDetectionState;
static uint32_t AudioPlayer_HeadphoneLastDetectionTimestamp = 0u;
static uint8_t AudioPlayer_MaxVolumeHeadphone = 11u; // Maximum volume that can be adjusted in headphone-mode (default; can be changed later via GUI)
#endif

static void AudioPlayer_Task(void *parameter);
static void AudioPlayer_HeadphoneVolumeManager(void);
static std::optional<Playlist *> AudioPlayer_ReturnPlaylistFromWebstream(const char *_webUrl);
static bool AudioPlayer_ArrSortHelper_strcmp(const char *a, const char *b);
static bool AudioPlayer_ArrSortHelper_strnatcmp(const char *a, const char *b);
static bool AudioPlayer_ArrSortHelper_strnatcasecmp(const char *a, const char *b);
static void AudioPlayer_SortPlaylist(Playlist *playlist);
static void AudioPlayer_RandomizePlaylist(Playlist *playlist);
static size_t AudioPlayer_NvsRfidWriteWrapper(const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks);
static void AudioPlayer_ClearCover(void);

void AudioPlayer_Init(void) {
	// load playtime total from NVS
	playTimeSecTotal = gPrefsSettings.getULong("playTimeTotal", 0);

	uint8_t playListSortModeValue = gPrefsSettings.getUChar("PLSortMode", EnumUtils::underlying_value(AudioPlayer_PlaylistSortMode));
	AudioPlayer_PlaylistSortMode = EnumUtils::to_enum<playlistSortMode>(playListSortModeValue);

	uint32_t nvsInitialVolume;
	if (!gPrefsSettings.getBool("recoverVolBoot", false)) {
		// Get initial volume from NVS
		nvsInitialVolume = gPrefsSettings.getUInt("initVolume", 0);
	} else {
		// Get volume used at last shutdown
		nvsInitialVolume = gPrefsSettings.getUInt("previousVolume", 999);
		if (nvsInitialVolume == 999) {
			gPrefsSettings.putUInt("previousVolume", AudioPlayer_GetInitVolume());
			nvsInitialVolume = AudioPlayer_GetInitVolume();
		} else {
			Log_Println(rememberLastVolume, LOGLEVEL_ERROR);
		}
	}

	if (nvsInitialVolume) {
		AudioPlayer_SetInitVolume(nvsInitialVolume);
		Log_Printf(LOGLEVEL_INFO, restoredInitialLoudnessFromNvs, nvsInitialVolume);
	} else {
		gPrefsSettings.putUInt("initVolume", AudioPlayer_GetInitVolume());
		Log_Println(wroteInitialLoudnessToNvs, LOGLEVEL_ERROR);
	}

	// Get maximum volume for speaker from NVS
	uint32_t nvsMaxVolumeSpeaker = gPrefsSettings.getUInt("maxVolumeSp", 0);
	if (nvsMaxVolumeSpeaker) {
		AudioPlayer_SetMaxVolumeSpeaker(nvsMaxVolumeSpeaker);
		AudioPlayer_SetMaxVolume(nvsMaxVolumeSpeaker);
		Log_Printf(LOGLEVEL_INFO, restoredMaxLoudnessForSpeakerFromNvs, nvsMaxVolumeSpeaker);
	} else {
		// Set max volume to max per default. Can be adjusted later via webinterface.
		gPrefsSettings.putUInt("maxVolumeSp", 21);
		Log_Println(wroteMaxLoudnessForSpeakerToNvs, LOGLEVEL_ERROR);
	}

#ifdef HEADPHONE_ADJUST_ENABLE
	#if (HP_DETECT >= 0 && HP_DETECT <= MAX_GPIO)
	pinMode(HP_DETECT, INPUT_PULLUP);
	#endif
	AudioPlayer_HeadphoneLastDetectionState = Audio_Detect_Mode_HP(Port_Read(HP_DETECT));

	// Get maximum volume for headphone from NVS
	uint32_t nvsAudioPlayer_MaxVolumeHeadphone = gPrefsSettings.getUInt("maxVolumeHp", 0);
	if (nvsAudioPlayer_MaxVolumeHeadphone) {
		AudioPlayer_MaxVolumeHeadphone = nvsAudioPlayer_MaxVolumeHeadphone;
		Log_Printf(LOGLEVEL_INFO, restoredMaxLoudnessForHeadphoneFromNvs, nvsAudioPlayer_MaxVolumeHeadphone);
	} else {
		// Set max volume to max per default. Can be adjusted later via webinterface.
		gPrefsSettings.putUInt("maxVolumeHp", 21);
		Log_Println(wroteMaxLoudnessForHeadphoneToNvs, LOGLEVEL_ERROR);
	}
#endif
	// Adjust volume depending on headphone is connected and volume-adjustment is enabled
	AudioPlayer_SetupVolumeAndAmps();

	// initialize gPlayProperties
	gPlayProperties = {};
	gPlayProperties.playlistFinished = true;

	// clear title and cover image
	gPlayProperties.title[0] = '\0';
	gPlayProperties.coverFilePos = 0;
	AudioPlayer_StationLogoUrl = "";
	gPlayProperties.playlist = new Playlist();
	gPlayProperties.SavePlayPosRfidChange = gPrefsSettings.getBool("savePosRfidChge", false); // SAVE_PLAYPOS_WHEN_RFID_CHANGE
	gPlayProperties.pauseOnMinVolume = gPrefsSettings.getBool("pauseOnMinVol", false); // PAUSE_ON_MIN_VOLUME

	// Don't start audio-task in BT-speaker mode!
	if ((System_GetOperationMode() == OPMODE_NORMAL) || (System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE)) {
		xTaskCreatePinnedToCore(
			AudioPlayer_Task, /* Function to implement the task */
			"mp3play", /* Name of the task */
			6000, /* Stack size in words */
			NULL, /* Task input parameter */
			2 | portPRIVILEGE_BIT, /* Priority of the task */
			&AudioTaskHandle, /* Task handle. */
			1 /* Core where the task should run */
		);
	}
}

void AudioPlayer_Exit(void) {
	Log_Println("shutdown audioplayer..", LOGLEVEL_NOTICE);
	// save playtime total to NVS
	playTimeSecTotal += playTimeSecSinceStart;
	gPrefsSettings.putULong("playTimeTotal", playTimeSecTotal);
	// Make sure last playposition for audiobook is saved when playback is active while shutdown was initiated
	if (gPrefsSettings.getBool("savePosShutdown", false) && !gPlayProperties.pausePlay && (gPlayProperties.playMode == AUDIOBOOK || gPlayProperties.playMode == AUDIOBOOK_LOOP)) {
		AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
		while (!gPlayProperties.pausePlay) { // Make sure to wait until playback is paused in order to be sure that playposition saved in NVS
			vTaskDelay(portTICK_PERIOD_MS * 100u);
		}
	}
}

static uint32_t lastPlayingTimestamp = 0;

void AudioPlayer_Cyclic(void) {
	AudioPlayer_HeadphoneVolumeManager();
	if ((millis() - lastPlayingTimestamp >= 1000) && gPlayProperties.playMode != NO_PLAYLIST && gPlayProperties.playMode != BUSY && !gPlayProperties.pausePlay) {
		// audio is playing, update the playtime since start
		lastPlayingTimestamp = millis();
		playTimeSecSinceStart += 1;
	}
}

// Wrapper-function to reverse detection of connected headphones.
// Normally headphones are supposed to be plugged in if a given GPIO/channel is LOW/false.
bool Audio_Detect_Mode_HP(bool _state) {
#ifndef DETECT_HP_ON_HIGH
	return _state;
#else
	return !_state;
#endif
}

playlistSortMode AudioPlayer_GetPlaylistSortMode(void) {
	return AudioPlayer_PlaylistSortMode;
}

bool AudioPlayer_SetPlaylistSortMode(playlistSortMode value) {
	AudioPlayer_PlaylistSortMode = value;
	size_t written = gPrefsSettings.putUChar("PLSortMode", EnumUtils::underlying_value(AudioPlayer_PlaylistSortMode));
	return (written == 1);
}

bool AudioPlayer_SetPlaylistSortMode(uint8_t value) {
	return AudioPlayer_SetPlaylistSortMode(EnumUtils::to_enum<playlistSortMode>(value));
}

uint8_t AudioPlayer_GetCurrentVolume(void) {
	return AudioPlayer_CurrentVolume;
}

void AudioPlayer_SetCurrentVolume(uint8_t value) {
	AudioPlayer_CurrentVolume = value;
}

uint8_t AudioPlayer_GetMaxVolume(void) {
	return AudioPlayer_MaxVolume;
}

void AudioPlayer_SetMaxVolume(uint8_t value) {
	AudioPlayer_MaxVolume = value;
}

uint8_t AudioPlayer_GetMaxVolumeSpeaker(void) {
	return AudioPlayer_MaxVolumeSpeaker;
}

void AudioPlayer_SetMaxVolumeSpeaker(uint8_t value) {
	AudioPlayer_MaxVolumeSpeaker = value;
}

uint8_t AudioPlayer_GetMinVolume(void) {
	return AudioPlayer_MinVolume;
}

void AudioPlayer_SetMinVolume(uint8_t value) {
	AudioPlayer_MinVolume = value;
}

uint8_t AudioPlayer_GetInitVolume(void) {
	return AudioPlayer_InitVolume;
}

void AudioPlayer_SetInitVolume(uint8_t value) {
	AudioPlayer_InitVolume = value;
}

time_t AudioPlayer_GetPlayTimeAllTime(void) {
	return (playTimeSecTotal + playTimeSecSinceStart) * 1000;
}

time_t AudioPlayer_GetPlayTimeSinceStart(void) {
	return (playTimeSecSinceStart * 1000);
}

uint32_t AudioPlayer_GetCurrentTime(void) {
	return AudioPlayer_CurrentTime;
}

uint32_t AudioPlayer_GetFileDuration(void) {
	return AudioPlayer_FileDuration;
}

String AudioPlayer_GetStationLogoUrl(void) {
	return AudioPlayer_StationLogoUrl;
}

void Audio_setTitle(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(gPlayProperties.title, sizeof(gPlayProperties.title) / sizeof(gPlayProperties.title[0]), format, args);
	va_end(args);

	// notify web ui and mqtt
	Web_SendWebsocketData(0, WebsocketCodeType::TrackInfo);
#ifdef MQTT_ENABLE
	publishMqtt(topicTrackState, gPlayProperties.title, false);
#endif
}

// Set maxVolume depending on headphone-adjustment is enabled and headphone is/is not connected
// Enable/disable PA/HP-amps initially
void AudioPlayer_SetupVolumeAndAmps(void) {
	gPlayProperties.currentPlayMono = gPrefsSettings.getBool("playMono", false);
	gPlayProperties.newPlayMono = gPrefsSettings.getBool("playMono", false);

#ifndef HEADPHONE_ADJUST_ENABLE
	AudioPlayer_MaxVolume = AudioPlayer_MaxVolumeSpeaker;
	// If automatic HP-detection is not used, we enabled both (PA / HP) if defined
	#ifdef GPIO_PA_EN
	Port_Write(GPIO_PA_EN, true, true);
	#endif
	#ifdef GPIO_HP_EN
	Port_Write(GPIO_HP_EN, true, true);
	#endif
#else
	if (Audio_Detect_Mode_HP(Port_Read(HP_DETECT))) {
		AudioPlayer_MaxVolume = AudioPlayer_MaxVolumeSpeaker; // 1 if headphone is not connected
	#ifdef GPIO_PA_EN
		Port_Write(GPIO_PA_EN, true, true);
	#endif
	#ifdef GPIO_HP_EN
		Port_Write(GPIO_HP_EN, false, true);
	#endif
	} else {
		AudioPlayer_MaxVolume = AudioPlayer_MaxVolumeHeadphone; // 0 if headphone is connected (put to GND)
		gPlayProperties.newPlayMono = false; // always stereo for headphones!

	#ifdef GPIO_PA_EN
		Port_Write(GPIO_PA_EN, false, true);
	#endif
	#ifdef GPIO_HP_EN
		Port_Write(GPIO_HP_EN, true, true);
	#endif
	}
	Log_Printf(LOGLEVEL_INFO, maxVolumeSet, AudioPlayer_MaxVolume);
	return;
#endif
}

void AudioPlayer_HeadphoneVolumeManager(void) {
#ifdef HEADPHONE_ADJUST_ENABLE
	bool currentHeadPhoneDetectionState = Audio_Detect_Mode_HP(Port_Read(HP_DETECT));

	if (AudioPlayer_HeadphoneLastDetectionState != currentHeadPhoneDetectionState && (millis() - AudioPlayer_HeadphoneLastDetectionTimestamp >= headphoneLastDetectionDebounce)) {
		if (currentHeadPhoneDetectionState) {
			AudioPlayer_MaxVolume = AudioPlayer_MaxVolumeSpeaker;
			gPlayProperties.newPlayMono = gPrefsSettings.getBool("playMono", false);

	#ifdef GPIO_PA_EN
			Port_Write(GPIO_PA_EN, true, false);
	#endif
	#ifdef GPIO_HP_EN
			Port_Write(GPIO_HP_EN, false, false);
	#endif
		} else {
			AudioPlayer_MaxVolume = AudioPlayer_MaxVolumeHeadphone;
			gPlayProperties.newPlayMono = false; // Always stereo for headphones
			if (AudioPlayer_GetCurrentVolume() > AudioPlayer_MaxVolume) {
				AudioPlayer_VolumeToQueueSender(AudioPlayer_MaxVolume, true); // Lower volume for headphone if headphone's maxvolume is exceeded by volume set in speaker-mode
			}

	#ifdef GPIO_PA_EN
			Port_Write(GPIO_PA_EN, false, false);
	#endif
	#ifdef GPIO_HP_EN
			Port_Write(GPIO_HP_EN, true, false);
	#endif
		}
		AudioPlayer_HeadphoneLastDetectionState = currentHeadPhoneDetectionState;
		AudioPlayer_HeadphoneLastDetectionTimestamp = millis();
		Log_Printf(LOGLEVEL_INFO, maxVolumeSet, AudioPlayer_MaxVolume);
	}
#endif
}

class AudioCustom : public Audio {
public:
	void *operator new(size_t size) {
		return psramFound() ? ps_malloc(size) : malloc(size);
	}
};

// Function to play music as task
void AudioPlayer_Task(void *parameter) {
#ifdef BOARD_HAS_PSRAM
	AudioCustom *audio = new AudioCustom();
#else
	static Audio audioAsStatic; // Don't use heap as it's needed for other stuff :-)
	Audio *audio = &audioAsStatic;
#endif

#ifdef I2S_COMM_FMT_LSB_ENABLE
	audio->setI2SCommFMT_LSB(true);
#endif

	constexpr uint32_t playbackTimeout = 2000;
	uint32_t playbackTimeoutStart = millis();

	AudioPlayer_CurrentVolume = AudioPlayer_GetInitVolume();
	audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
	audio->setVolumeSteps(AUDIOPLAYER_VOLUME_MAX);
	audio->setVolume(AudioPlayer_CurrentVolume, gPrefsSettings.getUChar("volumeCurve", 0));
	audio->forceMono(gPlayProperties.currentPlayMono);
	int8_t currentEqualizer[3] = {gPrefsSettings.getChar("gainLowPass", 0), gPrefsSettings.getChar("gainBandPass", 0), gPrefsSettings.getChar("gainHighPass", 0)};
	audio->setTone(currentEqualizer[0], currentEqualizer[1], currentEqualizer[2]);

	uint8_t currentVolume;
	BaseType_t trackQStatus = pdFAIL;
	uint8_t trackCommand = NO_ACTION;
	bool audioReturnCode;
	AudioPlayer_CurrentTime = 0;
	AudioPlayer_FileDuration = 0;
	uint32_t AudioPlayer_LastPlaytimeStatsTimestamp = 0u;

	for (;;) {
		/*
		if (cnt123++ % 100 == 0) {
			Log_Printf(LOGLEVEL_DEBUG, "%u", uxTaskGetStackHighWaterMark(NULL));
		}
		*/
		if (xQueueReceive(gVolumeQueue, &currentVolume, 0) == pdPASS) {
			Log_Printf(LOGLEVEL_INFO, newLoudnessReceivedQueue, currentVolume);
			audio->setVolume(currentVolume, gPrefsSettings.getUChar("volumeCurve", 0));
			Web_SendWebsocketData(0, WebsocketCodeType::Volume);
#ifdef MQTT_ENABLE
			publishMqtt(topicLoudnessState, currentVolume, false);
#endif
		}

		if (xQueueReceive(gEqualizerQueue, &currentEqualizer, 0) == pdPASS) {
			Log_Printf(LOGLEVEL_DEBUG, newEqualizerReceivedQueue, currentEqualizer[0], currentEqualizer[1], currentEqualizer[2]);
			audio->setTone(currentEqualizer[0], currentEqualizer[1], currentEqualizer[2]);
		}

		if (xQueueReceive(gTrackControlQueue, &trackCommand, 0) == pdPASS) {
			Log_Printf(LOGLEVEL_INFO, newCntrlReceivedQueue, trackCommand);
		}

		// Update playtime stats every 250 ms
		if ((millis() - AudioPlayer_LastPlaytimeStatsTimestamp) > 250) {
			AudioPlayer_LastPlaytimeStatsTimestamp = millis();
			// Update current playtime and duration
			AudioPlayer_CurrentTime = audio->getAudioCurrentTime();
			AudioPlayer_FileDuration = audio->getAudioFileDuration();
			// Calculate relative position in file (for trackprogress neopixel & web-ui)
			uint32_t fileSize = audio->getFileSize();
			gPlayProperties.audioFileSize = fileSize;
			if (!gPlayProperties.playlistFinished && fileSize > 0) {
				// for local files and web files with known size
				if (!gPlayProperties.pausePlay && (gPlayProperties.seekmode != SEEK_POS_PERCENT)) { // To progress necessary when paused
					uint32_t audioDataStartPos = audio->getAudioDataStartPos();
					gPlayProperties.currentRelPos = ((double) (audio->getFilePos() - audioDataStartPos - audio->inBufferFilled()) / (fileSize - audioDataStartPos)) * 100;
				}
			} else {
				if (gPlayProperties.isWebstream && (audio->inBufferSize() > 0)) {
					// calc current fillbuffer percent for webstream with unknown size/end
					gPlayProperties.currentRelPos = (double) (audio->inBufferFilled() / (double) audio->inBufferSize()) * 100;
				} else {
					gPlayProperties.currentRelPos = 0;
				}
			}
		}

		Playlist *newPlaylist;
		trackQStatus = xQueueReceive(gTrackQueue, &newPlaylist, 0);
		if (trackQStatus == pdPASS || gPlayProperties.trackFinished || trackCommand != NO_ACTION) {
			if (trackQStatus == pdPASS) {
				audio->stopSong();

				// destroy the old playlist and assign the new
				freePlaylist(gPlayProperties.playlist);
				gPlayProperties.playlist = newPlaylist;
				Log_Printf(LOGLEVEL_NOTICE, newPlaylistReceived, gPlayProperties.playlist->size());
				Log_Printf(LOGLEVEL_DEBUG, "Free heap: %u", ESP.getFreeHeap());
				playbackTimeoutStart = millis();
				gPlayProperties.pausePlay = false;
				gPlayProperties.trackFinished = false;
				gPlayProperties.playlistFinished = false;
#ifdef MQTT_ENABLE
				publishMqtt(topicPlaymodeState, gPlayProperties.playMode, false);
				publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
#endif

				// If we're in audiobook-mode and apply a modification-card, we don't
				// want to save lastPlayPosition for the mod-card but for the card that holds the playlist
				if (strlen(gCurrentRfidTagId) > 0) {
					strncpy(gPlayProperties.playRfidTag, gCurrentRfidTagId, sizeof(gPlayProperties.playRfidTag) / sizeof(gPlayProperties.playRfidTag[0]));
				}
			}
			if (gPlayProperties.trackFinished) {
				gPlayProperties.trackFinished = false;
				if (gPlayProperties.playMode == NO_PLAYLIST || gPlayProperties.playlist == nullptr) {
					gPlayProperties.playlistFinished = true;
					continue;
				}
				if (gPlayProperties.saveLastPlayPosition) { // Don't save for AUDIOBOOK_LOOP because not necessary
					if (gPlayProperties.currentTrackNumber + 1 < gPlayProperties.playlist->size()) {
						// Only save if there's another track, otherwise it will be saved at end of playlist anyway
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber + 1, gPlayProperties.playlist->size());
					}
				}
				if (gPlayProperties.sleepAfterCurrentTrack) { // Go to sleep if "sleep after track" was requested
					gPlayProperties.playlistFinished = true;
					gPlayProperties.playMode = NO_PLAYLIST;
					System_RequestSleep();
					break;
				}
				if (!gPlayProperties.repeatCurrentTrack) { // If endless-loop requested, track-number will not be incremented
					gPlayProperties.currentTrackNumber++;
				} else {
					Log_Println(repeatTrackDueToPlaymode, LOGLEVEL_INFO);
					Led_Indicate(LedIndicatorType::Rewind);
				}
			}

			if (gPlayProperties.playlistFinished && trackCommand != NO_ACTION) {
				if (gPlayProperties.playMode != BUSY) { // Prevents from staying in mode BUSY forever when error occured (e.g. directory empty that should be played)
					Log_Println(noPlaymodeChangeIfIdle, LOGLEVEL_NOTICE);
					trackCommand = NO_ACTION;
					System_IndicateError();
					continue;
				}
			}
			/* Check if track-control was called
			   (stop, start, next track, prev. track, last track, first track...) */
			switch (trackCommand) {
				case STOP:
					audio->stopSong();
					trackCommand = NO_ACTION;
					Log_Println(cmndStop, LOGLEVEL_INFO);
					gPlayProperties.pausePlay = true;
					gPlayProperties.playlistFinished = true;
					gPlayProperties.playMode = NO_PLAYLIST;
					Audio_setTitle(noPlaylist);
					AudioPlayer_ClearCover();
					continue;

				case PAUSEPLAY:
					trackCommand = NO_ACTION;
					audio->pauseResume();
					if (gPlayProperties.pausePlay) {
						Log_Println(cmndResumeFromPause, LOGLEVEL_INFO);
					} else {
						Log_Println(cmndPause, LOGLEVEL_INFO);
					}
					if (gPlayProperties.saveLastPlayPosition && !gPlayProperties.pausePlay) {
						Log_Printf(LOGLEVEL_INFO, trackPausedAtPos, audio->getFilePos(), audio->getFilePos() - audio->inBufferFilled());
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), audio->getFilePos() - audio->inBufferFilled(), gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.playlist->size());
					}
					gPlayProperties.pausePlay = !gPlayProperties.pausePlay;
					Web_SendWebsocketData(0, WebsocketCodeType::TrackInfo);
					continue;

				case NEXTTRACK:
					trackCommand = NO_ACTION;
					if (gPlayProperties.pausePlay) {
						audio->pauseResume();
						gPlayProperties.pausePlay = false;
					}
					if (gPlayProperties.repeatCurrentTrack) { // End loop if button was pressed
						gPlayProperties.repeatCurrentTrack = false;
#ifdef MQTT_ENABLE
						publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
#endif
					}
					// Allow next track if current track played in playlist isn't the last track.
					// Exception: loop-playlist is active. In this case playback restarts at the first track of the playlist.
					if ((gPlayProperties.currentTrackNumber + 1 < gPlayProperties.playlist->size()) || gPlayProperties.repeatPlaylist) {
						if ((gPlayProperties.currentTrackNumber + 1 >= gPlayProperties.playlist->size()) && gPlayProperties.repeatPlaylist) {
							gPlayProperties.currentTrackNumber = 0;
						} else {
							gPlayProperties.currentTrackNumber++;
						}
						if (gPlayProperties.saveLastPlayPosition) {
							AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.playlist->size());
							Log_Println(trackStartAudiobook, LOGLEVEL_INFO);
						}
						Log_Println(cmndNextTrack, LOGLEVEL_INFO);
						if (!gPlayProperties.playlistFinished) {
							audio->stopSong();
						}
					} else {
						Log_Println(lastTrackAlreadyActive, LOGLEVEL_NOTICE);
						System_IndicateError();
						continue;
					}
					break;

				case PREVIOUSTRACK:
					trackCommand = NO_ACTION;
					if (gPlayProperties.pausePlay) {
						audio->pauseResume();
						gPlayProperties.pausePlay = false;
					}
					if (gPlayProperties.repeatCurrentTrack) { // End loop if button was pressed
						gPlayProperties.repeatCurrentTrack = false;
#ifdef MQTT_ENABLE
						publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
#endif
					}
					if (gPlayProperties.playMode == WEBSTREAM) {
						Log_Println(trackChangeWebstream, LOGLEVEL_INFO);
						System_IndicateError();
						continue;
					} else if (gPlayProperties.playMode == LOCAL_M3U) {
						Log_Println(cmndPrevTrack, LOGLEVEL_INFO);
						if (gPlayProperties.currentTrackNumber > 0) {
							gPlayProperties.currentTrackNumber--;
						} else {
							System_IndicateError();
							continue;
						}
					} else {
						if (gPlayProperties.currentTrackNumber > 0 || gPlayProperties.repeatPlaylist) {
							if (audio->getAudioCurrentTime() < 5) { // play previous track when current track time is small, else play current track again
								if (gPlayProperties.currentTrackNumber == 0 && gPlayProperties.repeatPlaylist) {
									gPlayProperties.currentTrackNumber = gPlayProperties.playlist->size() - 1; // Go back to last track in loop-mode when first track is played
								} else {
									gPlayProperties.currentTrackNumber--;
								}
							}

							if (gPlayProperties.saveLastPlayPosition) {
								AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.playlist->size());
								Log_Println(trackStartAudiobook, LOGLEVEL_INFO);
							}

							Log_Println(cmndPrevTrack, LOGLEVEL_INFO);
							if (!gPlayProperties.playlistFinished) {
								audio->stopSong();
							}
						} else {
							if (gPlayProperties.saveLastPlayPosition) {
								AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.playlist->size());
							}
							audio->stopSong();
							Led_Indicate(LedIndicatorType::Rewind);
							audioReturnCode = audio->connecttoFS(gFSystem, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber));
							// consider track as finished, when audio lib call was not successful
							if (!audioReturnCode) {
								System_IndicateError();
								gPlayProperties.trackFinished = true;
								continue;
							}
							Log_Println(trackStart, LOGLEVEL_INFO);
							continue;
						}
					}
					break;
				case FIRSTTRACK:
					trackCommand = NO_ACTION;
					if (gPlayProperties.pausePlay) {
						audio->pauseResume();
						gPlayProperties.pausePlay = false;
					}
					gPlayProperties.currentTrackNumber = 0;
					if (gPlayProperties.saveLastPlayPosition) {
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.playlist->size());
						Log_Println(trackStartAudiobook, LOGLEVEL_INFO);
					}
					Log_Println(cmndFirstTrack, LOGLEVEL_INFO);
					if (!gPlayProperties.playlistFinished) {
						audio->stopSong();
					}
					break;

				case LASTTRACK:
					trackCommand = NO_ACTION;
					if (gPlayProperties.pausePlay) {
						audio->pauseResume();
						gPlayProperties.pausePlay = false;
					}
					if (gPlayProperties.currentTrackNumber + 1 < gPlayProperties.playlist->size()) {
						gPlayProperties.currentTrackNumber = gPlayProperties.playlist->size() - 1;
						if (gPlayProperties.saveLastPlayPosition) {
							AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.playlist->size());
							Log_Println(trackStartAudiobook, LOGLEVEL_INFO);
						}
						Log_Println(cmndLastTrack, LOGLEVEL_INFO);
						if (!gPlayProperties.playlistFinished) {
							audio->stopSong();
						}
					} else {
						Log_Println(lastTrackAlreadyActive, LOGLEVEL_NOTICE);
						System_IndicateError();
						continue;
					}
					break;

				case 0:
					break;

				default:
					trackCommand = NO_ACTION;
					Log_Println(cmndDoesNotExist, LOGLEVEL_NOTICE);
					System_IndicateError();
					continue;
			}

			if (gPlayProperties.playUntilTrackNumber == gPlayProperties.currentTrackNumber && gPlayProperties.playUntilTrackNumber > 0) {
				if (gPlayProperties.saveLastPlayPosition) {
					AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, 0, gPlayProperties.playlist->size());
				}
				gPlayProperties.playlistFinished = true;
				gPlayProperties.playMode = NO_PLAYLIST;
				System_RequestSleep();
				continue;
			}

			if (gPlayProperties.currentTrackNumber >= gPlayProperties.playlist->size()) { // Check if last element of playlist is already reached
				Log_Println(endOfPlaylistReached, LOGLEVEL_NOTICE);
				if (!gPlayProperties.repeatPlaylist) {
					if (gPlayProperties.saveLastPlayPosition) {
						// Set back to first track
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(0), 0, gPlayProperties.playMode, 0, gPlayProperties.playlist->size());
					}
					gPlayProperties.playlistFinished = true;
					gPlayProperties.playMode = NO_PLAYLIST;
					Audio_setTitle(noPlaylist);
					AudioPlayer_ClearCover();
#ifdef MQTT_ENABLE
					publishMqtt(topicPlaymodeState, gPlayProperties.playMode, false);
#endif
					gPlayProperties.currentTrackNumber = 0;
					if (gPlayProperties.sleepAfterPlaylist) {
						System_RequestSleep();
					}
					continue;
				} else { // Check if sleep after current track/playlist was requested
					if (gPlayProperties.sleepAfterPlaylist || gPlayProperties.sleepAfterCurrentTrack) {
						gPlayProperties.playlistFinished = true;
						gPlayProperties.playMode = NO_PLAYLIST;
						System_RequestSleep();
						continue;
					} // Repeat playlist; set current track number back to 0
					Log_Println(repeatPlaylistDueToPlaymode, LOGLEVEL_NOTICE);
					gPlayProperties.currentTrackNumber = 0;
					if (gPlayProperties.saveLastPlayPosition) {
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, gPlayProperties.playlist->at(0), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.playlist->size());
					}
				}
			}

			if (!strncmp("http", gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), 4)) {
				gPlayProperties.isWebstream = true;
			} else {
				gPlayProperties.isWebstream = false;
			}
			gPlayProperties.currentRelPos = 0;
			audioReturnCode = false;

			if (gPlayProperties.playMode == WEBSTREAM || (gPlayProperties.playMode == LOCAL_M3U && gPlayProperties.isWebstream)) { // Webstream
				audioReturnCode = audio->connecttohost(gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber));
				gPlayProperties.playlistFinished = false;
				gTriedToConnectToHost = true;
			} else if (gPlayProperties.playMode != WEBSTREAM && !gPlayProperties.isWebstream) {
				// Files from SD
				if (!gFSystem.exists(gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber))) { // Check first if file/folder exists
					Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber));
					gPlayProperties.trackFinished = true;
					continue;
				} else {
					audioReturnCode = audio->connecttoFS(gFSystem, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber));
					// consider track as finished, when audio lib call was not successful
				}
			}

			if (!audioReturnCode) {
				System_IndicateError();
				gPlayProperties.trackFinished = true;
				continue;
			} else {
				if (gPlayProperties.currentTrackNumber) {
					Led_Indicate(LedIndicatorType::PlaylistProgress);
				}
				if (gPlayProperties.startAtFilePos > 0) {
					audio->setFilePos(gPlayProperties.startAtFilePos);
					Log_Printf(LOGLEVEL_NOTICE, trackStartatPos, gPlayProperties.startAtFilePos);
					gPlayProperties.startAtFilePos = 0;
				}
				const char *title = gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber);
				if (gPlayProperties.isWebstream) {
					title = "Webradio";
				}
				if (gPlayProperties.playlist->size() > 1) {
					Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber + 1, gPlayProperties.playlist->size(), title);
				} else {
					Audio_setTitle("%s", title);
				}
				AudioPlayer_ClearCover();
				Log_Printf(LOGLEVEL_NOTICE, currentlyPlaying, gPlayProperties.playlist->at(gPlayProperties.currentTrackNumber), (gPlayProperties.currentTrackNumber + 1), gPlayProperties.playlist->size());
				gPlayProperties.playlistFinished = false;
			}
		}

		// Handle seekmodes
		if (gPlayProperties.seekmode != SEEK_NORMAL) {
			if (gPlayProperties.seekmode == SEEK_FORWARDS) {
				if (audio->setTimeOffset(jumpOffset)) {
					Log_Printf(LOGLEVEL_NOTICE, secondsJumpForward, jumpOffset);
				} else {
					System_IndicateError();
				}
			} else if (gPlayProperties.seekmode == SEEK_BACKWARDS) {
				if (audio->setTimeOffset(-(jumpOffset))) {
					Log_Printf(LOGLEVEL_NOTICE, secondsJumpBackward, jumpOffset);
				} else {
					System_IndicateError();
				}
			} else if ((gPlayProperties.seekmode == SEEK_POS_PERCENT) && (gPlayProperties.currentRelPos > 0) && (gPlayProperties.currentRelPos < 100)) {
				uint32_t newFilePos = uint32_t((double) audio->getAudioDataStartPos() * (1 - gPlayProperties.currentRelPos / 100) + (gPlayProperties.currentRelPos / 100) * audio->getFileSize());
				if (audio->setFilePos(newFilePos)) {
					Log_Printf(LOGLEVEL_NOTICE, JumpToPosition, newFilePos, audio->getFileSize());
				} else {
					System_IndicateError();
				}
			}
			gPlayProperties.seekmode = SEEK_NORMAL;
		}

		// Handle IP-announcement
		if (gPlayProperties.tellMode == TTS_IP_ADDRESS) {
			gPlayProperties.tellMode = TTS_NONE;
			String ipText = Wlan_GetIpAddress();
			bool speechOk;
			// make IP as text (replace thousand separator with locale text)
			switch (LANGUAGE) {
				case DE:
					ipText.replace(".", "Punkt");
					speechOk = audio->connecttospeech(ipText.c_str(), "de");
					break;
				case FR:
					ipText.replace(".", "point");
					speechOk = audio->connecttospeech(ipText.c_str(), "fr");
					break;
				default:
					ipText.replace(".", "point");
					speechOk = audio->connecttospeech(ipText.c_str(), "en");
			}
			if (!speechOk) {
				System_IndicateError();
			}
		}

		// Handle time-announcement
		if (gPlayProperties.tellMode == TTS_CURRENT_TIME) {
			gPlayProperties.tellMode = TTS_NONE;
			struct tm timeinfo;
			getLocalTime(&timeinfo);
			static char timeStringBuff[64];
			bool speechOk;
#if (LANGUAGE == DE)
			snprintf(timeStringBuff, sizeof(timeStringBuff), "Es ist %02d:%02d Uhr", timeinfo.tm_hour, timeinfo.tm_min);
			speechOk = audio->connecttospeech(timeStringBuff, "de");
#else
			if (timeinfo.tm_hour > 12) {
				snprintf(timeStringBuff, sizeof(timeStringBuff), "It is %02d:%02d PM", timeinfo.tm_hour - 12, timeinfo.tm_min);
			} else {
				snprintf(timeStringBuff, sizeof(timeStringBuff), "It is %02d:%02d AM", timeinfo.tm_hour, timeinfo.tm_min);
			}
			speechOk = audio->connecttospeech(timeStringBuff, "en");
#endif
			if (!speechOk) {
				System_IndicateError();
			}
		}

		// If speech is over, go back to predefined state
		if (!gPlayProperties.currentSpeechActive && gPlayProperties.lastSpeechActive) {
			gPlayProperties.lastSpeechActive = false;
			if (gPlayProperties.playMode != NO_PLAYLIST) {
				xQueueSend(gRfidCardQueue, gPlayProperties.playRfidTag, 0); // Re-inject previous RFID-ID in order to continue playback
			}
		}

		// Handle if mono/stereo should be changed (e.g. if plugging headphones)
		if (gPlayProperties.newPlayMono != gPlayProperties.currentPlayMono) {
			gPlayProperties.currentPlayMono = gPlayProperties.newPlayMono;
			audio->forceMono(gPlayProperties.currentPlayMono);
			if (gPlayProperties.currentPlayMono) {
				Log_Println(newPlayModeMono, LOGLEVEL_NOTICE);
			} else {
				Log_Println(newPlayModeStereo, LOGLEVEL_NOTICE);
			}
			audio->setTone(gPlayProperties.gainLowPass, gPlayProperties.gainBandPass, gPlayProperties.gainHighPass);
		}

		audio->loop();
		if (gPlayProperties.playlistFinished || gPlayProperties.pausePlay) {
			if (!gPlayProperties.currentSpeechActive) {
				vTaskDelay(portTICK_PERIOD_MS * 10); // Waste some time if playlist is not active
			}
		} else {
			System_UpdateActivityTimer(); // Refresh if playlist is active so uC will not fall asleep due to reaching inactivity-time
		}

		if (audio->isRunning()) {
			playbackTimeoutStart = millis();
		}

		// If error occured: move to the next track in the playlist
		const bool activeMode = (gPlayProperties.playMode != NO_PLAYLIST && gPlayProperties.playMode != BUSY);
		const bool noAudio = (!audio->isRunning() && !gPlayProperties.pausePlay);
		const bool timeout = ((millis() - playbackTimeoutStart) > playbackTimeout);
		if (activeMode) {
			// we check for timeout
			if (noAudio && timeout) {
				// Audio playback timed out, move on to the next
				System_IndicateError();
				gPlayProperties.trackFinished = true;
				playbackTimeoutStart = millis();
			}
		} else {
			// we are idle, update timeout so that we do not get a spurious error when launching into a playlist
			playbackTimeoutStart = millis();
		}
		if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && audio->isRunning()) {
			// do not delay here, audio task is time critical in BT-Source mode
		} else {
			vTaskDelay(portTICK_PERIOD_MS * 1);
		}
		// esp_task_wdt_reset(); // Don't forget to feed the dog!

#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
		static uint8_t resetOnNextIdle = false;
		if (gPlayProperties.playlistFinished || gPlayProperties.playMode == NO_PLAYLIST) {
			if (resetOnNextIdle) {
				Rfid_ResetOldRfid();
				resetOnNextIdle = false;
			}
		} else {
			resetOnNextIdle = true;
		}
#endif
	}
	vTaskDelete(NULL);
}

// Returns current repeat-mode (mix of repeat current track and current playlist)
uint8_t AudioPlayer_GetRepeatMode(void) {
	if (gPlayProperties.repeatPlaylist && gPlayProperties.repeatCurrentTrack) {
		return TRACK_N_PLAYLIST;
	} else if (gPlayProperties.repeatPlaylist && !gPlayProperties.repeatCurrentTrack) {
		return PLAYLIST;
	} else if (!gPlayProperties.repeatPlaylist && gPlayProperties.repeatCurrentTrack) {
		return TRACK;
	} else {
		return NO_REPEAT;
	}
}

// Adds new volume-entry to volume-queue
// If volume is changed via webgui or MQTT, it's necessary to re-adjust current value of rotary-encoder.
void AudioPlayer_VolumeToQueueSender(const int32_t _newVolume, bool reAdjustRotary) {
	uint32_t _volume;
	int32_t _volumeBuf = AudioPlayer_GetCurrentVolume();

	Led_Indicate(LedIndicatorType::VolumeChange);
	if (_newVolume < AudioPlayer_GetMinVolume()) {
		Log_Println(minLoudnessReached, LOGLEVEL_INFO);
		return;
	} else if (_newVolume > AudioPlayer_GetMaxVolume()) {
		Log_Println(maxLoudnessReached, LOGLEVEL_INFO);
		return;
	} else {
		_volume = _newVolume;
		AudioPlayer_SetCurrentVolume(_volume);
		if (reAdjustRotary) {
			RotaryEncoder_Readjust();
		}
		xQueueSend(gVolumeQueue, &_volume, 0);
		AudioPlayer_PauseOnMinVolume(_volumeBuf, _newVolume);
	}
}

// Adds equalizer settings low, band and high pass and readjusts the equalizer
void AudioPlayer_EqualizerToQueueSender(const int8_t gainLowPass, const int8_t gainBandPass, const int8_t gainHighPass) {
	int8_t _equalizer[3] = {gainLowPass, gainBandPass, gainHighPass};
	xQueueSend(gEqualizerQueue, &_equalizer, 0);
}

// Pauses playback if playback is active and volume is changes from minVolume+1 to minVolume (usually 0)
void AudioPlayer_PauseOnMinVolume(const uint8_t oldVolume, const uint8_t newVolume) {
	if (gPlayProperties.pauseOnMinVolume) {
		if (gPlayProperties.playMode == BUSY || gPlayProperties.playMode == NO_PLAYLIST) {
			return;
		}

		if (!gPlayProperties.pausePlay) { // Volume changes from 1 to 0
			if (oldVolume == AudioPlayer_GetMinVolume() + 1 && newVolume == AudioPlayer_GetMinVolume()) {
				Cmd_Action(CMD_PLAYPAUSE);
			}
		}
		if (gPlayProperties.pausePlay) { // Volume changes from 0 to 1
			if (oldVolume == AudioPlayer_GetMinVolume() && newVolume > AudioPlayer_GetMinVolume()) {
				Cmd_Action(CMD_PLAYPAUSE);
			}
		}
	}
}

// Receives de-serialized RFID-data (from NVS) and dispatches playlists for the given
// playmode to the track-queue.
void AudioPlayer_TrackQueueDispatcher(const char *_itemToPlay, const uint32_t _lastPlayPos, const uint32_t _playMode, const uint16_t _trackLastPlayed) {
	// Make sure last playposition for audiobook is saved when new RFID-tag is applied
	if (gPlayProperties.SavePlayPosRfidChange && !gPlayProperties.pausePlay && (gPlayProperties.playMode == AUDIOBOOK || gPlayProperties.playMode == AUDIOBOOK_LOOP)) {
		AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
		while (!gPlayProperties.pausePlay) { // Make sure to wait until playback is paused in order to be sure that playposition saved in NVS
			vTaskDelay(portTICK_PERIOD_MS * 100u);
		}
	}

	gPlayProperties.startAtFilePos = _lastPlayPos;
	gPlayProperties.currentTrackNumber = _trackLastPlayed;
	std::optional<Playlist *> musicFiles;
	String folderPath = _itemToPlay;

	if (_playMode != WEBSTREAM) {
		if (_playMode == RANDOM_SUBDIRECTORY_OF_DIRECTORY || _playMode == RANDOM_SUBDIRECTORY_OF_DIRECTORY_ALL_TRACKS_OF_DIR_RANDOM) {
			folderPath = SdCard_pickRandomSubdirectory(_itemToPlay);
			if (!folderPath) {
				// If error occured while extracting random subdirectory
				musicFiles = std::nullopt;
			} else {
				musicFiles = SdCard_ReturnPlaylist(folderPath.c_str(), _playMode); // Provide random subdirectory in order to enter regular playlist-generation
			}
		} else {
			musicFiles = SdCard_ReturnPlaylist(_itemToPlay, _playMode);
		}
	} else {
		musicFiles = AudioPlayer_ReturnPlaylistFromWebstream(_itemToPlay);
	}

	// Catch if error occured (e.g. file not found)
	if (!musicFiles) {
		Log_Println(errorOccured, LOGLEVEL_ERROR);
		System_IndicateError();
		if (gPlayProperties.playMode != NO_PLAYLIST) {
			AudioPlayer_TrackControlToQueueSender(STOP);
		}
		return;
	}

	gPlayProperties.playMode = BUSY; // Show @Neopixel, if uC is busy with creating playlist
	Playlist *list = musicFiles.value();
	if (!list->size()) {
		Log_Println(noMp3FilesInDir, LOGLEVEL_NOTICE);
		System_IndicateError();
		if (!gPlayProperties.pausePlay) {
			AudioPlayer_TrackControlToQueueSender(STOP);
			while (!gPlayProperties.pausePlay) {
				vTaskDelay(portTICK_PERIOD_MS * 10u);
			}
		}

		gPlayProperties.playMode = NO_PLAYLIST;
		freePlaylist(list);
		return;
	}

	// Set some default-values
	gPlayProperties.repeatCurrentTrack = false;
	gPlayProperties.repeatPlaylist = false;
	gPlayProperties.sleepAfterCurrentTrack = false;
	gPlayProperties.sleepAfterPlaylist = false;
	gPlayProperties.saveLastPlayPosition = false;
	gPlayProperties.playUntilTrackNumber = 0;

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
	// Store last RFID-tag to NVS
	gPrefsSettings.putString("lastRfid", gCurrentRfidTagId);
#endif

	bool error = false;
	switch (_playMode) {
		case SINGLE_TRACK: {
			Log_Println(modeSingleTrack, LOGLEVEL_NOTICE);

			break;
		}

		case SINGLE_TRACK_LOOP: {
			gPlayProperties.repeatCurrentTrack = true;
			gPlayProperties.repeatPlaylist = true;
			Log_Println(modeSingleTrackLoop, LOGLEVEL_NOTICE);
			break;
		}

		case SINGLE_TRACK_OF_DIR_RANDOM: {
			gPlayProperties.sleepAfterCurrentTrack = true;
			gPlayProperties.playUntilTrackNumber = 0;
			Led_SetNightmode(true);
			Log_Println(modeSingleTrackRandom, LOGLEVEL_NOTICE);
			AudioPlayer_RandomizePlaylist(list);
			// we have a random order, so pick the first entry and scrap the rest
			auto first = list->at(0);
			list->at(0) = nullptr; // prevent our entry from being destroyed
			freePlaylist(list); // this also scrapped our vector --> recreate it
			list = new Playlist();
			list->push_back(first);
			break;
		}

		case AUDIOBOOK: { // Tracks need to be sorted!
			gPlayProperties.saveLastPlayPosition = true;
			Log_Println(modeSingleAudiobook, LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist(list);
			break;
		}

		case AUDIOBOOK_LOOP: { // Tracks need to be sorted!
			gPlayProperties.repeatPlaylist = true;
			gPlayProperties.saveLastPlayPosition = true;
			Log_Println(modeSingleAudiobookLoop, LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist(list);
			break;
		}

		case ALL_TRACKS_OF_DIR_SORTED:
		case RANDOM_SUBDIRECTORY_OF_DIRECTORY: {
			Log_Printf(LOGLEVEL_NOTICE, modeAllTrackAlphSorted, folderPath.c_str());
			AudioPlayer_SortPlaylist(list);
			break;
		}

		case ALL_TRACKS_OF_DIR_RANDOM:
		case RANDOM_SUBDIRECTORY_OF_DIRECTORY_ALL_TRACKS_OF_DIR_RANDOM: {
			Log_Printf(LOGLEVEL_NOTICE, modeAllTrackRandom, folderPath.c_str());
			AudioPlayer_RandomizePlaylist(list);
			break;
		}

		case ALL_TRACKS_OF_DIR_SORTED_LOOP: {
			gPlayProperties.repeatPlaylist = true;
			Log_Println(modeAllTrackAlphSortedLoop, LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist(list);
			break;
		}

		case ALL_TRACKS_OF_DIR_RANDOM_LOOP: {
			gPlayProperties.repeatPlaylist = true;
			Log_Println(modeAllTrackRandomLoop, LOGLEVEL_NOTICE);
			AudioPlayer_RandomizePlaylist(list);
			break;
		}

		case WEBSTREAM: { // This is always just one "track"
			Log_Println(modeWebstream, LOGLEVEL_NOTICE);
			if (!Wlan_IsConnected()) {
				Log_Println(webstreamNotAvailable, LOGLEVEL_ERROR);
				error = true;
			}
			break;
		}

		case LOCAL_M3U: { // Can be one or multiple SD-files or webradio-stations; or a mix of both
			Log_Println(modeWebstreamM3u, LOGLEVEL_NOTICE);
			break;
		}

		default:
			Log_Printf(LOGLEVEL_ERROR, modeInvalid, gPlayProperties.playMode);
			error = true;
	}

	if (!error) {
		gPlayProperties.playMode = _playMode;
		xQueueSend(gTrackQueue, &list, 0);
		return;
	}

	// we had an error, blink and destroy playlist
	gPlayProperties.playMode = NO_PLAYLIST;
	System_IndicateError();
	freePlaylist(list);
}

/* Wraps putString for writing settings into NVS for RFID-cards.
   Returns number of characters written. */
size_t AudioPlayer_NvsRfidWriteWrapper(const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks) {
	if (_playMode == NO_PLAYLIST) {
		// writing back to NVS with NO_PLAYLIST seems to be a bug - Todo: Find the cause here
		Log_Printf(LOGLEVEL_ERROR, modeInvalid, _playMode);
		return 0;
	}
	Led_SetPause(true); // Workaround to prevent exceptions due to Neopixel-signalisation while NVS-write
	char prefBuf[275];
	char trackBuf[255];
	snprintf(trackBuf, sizeof(trackBuf) / sizeof(trackBuf[0]), _track);

	// If it's a directory we just want to play/save basename(path)
	if (_numberOfTracks > 1) {
		const char s = '/';
		const char *last = strrchr(_track, s);
		const char *first = strchr(_track, s);
		unsigned long substr = last - first + 1;
		if (substr <= sizeof(trackBuf) / sizeof(trackBuf[0])) {
			snprintf(trackBuf, substr, _track); // save substring basename(_track)
		} else {
			return 0; // Filename too long!
		}
	}

	snprintf(prefBuf, sizeof(prefBuf) / sizeof(prefBuf[0]), "%s%s%s%" PRIu32 "%s%d%s%" PRIu16, stringDelimiter, trackBuf, stringDelimiter, _playPosition, stringDelimiter, _playMode, stringDelimiter, _trackLastPlayed);
	Log_Printf(LOGLEVEL_INFO, wroteLastTrackToNvs, prefBuf, _rfidCardId, _playMode, _trackLastPlayed);
	Log_Println(prefBuf, LOGLEVEL_INFO);
	Led_SetPause(false);
	return gPrefsRfid.putString(_rfidCardId, prefBuf);

	// Examples for serialized RFID-actions that are stored in NVS
	// #<file/folder>#<startPlayPositionInBytes>#<playmode>#<trackNumberToStartWith>
	// Please note: There's no need to do this manually (unless you want to)
	/*gPrefsRfid.putString("215123125075", "#/mp3/Kinderlieder#0#6#0");
	gPrefsRfid.putString("169239075184", "#http://radio.koennmer.net/evosonic.mp3#0#8#0");
	gPrefsRfid.putString("244105171042", "#0#0#111#0"); // modification-card (repeat track)
	gPrefsRfid.putString("228064156042", "#0#0#110#0"); // modification-card (repeat playlist)
	gPrefsRfid.putString("212130160042", "#/mp3/Hoerspiele/Yakari/Sammlung2#0#3#0");*/
}

// Adds webstream to playlist; same like SdCard_ReturnPlaylist() but always only one entry
std::optional<Playlist *> AudioPlayer_ReturnPlaylistFromWebstream(const char *_webUrl) {
	Playlist *playlist = new Playlist();
	const size_t len = strlen(_webUrl) + 1;
	char *entry = static_cast<char *>(x_malloc(len));
	if (!entry) {
		// OOM
		Log_Println(unableToAllocateMemForLinearPlaylist, LOGLEVEL_ERROR);
		freePlaylist(playlist);
		return std::nullopt;
	}
	strncpy(entry, _webUrl, len);
	entry[len - 1] = '\0';
	playlist->push_back(entry);

	return playlist;
}

// Adds new control-command to control-queue
void AudioPlayer_TrackControlToQueueSender(const uint8_t trackCommand) {
	xQueueSend(gTrackControlQueue, &trackCommand, 0);
}

// Knuth-Fisher-Yates-algorithm to randomize playlist
void AudioPlayer_RandomizePlaylist(Playlist *playlist) {
	if (playlist->size() < 2) {
		// we can not randomize less than 2 entries
		return;
	}

	// randomize using the "normal" random engine and shuffle
	std::default_random_engine rnd(millis());
	std::shuffle(playlist->begin(), playlist->end(), rnd);
}

// Helper to sort playlist - standard string comparison
static bool AudioPlayer_ArrSortHelper_strcmp(const char *a, const char *b) {
	return strcmp(a, b) < 0;
}

// Helper to sort playlist - natural case-sensitive
static bool AudioPlayer_ArrSortHelper_strnatcmp(const char *a, const char *b) {
	return strnatcmp(a, b) < 0;
}

// Helper to sort playlist - natural case-insensitive
static bool AudioPlayer_ArrSortHelper_strnatcasecmp(const char *a, const char *b) {
	return strnatcasecmp(a, b) < 0;
}

// Sort playlist
void AudioPlayer_SortPlaylist(Playlist *playlist) {
	std::function<bool(const char *, const char *)> cmpFunc;
	const char *mode;
	switch (AudioPlayer_PlaylistSortMode) {
		case playlistSortMode::STRCMP:
			cmpFunc = AudioPlayer_ArrSortHelper_strcmp; // standard string comparison
			mode = "standard string compare";
			break;
		case playlistSortMode::STRNATCMP:
			cmpFunc = AudioPlayer_ArrSortHelper_strnatcmp; // natural case-sensitive
			mode = "case-sensitive natural sorting";
			break;
		case playlistSortMode::STRNATCASECMP:
		default:
			cmpFunc = AudioPlayer_ArrSortHelper_strnatcasecmp; // natural case-insensitive
			mode = "case-insensitive natural sorting";
			break;
	}

	Log_Printf(LOGLEVEL_INFO, "Sorting files using %s", mode);
	std::sort(playlist->begin(), playlist->end(), cmpFunc);
}

// Clear cover send notification
void AudioPlayer_ClearCover(void) {
	gPlayProperties.coverFilePos = 0;
	AudioPlayer_StationLogoUrl = "";
	// websocket and mqtt notify cover image has changed
	Web_SendWebsocketData(0, WebsocketCodeType::CoverImg);
#ifdef MQTT_ENABLE
	publishMqtt(topicCoverChangedState, "", false);
#endif
}

// Some mp3-lib-stuff (slightly changed from default)
void audio_info(const char *info) {
	Log_Printf(LOGLEVEL_INFO, "info        : %s", info);
	if (startsWith((char *) info, "slow stream, dropouts")) {
		// websocket notify for slow stream
		Web_SendWebsocketData(0, WebsocketCodeType::Dropout);
	}
}

void audio_id3data(const char *info) { // id3 metadata
	Log_Printf(LOGLEVEL_INFO, "id3data     : %s", info);
	// get title
	if (startsWith((char *) info, "Title") || startsWith((char *) info, "TITLE=") || startsWith((char *) info, "title=")) { // ID3: "Title:", VORBISCOMMENT: "TITLE=", "title=", "Title="
		if (gPlayProperties.playlist->size() > 1) {
			Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber + 1, gPlayProperties.playlist->size(), info + 6);
		} else {
			Audio_setTitle("%s", info + 6);
		}
	}
}

void audio_eof_mp3(const char *info) { // end of file
	Log_Printf(LOGLEVEL_INFO, "eof_mp3     : %s", info);
	gPlayProperties.trackFinished = true;
}

void audio_showstation(const char *info) {
	Log_Printf(LOGLEVEL_NOTICE, "station     : %s", info);
	if (strcmp(info, "")) {
		if (gPlayProperties.playlist->size() > 1) {
			Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber + 1, gPlayProperties.playlist->size(), info);
		} else {
			Audio_setTitle("%s", info);
		}
	}
}

void audio_showstreamtitle(const char *info) {
	Log_Printf(LOGLEVEL_INFO, "streamtitle : %s", info);
	if (strcmp(info, "")) {
		if (gPlayProperties.playlist->size() > 1) {
			Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber + 1, gPlayProperties.playlist->size(), info);
		} else {
			Audio_setTitle("%s", info);
		}
	}
}

void audio_bitrate(const char *info) {
	Log_Printf(LOGLEVEL_INFO, "bitrate     : %s", info);
}

void audio_commercial(const char *info) { // duration in sec
	Log_Printf(LOGLEVEL_INFO, "commercial  : %s", info);
}

void audio_icyurl(const char *info) { // homepage
	Log_Printf(LOGLEVEL_INFO, "icyurl      : %s", info);
	if ((String(info) != "") && (AudioPlayer_StationLogoUrl == "")) {
		// has station homepage, get favicon url
		AudioPlayer_StationLogoUrl = "https://www.google.com/s2/favicons?sz=256&domain_url=" + String(info);
		// websocket and mqtt notify station logo has changed
		Web_SendWebsocketData(0, WebsocketCodeType::CoverImg);
	}
}

void audio_icylogo(const char *info) { // logo
	Log_Printf(LOGLEVEL_INFO, "icylogo      : %s", info);
	if (String(info) != "") {
		AudioPlayer_StationLogoUrl = info;
		// websocket and mqtt notify station logo has changed
		Web_SendWebsocketData(0, WebsocketCodeType::CoverImg);
	}
}

void audio_lasthost(const char *info) { // stream URL played
	Log_Printf(LOGLEVEL_INFO, "lasthost    : %s", info);
}

// id3 tag: save cover image
void audio_id3image(File &file, const size_t pos, const size_t size) {
	// save cover image position and size for later use
	gPlayProperties.coverFilePos = pos;
	gPlayProperties.coverFileSize = size;
	// websocket and mqtt notify cover image has changed
	Web_SendWebsocketData(0, WebsocketCodeType::CoverImg);
#ifdef MQTT_ENABLE
	publishMqtt(topicCoverChangedState, "", false);
#endif
}

// encoded blockpicture cover image segments (all ogg, vorbis, opus files, some flac files)
void audio_oggimage(File &file, std::vector<uint32_t> v) {
	// save decoded cover in /.cache/file.path()
	String decodedCover = "/.cache";
	decodedCover.concat(file.path());
	if (gFSystem.exists(decodedCover)) {
		Log_Printf(LOGLEVEL_DEBUG, "Cover already cached in %s", decodedCover.c_str());
	} else {
		String tmpDecodedCover = decodedCover.substring(0, decodedCover.lastIndexOf('/') + 1); // to prevent coverFile corruption write into temporary file; fixed name since no parallel usage of audioI2S
		tmpDecodedCover.concat(".tmp");
		File coverFile = gFSystem.open(tmpDecodedCover, FILE_WRITE, true); // open file with create=true to make sure parent directories are created
		if (!coverFile) {
			return;
		}

		// write fLaC marker in order to use flac Routine, since decoded cover has METADATA_BLOCK_PICTURE like flac
		constexpr uint8_t flacMarker[] = "fLaC";
		coverFile.write(flacMarker, std::char_traits<uint8_t>::length(flacMarker));

		const size_t chunkSize = 2048; // must be base64 compatible, i.e. a multiple of 4
		uint8_t *encodedChunk = (uint8_t *) x_malloc(chunkSize);
		size_t decodedLength;
		size_t currentRemainder = 0;
		size_t currentPosition = file.position(); // save current position in audio file otherwise playback will result in an error

		for (size_t i = 0; i < v.size(); i += 2) {
			// calculate the number of chunks needed to read the segment
			size_t numChunks = (v[i + 1] + currentRemainder) / chunkSize;
			size_t remainder = currentRemainder;

			// read and decode the segment chunk by chunk, write decodedChunk into encodedChunk to save memory
			file.seek(v[i]);
			for (size_t chunk = 0; chunk < numChunks; chunk++) {
				file.readBytes(reinterpret_cast<char *>(&encodedChunk[remainder]), chunkSize - remainder);
				decodedLength = b64decode(encodedChunk, encodedChunk, chunkSize);
				coverFile.write(encodedChunk, decodedLength);
				remainder = 0;
			}

			// calculate new remainder, read it, and if it is the end of file, decode it
			currentRemainder = (v[i + 1] + currentRemainder) % chunkSize;
			if (currentRemainder) {
				file.readBytes(reinterpret_cast<char *>(&encodedChunk[remainder]), currentRemainder - remainder);
				if (i == v.size() - 2) {
					decodedLength = b64decode(encodedChunk, encodedChunk, currentRemainder);
					coverFile.write(encodedChunk, decodedLength);
				}
			}
		}
		free(encodedChunk);
		coverFile.close();
		file.seek(currentPosition);
		gFSystem.rename(tmpDecodedCover, decodedCover);
		Log_Printf(LOGLEVEL_DEBUG, "Cover decoded and cached in %s", decodedCover.c_str());
	}
	gPlayProperties.coverFilePos = 1; // flacMarker gives 4 Bytes before METADATA_BLOCK_PICTURE, whereas for flac files audioI2S points 3 Bytes before METADATA_BLOCK_PICTURE, so gPlayProperties.coverFilePos has to be set to 4-3=1
	// websocket and mqtt notify cover image has changed
	Web_SendWebsocketData(0, WebsocketCodeType::CoverImg);
#ifdef MQTT_ENABLE
	publishMqtt(topicCoverChangedState, "", false);
#endif
}

void audio_eof_speech(const char *info) {
	gPlayProperties.currentSpeechActive = false;
}

void audio_process_i2s(int16_t *outBuff, uint16_t validSamples, uint8_t bitsPerSample, uint8_t channels, bool *continueI2S) {

	uint32_t sample;
	for (int i = 0; i < validSamples; i++) {
		if (channels == 2) {
			// stereo
			sample = (uint16_t(outBuff[i * 2]) << 16) | uint16_t(outBuff[i * 2 + 1]);
			*continueI2S = !Bluetooth_Source_SendAudioData(&sample);
		}
		if (channels == 1) {
			// mono
			sample = (uint16_t(outBuff[i]) << 16) | uint16_t(outBuff[i]);
			*continueI2S = !Bluetooth_Source_SendAudioData(&sample);
		}
	}
}
