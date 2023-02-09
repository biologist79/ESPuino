#include <Arduino.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include "settings.h"
#include "Audio.h"
#include "AudioPlayer.h"
#include "Common.h"
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
#include "Wlan.h"
#include "Web.h"
#include "Bluetooth.h"
#include "Cmd.h"

#define AUDIOPLAYER_VOLUME_MAX 21u
#define AUDIOPLAYER_VOLUME_MIN 0u
#define AUDIOPLAYER_VOLUME_INIT 3u

playProps gPlayProperties;
//uint32_t cnt123 = 0;

// Volume
static uint8_t AudioPlayer_CurrentVolume = AUDIOPLAYER_VOLUME_INIT;
static uint8_t AudioPlayer_MaxVolume = AUDIOPLAYER_VOLUME_MAX;
static uint8_t AudioPlayer_MaxVolumeSpeaker = AUDIOPLAYER_VOLUME_MAX;
static uint8_t AudioPlayer_MinVolume = AUDIOPLAYER_VOLUME_MIN;
static uint8_t AudioPlayer_InitVolume = AUDIOPLAYER_VOLUME_INIT;

#ifdef HEADPHONE_ADJUST_ENABLE
	static bool AudioPlayer_HeadphoneLastDetectionState;
	static uint32_t AudioPlayer_HeadphoneLastDetectionTimestamp = 0u;
	static uint8_t AudioPlayer_MaxVolumeHeadphone = 11u; // Maximum volume that can be adjusted in headphone-mode (default; can be changed later via GUI)
#endif

static void AudioPlayer_Task(void *parameter);
static void AudioPlayer_HeadphoneVolumeManager(void);
static char **AudioPlayer_ReturnPlaylistFromWebstream(const char *_webUrl);
static int AudioPlayer_ArrSortHelper(const void *a, const void *b);
static void AudioPlayer_SortPlaylist(const char **arr, int n);
static void AudioPlayer_SortPlaylist(char *str[], const uint32_t count);
static size_t AudioPlayer_NvsRfidWriteWrapper(const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks);
static void AudioPlayer_ClearCover(void);

void AudioPlayer_Init(void) {
	#ifndef USE_LAST_VOLUME_AFTER_REBOOT
		// Get initial volume from NVS
		uint32_t nvsInitialVolume = gPrefsSettings.getUInt("initVolume", 0);
	#else
		// Get volume used at last shutdown
		uint32_t nvsInitialVolume = gPrefsSettings.getUInt("previousVolume", 999);
		if (nvsInitialVolume == 999) {
			gPrefsSettings.putUInt("previousVolume", AudioPlayer_GetInitVolume());
			nvsInitialVolume = AudioPlayer_GetInitVolume();
		} else {
			Log_Println((char *) FPSTR(rememberLastVolume), LOGLEVEL_ERROR);
		}
	#endif

	if (nvsInitialVolume) {
		AudioPlayer_SetInitVolume(nvsInitialVolume);
		snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(restoredInitialLoudnessFromNvs), nvsInitialVolume);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	} else {
		gPrefsSettings.putUInt("initVolume", AudioPlayer_GetInitVolume());
		Log_Println((char *) FPSTR(wroteInitialLoudnessToNvs), LOGLEVEL_ERROR);
	}

	// Get maximum volume for speaker from NVS
	uint32_t nvsMaxVolumeSpeaker = gPrefsSettings.getUInt("maxVolumeSp", 0);
	if (nvsMaxVolumeSpeaker) {
		AudioPlayer_SetMaxVolumeSpeaker(nvsMaxVolumeSpeaker);
		AudioPlayer_SetMaxVolume(nvsMaxVolumeSpeaker);
		snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(restoredMaxLoudnessForSpeakerFromNvs), nvsMaxVolumeSpeaker);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	} else {
		gPrefsSettings.putUInt("maxVolumeSp", nvsMaxVolumeSpeaker);
		Log_Println((char *) FPSTR(wroteMaxLoudnessForSpeakerToNvs), LOGLEVEL_ERROR);
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
			snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(restoredMaxLoudnessForHeadphoneFromNvs), nvsAudioPlayer_MaxVolumeHeadphone);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			gPrefsSettings.putUInt("maxVolumeHp", nvsAudioPlayer_MaxVolumeHeadphone);
			Log_Println((char *) FPSTR(wroteMaxLoudnessForHeadphoneToNvs), LOGLEVEL_ERROR);
		}
	#endif
	// Adjust volume depending on headphone is connected and volume-adjustment is enabled
	AudioPlayer_SetupVolumeAndAmps();

	// clear title and cover image
	gPlayProperties.title = NULL;
	gPlayProperties.coverFilePos = 0;

	// Don't start audio-task in BT-speaker mode!
	if ((System_GetOperationMode() == OPMODE_NORMAL) || (System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE)) {
		xTaskCreatePinnedToCore(
			AudioPlayer_Task,      /* Function to implement the task */
			"mp3play",             /* Name of the task */
			5500,                  /* Stack size in words */
			NULL,                  /* Task input parameter */
			2 | portPRIVILEGE_BIT, /* Priority of the task */
			NULL,                  /* Task handle. */
			1                      /* Core where the task should run */
		);
	}
}

void AudioPlayer_Cyclic(void) {
	AudioPlayer_HeadphoneVolumeManager();
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

void Audio_setTitle(const char *format, ...)
{
	// Allocates space for title of current track only once and keeps char* in order to avoid heap-fragmentation.
	static char* _title = NULL;
	if (_title == NULL) {
		_title = (char *) x_malloc(sizeof(char) * 255);
		gPlayProperties.title = _title;
	}

	char buf[256];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf)/sizeof(buf[0]), format, args);
	va_end(args);
	convertAsciiToUtf8(buf, gPlayProperties.title);

	// notify web ui and mqtt
	Web_SendWebsocketData(0, 30);
	#ifdef MQTT_ENABLE
		publishMqtt((char *) FPSTR(topicTrackState), gPlayProperties.title, false);
	#endif
}

// Set maxVolume depending on headphone-adjustment is enabled and headphone is/is not connected
// Enable/disable PA/HP-amps initially
void AudioPlayer_SetupVolumeAndAmps(void) {
	#ifdef PLAY_MONO_SPEAKER
		gPlayProperties.currentPlayMono = true;
		gPlayProperties.newPlayMono = true;
	#else
		gPlayProperties.currentPlayMono = false;
		gPlayProperties.newPlayMono = false;
	#endif

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
			gPlayProperties.newPlayMono = false;                     // always stereo for headphones!

			#ifdef GPIO_PA_EN
				Port_Write(GPIO_PA_EN, false, true);
			#endif
			#ifdef GPIO_HP_EN
				Port_Write(GPIO_HP_EN, true, true);
			#endif
		}
		snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(maxVolumeSet), AudioPlayer_MaxVolume);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
		return;
	#endif
}

void AudioPlayer_HeadphoneVolumeManager(void) {
	#ifdef HEADPHONE_ADJUST_ENABLE
		bool currentHeadPhoneDetectionState = Audio_Detect_Mode_HP(Port_Read(HP_DETECT));

		if (AudioPlayer_HeadphoneLastDetectionState != currentHeadPhoneDetectionState && (millis() - AudioPlayer_HeadphoneLastDetectionTimestamp >= headphoneLastDetectionDebounce)) {
			if (currentHeadPhoneDetectionState) {
				AudioPlayer_MaxVolume = AudioPlayer_MaxVolumeSpeaker;
				#ifdef PLAY_MONO_SPEAKER
					gPlayProperties.newPlayMono = true;
				#else
					gPlayProperties.newPlayMono = false;
				#endif

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
			snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(maxVolumeSet), AudioPlayer_MaxVolume);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		}
	#endif
}

class AudioCustom: public Audio {
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
		static Audio audioAsStatic;         // Don't use heap as it's needed for other stuff :-)
		Audio *audio = &audioAsStatic;
	#endif

	#ifdef I2S_COMM_FMT_LSB_ENABLE
		audio->setI2SCommFMT_LSB(true);
	#endif

	uint8_t settleCount = 0;
	audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
	audio->setVolume(AudioPlayer_GetInitVolume());
	audio->forceMono(gPlayProperties.currentPlayMono);
	if (gPlayProperties.currentPlayMono) {
		audio->setTone(3, 0, 0);
	}

	uint8_t currentVolume;
	static BaseType_t trackQStatus;
	static uint8_t trackCommand = NO_ACTION;
	bool audioReturnCode;

	for (;;) {
		/*
		if (cnt123++ % 100 == 0) {
			snprintf(Log_Buffer, Log_BufferLength, "%u", uxTaskGetStackHighWaterMark(NULL));
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
		}
		*/
		if (xQueueReceive(gVolumeQueue, &currentVolume, 0) == pdPASS) {
			snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(newLoudnessReceivedQueue), currentVolume);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
			audio->setVolume(currentVolume);
			Web_SendWebsocketData(0, 50);
			#ifdef MQTT_ENABLE
				publishMqtt((char *) FPSTR(topicLoudnessState), currentVolume, false);
			#endif
		}

		if (xQueueReceive(gTrackControlQueue, &trackCommand, 0) == pdPASS) {
			snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(newCntrlReceivedQueue), trackCommand);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		}

		trackQStatus = xQueueReceive(gTrackQueue, &gPlayProperties.playlist, 0);
		if (trackQStatus == pdPASS || gPlayProperties.trackFinished || trackCommand != NO_ACTION) {
			if (trackQStatus == pdPASS) {
				if (gPlayProperties.pausePlay) {
					gPlayProperties.pausePlay = false;
				}
				audio->stopSong();
				#if (LANGUAGE == DE)
					snprintf(Log_Buffer, Log_BufferLength, "%s mit %d Titel(n)", (char *) FPSTR(newPlaylistReceived), gPlayProperties.numberOfTracks);
				#else
					snprintf(Log_Buffer, Log_BufferLength, "%s with %d track(s)", (char *) FPSTR(newPlaylistReceived), gPlayProperties.numberOfTracks);
				#endif
				Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

				snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) F("Free heap: "), ESP.getFreeHeap());
				Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicPlaymodeState), gPlayProperties.playMode, false);
					publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
				#endif

				// If we're in audiobook-mode and apply a modification-card, we don't
				// want to save lastPlayPosition for the mod-card but for the card that holds the playlist
				if (gCurrentRfidTagId != NULL) {
					strncpy(gPlayProperties.playRfidTag, gCurrentRfidTagId, sizeof(gPlayProperties.playRfidTag) / sizeof(gPlayProperties.playRfidTag[0]));
				}
			}
			if (gPlayProperties.trackFinished) {
				gPlayProperties.trackFinished = false;
				if (gPlayProperties.playMode == NO_PLAYLIST) {
					gPlayProperties.playlistFinished = true;
					continue;
				}
				if (gPlayProperties.saveLastPlayPosition) { // Don't save for AUDIOBOOK_LOOP because not necessary
					if (gPlayProperties.currentTrackNumber + 1 < gPlayProperties.numberOfTracks)
					{
						// Only save if there's another track, otherwise it will be saved at end of playlist anyway
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber + 1, gPlayProperties.numberOfTracks);
					}
				}
				if (gPlayProperties.sleepAfterCurrentTrack) { // Go to sleep if "sleep after track" was requested
					System_RequestSleep();
					break;
				}
				if (!gPlayProperties.repeatCurrentTrack) { // If endless-loop requested, track-number will not be incremented
					gPlayProperties.currentTrackNumber++;
				} else {
					Log_Println((char *) FPSTR(repeatTrackDueToPlaymode), LOGLEVEL_INFO);
					Led_Indicate(LedIndicatorType::Rewind);
				}
			}

			if (gPlayProperties.playlistFinished && trackCommand != NO_ACTION) {
				if (gPlayProperties.playMode != BUSY ) {    // Prevents from staying in mode BUSY forever when error occured (e.g. directory empty that should be played)
					Log_Println((char *) FPSTR(noPlaymodeChangeIfIdle), LOGLEVEL_NOTICE);
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
					Log_Println((char *) FPSTR(cmndStop), LOGLEVEL_INFO);
					gPlayProperties.pausePlay = true;
					gPlayProperties.playlistFinished = true;
					gPlayProperties.playMode = NO_PLAYLIST;
					Audio_setTitle((char *)FPSTR (noPlaylist));
					AudioPlayer_ClearCover();
					continue;

				case PAUSEPLAY:
					trackCommand = NO_ACTION;
					audio->pauseResume();
					Log_Println((char *) FPSTR(cmndPause), LOGLEVEL_INFO);
					if (gPlayProperties.saveLastPlayPosition && !gPlayProperties.pausePlay) {
						snprintf(Log_Buffer, Log_BufferLength, "%s: %u (%u)", (char *) FPSTR(trackPausedAtPos), audio->getFilePos(), audio->getFilePos() - audio->inBufferFilled());
						Log_Println(Log_Buffer, LOGLEVEL_INFO);
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), audio->getFilePos() - audio->inBufferFilled(), gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.numberOfTracks);
					}
					gPlayProperties.pausePlay = !gPlayProperties.pausePlay;
					Web_SendWebsocketData(0, 30);
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
							publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
						#endif
					}
					// Allow next track if current track played in playlist isn't the last track.
					// Exception: loop-playlist is active. In this case playback restarts at the first track of the playlist.
					if ((gPlayProperties.currentTrackNumber + 1 < gPlayProperties.numberOfTracks) || gPlayProperties.repeatPlaylist) {
						if ((gPlayProperties.currentTrackNumber + 1 >= gPlayProperties.numberOfTracks) && gPlayProperties.repeatPlaylist) {
							gPlayProperties.currentTrackNumber = 0;
						} else {
							gPlayProperties.currentTrackNumber++;
						}
						if (gPlayProperties.saveLastPlayPosition) {
							AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.numberOfTracks);
							Log_Println((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
						}
						Log_Println((char *) FPSTR(cmndNextTrack), LOGLEVEL_INFO);
						if (!gPlayProperties.playlistFinished) {
							audio->stopSong();
						}
					} else {
						Log_Println((char *) FPSTR(lastTrackAlreadyActive), LOGLEVEL_NOTICE);
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
							publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
						#endif
					}
					if (gPlayProperties.currentTrackNumber > 0 || gPlayProperties.repeatPlaylist) {
						if (audio->getAudioCurrentTime() < 5) { // play previous track when current track time is small, else play current track again
							if (gPlayProperties.currentTrackNumber == 0 && gPlayProperties.repeatPlaylist) {
								gPlayProperties.currentTrackNumber = gPlayProperties.numberOfTracks - 1;    // Go back to last track in loop-mode when first track is played
							} else {
								gPlayProperties.currentTrackNumber--;
							}
						}

						if (gPlayProperties.saveLastPlayPosition) {
							AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.numberOfTracks);
							Log_Println((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
						}

						Log_Println((char *) FPSTR(cmndPrevTrack), LOGLEVEL_INFO);
						if (!gPlayProperties.playlistFinished) {
							audio->stopSong();
						}
					} else {
						if (gPlayProperties.playMode == WEBSTREAM) {
							Log_Println((char *) FPSTR(trackChangeWebstream), LOGLEVEL_INFO);
							System_IndicateError();
							continue;
						}
						if (gPlayProperties.saveLastPlayPosition) {
							AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.numberOfTracks);
						}
						audio->stopSong();
						Led_Indicate(LedIndicatorType::Rewind);
						audioReturnCode = audio->connecttoFS(gFSystem, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber));
						// consider track as finished, when audio lib call was not successful
						if (!audioReturnCode) {
							System_IndicateError();
							gPlayProperties.trackFinished = true;
							continue;
						}
						Log_Println((char *) FPSTR(trackStart), LOGLEVEL_INFO);
						continue;
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
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.numberOfTracks);
						Log_Println((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
					}
					Log_Println((char *) FPSTR(cmndFirstTrack), LOGLEVEL_INFO);
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
					if (gPlayProperties.currentTrackNumber + 1 < gPlayProperties.numberOfTracks) {
						gPlayProperties.currentTrackNumber = gPlayProperties.numberOfTracks - 1;
						if (gPlayProperties.saveLastPlayPosition) {
							AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.numberOfTracks);
							Log_Println((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
						}
						Log_Println((char *) FPSTR(cmndLastTrack), LOGLEVEL_INFO);
						if (!gPlayProperties.playlistFinished) {
							audio->stopSong();
						}
					} else {
						Log_Println((char *) FPSTR(lastTrackAlreadyActive), LOGLEVEL_NOTICE);
						System_IndicateError();
						continue;
					}
					break;

				case 0:
					break;

				default:
					trackCommand = NO_ACTION;
					Log_Println((char *) FPSTR(cmndDoesNotExist), LOGLEVEL_NOTICE);
					System_IndicateError();
					continue;
			}

			if (gPlayProperties.playUntilTrackNumber == gPlayProperties.currentTrackNumber && gPlayProperties.playUntilTrackNumber > 0) {
				if (gPlayProperties.saveLastPlayPosition) {
					AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 0, gPlayProperties.playMode, 0, gPlayProperties.numberOfTracks);
				}
				gPlayProperties.playlistFinished = true;
				gPlayProperties.playMode = NO_PLAYLIST;
				System_RequestSleep();
				continue;
			}

			if (gPlayProperties.currentTrackNumber >= gPlayProperties.numberOfTracks) { // Check if last element of playlist is already reached
				Log_Println((char *) FPSTR(endOfPlaylistReached), LOGLEVEL_NOTICE);
				if (!gPlayProperties.repeatPlaylist) {
					if (gPlayProperties.saveLastPlayPosition) {
						// Set back to first track
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + 0), 0, gPlayProperties.playMode, 0, gPlayProperties.numberOfTracks);
					}
					gPlayProperties.playlistFinished = true;
					gPlayProperties.playMode = NO_PLAYLIST;
					Audio_setTitle((char *)FPSTR (noPlaylist));
					AudioPlayer_ClearCover();
					#ifdef MQTT_ENABLE
						publishMqtt((char *) FPSTR(topicPlaymodeState), gPlayProperties.playMode, false);
					#endif
					gPlayProperties.currentTrackNumber = 0;
					gPlayProperties.numberOfTracks = 0;
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
					Log_Println((char *) FPSTR(repeatPlaylistDueToPlaymode), LOGLEVEL_NOTICE);
					gPlayProperties.currentTrackNumber = 0;
					if (gPlayProperties.saveLastPlayPosition) {
						AudioPlayer_NvsRfidWriteWrapper(gPlayProperties.playRfidTag, *(gPlayProperties.playlist + 0), 0, gPlayProperties.playMode, gPlayProperties.currentTrackNumber, gPlayProperties.numberOfTracks);
					}
				}
			}

			if (!strncmp("http", *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), 4)) {
				gPlayProperties.isWebstream = true;
			} else {
				gPlayProperties.isWebstream = false;
			}
			gPlayProperties.currentRelPos = 0;
			audioReturnCode = false;

			if (gPlayProperties.playMode == WEBSTREAM || (gPlayProperties.playMode == LOCAL_M3U && gPlayProperties.isWebstream)) { // Webstream
				audioReturnCode = audio->connecttohost(*(gPlayProperties.playlist + gPlayProperties.currentTrackNumber));
				gPlayProperties.playlistFinished = false;
			} else if (gPlayProperties.playMode != WEBSTREAM && !gPlayProperties.isWebstream) {
				// Files from SD
				if (!gFSystem.exists(*(gPlayProperties.playlist + gPlayProperties.currentTrackNumber))) { // Check first if file/folder exists
					snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(dirOrFileDoesNotExist), *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber));
					Log_Println(Log_Buffer, LOGLEVEL_ERROR);
					gPlayProperties.trackFinished = true;
					continue;
				} else {
					audioReturnCode = audio->connecttoFS(gFSystem, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber));
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
					gPlayProperties.startAtFilePos = 0;
					snprintf(Log_Buffer, Log_BufferLength, "%s %u", (char *) FPSTR(trackStartatPos), audio->getFilePos());
					Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
				}
				if (gPlayProperties.isWebstream) {
					if (gPlayProperties.numberOfTracks > 1) {
						Audio_setTitle("(%u/%u): Webradio", gPlayProperties.currentTrackNumber+1,  gPlayProperties.numberOfTracks);
					} else {
						Audio_setTitle("Webradio");
					}
				} else {
					if (gPlayProperties.numberOfTracks > 1) {
						Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber+1,  gPlayProperties.numberOfTracks, *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber));
					} else {
						Audio_setTitle("%s", *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber));
					}
				}
				AudioPlayer_ClearCover();
				#if (LANGUAGE == DE)
					snprintf(Log_Buffer, Log_BufferLength, "'%s' wird abgespielt (%d von %d)", *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), (gPlayProperties.currentTrackNumber + 1), gPlayProperties.numberOfTracks);
				#else
					snprintf(Log_Buffer, Log_BufferLength, "'%s' is being played (%d of %d)", *(gPlayProperties.playlist + gPlayProperties.currentTrackNumber), (gPlayProperties.currentTrackNumber + 1), gPlayProperties.numberOfTracks);
				#endif
				Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
				gPlayProperties.playlistFinished = false;
			}
		}

		// Handle seekmodes
		if (gPlayProperties.seekmode != SEEK_NORMAL) {
			if (gPlayProperties.seekmode == SEEK_FORWARDS) {
				if (audio->setTimeOffset(jumpOffset)) {
					#if (LANGUAGE == DE)
						snprintf(Log_Buffer, Log_BufferLength, "%d Sekunden nach vorne gesprungen", jumpOffset);
					#else
						snprintf(Log_Buffer, Log_BufferLength, "Jumped %d seconds forwards", jumpOffset);
					#endif
					Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
				} else {
					System_IndicateError();
				}
			} else if (gPlayProperties.seekmode == SEEK_BACKWARDS) {
				if (audio->setTimeOffset(-(jumpOffset))) {
					#if (LANGUAGE == DE)
						snprintf(Log_Buffer, Log_BufferLength, "%d Sekunden zurueck gesprungen", jumpOffset);
					#else
						snprintf(Log_Buffer, Log_BufferLength, "Jumped %d seconds backwards", jumpOffset);
					#endif
					Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
				} else {
					System_IndicateError();
				}
			}
			gPlayProperties.seekmode = SEEK_NORMAL;
		}

		// Handle IP-announcement
		if (gPlayProperties.tellIpAddress) {
			gPlayProperties.tellIpAddress = false;
			char ipBuf[16];
			Wlan_GetIpAddress().toCharArray(ipBuf, sizeof(ipBuf));
			bool speechOk;
			#if (LANGUAGE == DE)
				speechOk = audio->connecttospeech(ipBuf, "de");
			#else
				speechOk = audio->connecttospeech(ipBuf, "en");
			#endif
			if (!speechOk) {
				System_IndicateError();
			}
		}

		// If speech is over, go back to predefined state
		if (!gPlayProperties.currentSpeechActive && gPlayProperties.lastSpeechActive) {
			gPlayProperties.lastSpeechActive = false;
			if (gPlayProperties.playMode != NO_PLAYLIST) {
				xQueueSend(gRfidCardQueue, gPlayProperties.playRfidTag, 0);     // Re-inject previous RFID-ID in order to continue playback
			}
		}

		// Handle if mono/stereo should be changed (e.g. if plugging headphones)
		if (gPlayProperties.newPlayMono != gPlayProperties.currentPlayMono) {
			gPlayProperties.currentPlayMono = gPlayProperties.newPlayMono;
			audio->forceMono(gPlayProperties.currentPlayMono);
			if (gPlayProperties.currentPlayMono) {
				Log_Println(newPlayModeMono, LOGLEVEL_NOTICE);
				audio->setTone(3, 0, 0);
			} else {
				Log_Println(newPlayModeStereo, LOGLEVEL_NOTICE);
				audio->setTone(0, 0, 0);
			}
		}

		// Calculate relative position in file (for neopixel) for SD-card-mode
		if (!gPlayProperties.playlistFinished && !gPlayProperties.isWebstream) {
			if (millis() % 20 == 0) {   // Keep it simple
				if (!gPlayProperties.pausePlay && (audio->getFileSize() > 0)) {   // To progress necessary when paused
					gPlayProperties.currentRelPos = ((double)(audio->getFilePos() - audio->inBufferFilled()) / (double)audio->getFileSize()) * 100;
				}
			}
		} else {
			gPlayProperties.currentRelPos = 0;
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
			settleCount = 0;
		}

		// If error occured: remove playlist from ESPuino
		if (gPlayProperties.playMode != NO_PLAYLIST && gPlayProperties.playMode != BUSY && !audio->isRunning() && !gPlayProperties.pausePlay) {
			if (settleCount++ == 50) { // Hack to give audio some time to settle down after playlist was generated
				gPlayProperties.playlistFinished = true;
				gPlayProperties.playMode = NO_PLAYLIST;
				settleCount = 0;
			}
		}
		if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && audio->isRunning()) {
			// do not delay here, audio task is time critical in BT-Source mode
		} else {
			vTaskDelay(portTICK_PERIOD_MS * 1);
		}
		//esp_task_wdt_reset(); // Don't forget to feed the dog!

		#ifdef DONT_ACCEPT_SAME_RFID_TWICE_ENABLE
			if (gPlayProperties.playlistFinished || gPlayProperties.playMode == NO_PLAYLIST) {
				strncpy(gOldRfidTagId, "X", cardIdStringSize-1);     // Set old rfid-id to crap in order to allow to re-apply an rfid-tag after playback is finished
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
	} else{
		return NO_REPEAT;
	}
}

// Adds new volume-entry to volume-queue
// If volume is changed via webgui or MQTT, it's necessary to re-adjust current value of rotary-encoder.
void AudioPlayer_VolumeToQueueSender(const int32_t _newVolume, bool reAdjustRotary) {
	uint32_t _volume;
	int32_t _volumeBuf = AudioPlayer_GetCurrentVolume();

	if (_newVolume < AudioPlayer_GetMinVolume()) {
		Log_Println((char *) FPSTR(minLoudnessReached), LOGLEVEL_INFO);
		return;
	} else if (_newVolume > AudioPlayer_GetMaxVolume()) {
		Log_Println((char *) FPSTR(maxLoudnessReached), LOGLEVEL_INFO);
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


// Pauses playback if playback is active and volume is changes from minVolume+1 to minVolume (usually 0)
void AudioPlayer_PauseOnMinVolume(const uint8_t oldVolume, const uint8_t newVolume) {
	#ifdef PAUSE_ON_MIN_VOLUME
		if (gPlayProperties.playMode == BUSY || gPlayProperties.playMode == NO_PLAYLIST) {
			return;
		}

		if (!gPlayProperties.pausePlay ) {	// Volume changes from 1 to 0
			if (oldVolume == AudioPlayer_GetMinVolume()+1 && newVolume == AudioPlayer_GetMinVolume()) {
				Cmd_Action(CMD_PLAYPAUSE);
			}
		}
		if (gPlayProperties.pausePlay) {	// Volume changes from 0 to 1
			if (oldVolume == AudioPlayer_GetMinVolume() && newVolume > AudioPlayer_GetMinVolume()) {
				Cmd_Action(CMD_PLAYPAUSE);
			}
		}
	#endif
}


// Receives de-serialized RFID-data (from NVS) and dispatches playlists for the given
// playmode to the track-queue.
void AudioPlayer_TrackQueueDispatcher(const char *_itemToPlay, const uint32_t _lastPlayPos, const uint32_t _playMode, const uint16_t _trackLastPlayed) {
	// Make sure last playposition for audiobook is saved when new RFID-tag is applied
	#ifdef SAVE_PLAYPOS_WHEN_RFID_CHANGE
		if (!gPlayProperties.pausePlay && (gPlayProperties.playMode == AUDIOBOOK || gPlayProperties.playMode == AUDIOBOOK_LOOP)) {
			AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
			while (!gPlayProperties.pausePlay) {    // Make sure to wait until playback is paused in order to be sure that playposition saved in NVS
				vTaskDelay(portTICK_RATE_MS * 100u);
			}
		}
	#endif
	char *filename;
	filename = (char *) x_malloc(sizeof(char) * 255);

	strncpy(filename, _itemToPlay, 255);
	gPlayProperties.startAtFilePos = _lastPlayPos;
	gPlayProperties.currentTrackNumber = _trackLastPlayed;
	char **musicFiles;

	if (_playMode != WEBSTREAM) {
		if (_playMode == RANDOM_SUBDIRECTORY_OF_DIRECTORY) {
			filename = SdCard_pickRandomSubdirectory(filename);     // *filename (input): target-directory  //   *filename (output): random subdirectory
			if (filename == NULL) {  // If error occured while extracting random subdirectory
				musicFiles = NULL;
			} else {
				musicFiles = SdCard_ReturnPlaylist(filename, _playMode);    // Provide random subdirectory in order to enter regular playlist-generation
			}
		} else {
			musicFiles = SdCard_ReturnPlaylist(filename, _playMode);
		}
	} else {
		musicFiles = AudioPlayer_ReturnPlaylistFromWebstream(filename);
	}

	// Catch if error occured (e.g. file not found)
	if (musicFiles == NULL) {
		Log_Println((char *) FPSTR(errorOccured), LOGLEVEL_ERROR);
		System_IndicateError();
		if (gPlayProperties.playMode != NO_PLAYLIST) {
			AudioPlayer_TrackControlToQueueSender(STOP);
		}
		return;
	}

	gPlayProperties.playMode = BUSY; // Show @Neopixel, if uC is busy with creating playlist
	if (!strcmp(*(musicFiles - 1), "0")) {
		Log_Println((char *) FPSTR(noMp3FilesInDir), LOGLEVEL_NOTICE);
		System_IndicateError();
		if (!gPlayProperties.pausePlay) {
			AudioPlayer_TrackControlToQueueSender(STOP);
			while (!gPlayProperties.pausePlay) {
				vTaskDelay(portTICK_RATE_MS * 10u);
			}
		}

		gPlayProperties.playMode = NO_PLAYLIST;
		free(filename);
		return;
	}

	gPlayProperties.playMode = _playMode;
	gPlayProperties.numberOfTracks = strtoul(*(musicFiles - 1), NULL, 10);
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

	switch (gPlayProperties.playMode) {
		case SINGLE_TRACK: {
			Log_Println((char *) FPSTR(modeSingleTrack), LOGLEVEL_NOTICE);
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case SINGLE_TRACK_LOOP: {
			gPlayProperties.repeatCurrentTrack = true;
			gPlayProperties.repeatPlaylist = true;
			Log_Println((char *) FPSTR(modeSingleTrackLoop), LOGLEVEL_NOTICE);
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case SINGLE_TRACK_OF_DIR_RANDOM: {
			gPlayProperties.sleepAfterCurrentTrack = true;
			gPlayProperties.playUntilTrackNumber = 0;
			gPlayProperties.numberOfTracks = 1; // Limit number to 1 even there are more entries in the playlist
			Led_ResetToNightBrightness();
			Log_Println((char *) FPSTR(modeSingleTrackRandom), LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist(musicFiles, strtoul(*(musicFiles - 1), NULL, 10));
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case AUDIOBOOK: { // Tracks need to be alph. sorted!
			gPlayProperties.saveLastPlayPosition = true;
			Log_Println((char *) FPSTR(modeSingleAudiobook), LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist((const char **)musicFiles, strtoul(*(musicFiles - 1), NULL, 10));
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case AUDIOBOOK_LOOP: { // Tracks need to be alph. sorted!
			gPlayProperties.repeatPlaylist = true;
			gPlayProperties.saveLastPlayPosition = true;
			Log_Println((char *) FPSTR(modeSingleAudiobookLoop), LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist((const char **)musicFiles, strtoul(*(musicFiles - 1), NULL, 10));
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case ALL_TRACKS_OF_DIR_SORTED:
		case RANDOM_SUBDIRECTORY_OF_DIRECTORY: {
			snprintf(Log_Buffer, Log_BufferLength, "%s '%s' ", (char *) FPSTR(modeAllTrackAlphSorted), filename);
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist((const char **)musicFiles, strtoul(*(musicFiles - 1), NULL, 10));
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case ALL_TRACKS_OF_DIR_RANDOM: {
			Log_Println((char *) FPSTR(modeAllTrackRandom), LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist(musicFiles, strtoul(*(musicFiles - 1), NULL, 10));
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case ALL_TRACKS_OF_DIR_SORTED_LOOP: {
			gPlayProperties.repeatPlaylist = true;
			Log_Println((char *) FPSTR(modeAllTrackAlphSortedLoop), LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist((const char **)musicFiles, strtoul(*(musicFiles - 1), NULL, 10));
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case ALL_TRACKS_OF_DIR_RANDOM_LOOP: {
			gPlayProperties.repeatPlaylist = true;
			Log_Println((char *) FPSTR(modeAllTrackRandomLoop), LOGLEVEL_NOTICE);
			AudioPlayer_SortPlaylist(musicFiles, strtoul(*(musicFiles - 1), NULL, 10));
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		case WEBSTREAM: { // This is always just one "track"
			Log_Println((char *) FPSTR(modeWebstream), LOGLEVEL_NOTICE);
			if (Wlan_IsConnected()) {
				xQueueSend(gTrackQueue, &(musicFiles), 0);
			} else {
				Log_Println((char *) FPSTR(webstreamNotAvailable), LOGLEVEL_ERROR);
				System_IndicateError();
				gPlayProperties.playMode = NO_PLAYLIST;
			}
			break;
		}

		case LOCAL_M3U: { // Can be one or multiple SD-files or webradio-stations; or a mix of both
			Log_Println((char *) FPSTR(modeWebstreamM3u), LOGLEVEL_NOTICE);
			xQueueSend(gTrackQueue, &(musicFiles), 0);
			break;
		}

		default:
			Log_Println((char *) FPSTR(modeDoesNotExist), LOGLEVEL_ERROR);
			gPlayProperties.playMode = NO_PLAYLIST;
			System_IndicateError();
	}
	free(filename);
}

/* Wraps putString for writing settings into NVS for RFID-cards.
   Returns number of characters written. */
size_t AudioPlayer_NvsRfidWriteWrapper(const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks) {
	Led_SetPause(true); // Workaround to prevent exceptions due to Neopixel-signalisation while NVS-write
	char prefBuf[275];
	char trackBuf[255];
	snprintf(trackBuf, sizeof(trackBuf) / sizeof(trackBuf[0]), _track);

	// If it's a directory we just want to play/save basename(path)
	if (_numberOfTracks > 1) {
		const char s = '/';
		char *last = strrchr(_track, s);
		char *first = strchr(_track, s);
		unsigned long substr = last - first + 1;
		if (substr <= sizeof(trackBuf) / sizeof(trackBuf[0])) {
			snprintf(trackBuf, substr, _track); // save substring basename(_track)
		} else {
			return 0; // Filename too long!
		}
	}

	snprintf(prefBuf, sizeof(prefBuf) / sizeof(prefBuf[0]), "%s%s%s%u%s%d%s%u", stringDelimiter, trackBuf, stringDelimiter, _playPosition, stringDelimiter, _playMode, stringDelimiter, _trackLastPlayed);
	#if (LANGUAGE == DE)
		snprintf(Log_Buffer, Log_BufferLength, "Schreibe '%s' in NVS für RFID-Card-ID %s mit Abspielmodus %d und letzter Track %u\n", prefBuf, _rfidCardId, _playMode, _trackLastPlayed);
	#else
		snprintf(Log_Buffer, Log_BufferLength, "Write '%s' to NVS for RFID-Card-ID %s with playmode %d and last track %u\n", prefBuf, _rfidCardId, _playMode, _trackLastPlayed);
	#endif
	Log_Print(Log_Buffer, LOGLEVEL_INFO, true);
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
char **AudioPlayer_ReturnPlaylistFromWebstream(const char *_webUrl) {
	char *webUrl = x_strdup(_webUrl);
	static char **url;

	if (url != NULL) {
		--url;
		freeMultiCharArray(url, strtoul(*url, NULL, 10));
	}

	url = (char **)x_malloc(sizeof(char *) * 2);

	url[0] = x_strdup("1"); // Number of files is always 1 in url-mode
	url[1] = x_strdup(webUrl);

	free(webUrl);
	return ++url;
}

// Adds new control-command to control-queue
void AudioPlayer_TrackControlToQueueSender(const uint8_t trackCommand) {
	xQueueSend(gTrackControlQueue, &trackCommand, 0);
}

// Knuth-Fisher-Yates-algorithm to randomize playlist
void AudioPlayer_SortPlaylist(char *str[], const uint32_t count) {
	if (!count) {
		return;
	}

	uint32_t i, r;
	char *swap = NULL;
	uint32_t max = count - 1;

	for (i = 0; i < count; i++) {
		r = (max > 0) ? rand() % max : 0;
		swap = *(str + max);
		*(str + max) = *(str + r);
		*(str + r) = swap;
		max--;
	}
}

// Helper to sort playlist alphabetically
static int AudioPlayer_ArrSortHelper(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}

// Sort playlist alphabetically
void AudioPlayer_SortPlaylist(const char **arr, int n) {
	qsort(arr, n, sizeof(const char *), AudioPlayer_ArrSortHelper);
}

// Clear cover send notification
void AudioPlayer_ClearCover(void) {
	gPlayProperties.coverFilePos = 0;
	// websocket and mqtt notify cover image has changed
	Web_SendWebsocketData(0, 40);
	#ifdef MQTT_ENABLE
		publishMqtt((char *) FPSTR(topicCoverChangedState), "", false);
	#endif
}

// Some mp3-lib-stuff (slightly changed from default)
void audio_info(const char *info) {
	snprintf(Log_Buffer, Log_BufferLength, "info        : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
}

void audio_id3data(const char *info) { //id3 metadata
	snprintf(Log_Buffer, Log_BufferLength, "id3data     : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
	// get title
	if (startsWith((char *)info, "Title:")) {
		if (gPlayProperties.numberOfTracks > 1) {
			Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber+1,  gPlayProperties.numberOfTracks, info + 6);
		} else {
			Audio_setTitle("%s", info + 6);
		}
	}
}

void audio_eof_mp3(const char *info) { //end of file
	snprintf(Log_Buffer, Log_BufferLength, "eof_mp3     : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
	gPlayProperties.trackFinished = true;
}

void audio_showstation(const char *info) {
	snprintf(Log_Buffer, Log_BufferLength, "station     : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
	if (strcmp(info, "")) {
		if (gPlayProperties.numberOfTracks > 1) {
			Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber+1,  gPlayProperties.numberOfTracks, info);
		} else {
			Audio_setTitle("%s", info);
		}
	}
}

void audio_showstreamtitle(const char *info) {
	snprintf(Log_Buffer, Log_BufferLength, "streamtitle : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
	if (strcmp(info, "")) {
		if (gPlayProperties.numberOfTracks > 1) {
			Audio_setTitle("(%u/%u): %s", gPlayProperties.currentTrackNumber+1,  gPlayProperties.numberOfTracks, info);
		} else {
			Audio_setTitle("%s", info);
		}
	}
}

void audio_bitrate(const char *info) {
	snprintf(Log_Buffer, Log_BufferLength, "bitrate     : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
}

void audio_commercial(const char *info) { //duration in sec
	snprintf(Log_Buffer, Log_BufferLength, "commercial  : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
}

void audio_icyurl(const char *info) { //homepage
	snprintf(Log_Buffer, Log_BufferLength, "icyurl      : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
}

void audio_lasthost(const char *info) { //stream URL played
	snprintf(Log_Buffer, Log_BufferLength, "lasthost    : %s", info);
	Log_Println(Log_Buffer, LOGLEVEL_INFO);
}

// id3 tag: save cover image
void audio_id3image(File& file, const size_t pos, const size_t size) {
	// save cover image position and size for later use
	gPlayProperties.coverFilePos = pos;
	gPlayProperties.coverFileSize = size;
	// websocket and mqtt notify cover image has changed
	Web_SendWebsocketData(0, 40);
	#ifdef MQTT_ENABLE
		publishMqtt((char *) FPSTR(topicCoverChangedState), "", false);
	#endif
}

void audio_eof_speech(const char *info){
	gPlayProperties.currentSpeechActive = false;
}


// process audio sample extern (for bluetooth source)
void audio_process_i2s(uint32_t* sample, bool *continueI2S){
	*continueI2S = !Bluetooth_Source_SendAudioData(sample);
}
