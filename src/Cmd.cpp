#include <Arduino.h>
#include "settings.h"
#include "Cmd.h"
#include "AudioPlayer.h"
#include "Battery.h"
#include "Bluetooth.h"
#include "Ftp.h"
#include "Led.h"
#include "Log.h"
#include "Mqtt.h"
#include "System.h"
#include "Wlan.h"

void Cmd_Action(const uint16_t mod) {
	switch (mod) {
		case CMD_LOCK_BUTTONS_MOD: { // Locks/unlocks all buttons
			System_ToggleLockControls();
			if (System_AreControlsLocked()) {
				Log_Println((char *) FPSTR(modificatorAllButtonsLocked), LOGLEVEL_NOTICE);
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicLockControlsState), "ON", false);
				#endif
			} else {
				Log_Println((char *) FPSTR(modificatorAllButtonsUnlocked), LOGLEVEL_NOTICE);
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicLockControlsState), "OFF", false);
				#endif
			}
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_15: { // Enables/disables sleep after 15 minutes
			System_SetSleepTimer(15u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false;     // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_30: { // Enables/disables sleep after 30 minutes
			System_SetSleepTimer(30u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false;     // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_60: { // Enables/disables sleep after 60 minutes
			System_SetSleepTimer(60u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false;     // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_120: { // Enables/disables sleep after 2 hrs
			System_SetSleepTimer(120u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false;     // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_AFTER_END_OF_TRACK: { // Puts uC to sleep after end of current track
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
				System_IndicateError();
				return;
			}
			if (gPlayProperties.sleepAfterCurrentTrack) {
				Log_Println((char *) FPSTR(modificatorSleepAtEOTd), LOGLEVEL_NOTICE);
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
				#endif
				#ifdef NEOPIXEL_ENABLE
					Led_ResetToInitialBrightness();
				#endif
			} else {
				Log_Println((char *) FPSTR(modificatorSleepAtEOT), LOGLEVEL_NOTICE);
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicSleepTimerState), "EOT", false);
				#endif
				#ifdef NEOPIXEL_ENABLE
					Led_ResetToNightBrightness();
					Log_Println((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
				#endif
			}
			gPlayProperties.sleepAfterCurrentTrack = !gPlayProperties.sleepAfterCurrentTrack;
			gPlayProperties.sleepAfterPlaylist = false;
			System_DisableSleepTimer();
			gPlayProperties.playUntilTrackNumber = 0;

			#ifdef MQTT_ENABLE
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
			#endif
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_AFTER_END_OF_PLAYLIST: { // Puts uC to sleep after end of whole playlist (can take a while :->)
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
				System_IndicateError();
				return;
			}
			if (gPlayProperties.sleepAfterCurrentTrack) {
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
				#endif
				#ifdef NEOPIXEL_ENABLE
					Led_ResetToInitialBrightness();
				#endif
				Log_Println((char *) FPSTR(modificatorSleepAtEOPd), LOGLEVEL_NOTICE);
			} else {
				#ifdef NEOPIXEL_ENABLE
					Led_ResetToNightBrightness();
					Log_Println((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
				#endif
					Log_Println((char *) FPSTR(modificatorSleepAtEOP), LOGLEVEL_NOTICE);
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicSleepTimerState), "EOP", false);
				#endif
			}

			gPlayProperties.sleepAfterCurrentTrack = false;
			gPlayProperties.sleepAfterPlaylist = !gPlayProperties.sleepAfterPlaylist;
			System_DisableSleepTimer();
			gPlayProperties.playUntilTrackNumber = 0;
			#ifdef MQTT_ENABLE
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
			#endif
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_AFTER_5_TRACKS: {
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
				System_IndicateError();
				return;
			}

			gPlayProperties.sleepAfterCurrentTrack = false;
			gPlayProperties.sleepAfterPlaylist = false;
			System_DisableSleepTimer();

			if (gPlayProperties.playUntilTrackNumber > 0) {
				gPlayProperties.playUntilTrackNumber = 0;
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
				#endif
				#ifdef NEOPIXEL_ENABLE
					Led_ResetToInitialBrightness();
				#endif
				Log_Println((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
			} else {
				if (gPlayProperties.currentTrackNumber + 5 > gPlayProperties.numberOfTracks) { // If currentTrack + 5 exceeds number of tracks in playlist, sleep after end of playlist
					gPlayProperties.sleepAfterPlaylist = true;
					#ifdef MQTT_ENABLE
						publishMqtt((char *) FPSTR(topicSleepTimerState), "EOP", false);
					#endif
				} else  {
					gPlayProperties.playUntilTrackNumber = gPlayProperties.currentTrackNumber + 5;
					#ifdef MQTT_ENABLE
						publishMqtt((char *) FPSTR(topicSleepTimerState), "EO5T", false);
					#endif
				}
				#ifdef NEOPIXEL_ENABLE
					Led_ResetToNightBrightness();
				#endif
				Log_Println((char *) FPSTR(sleepTimerEO5), LOGLEVEL_NOTICE);
			}

			#ifdef MQTT_ENABLE
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
			#endif
			System_IndicateOk();
			break;
		}

		case CMD_REPEAT_PLAYLIST: {
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
				System_IndicateError();
			} else {
				if (gPlayProperties.repeatPlaylist) {
					Log_Println((char *) FPSTR(modificatorPlaylistLoopDeactive), LOGLEVEL_NOTICE);
				} else {
					Log_Println((char *) FPSTR(modificatorPlaylistLoopActive), LOGLEVEL_NOTICE);
				}
				gPlayProperties.repeatPlaylist = !gPlayProperties.repeatPlaylist;
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
				#endif
				System_IndicateOk();
			}
			break;
		}

		case CMD_REPEAT_TRACK: { // Introduces looping for track-mode
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
				System_IndicateError();
			} else {
				if (gPlayProperties.repeatCurrentTrack) {
					Log_Println((char *) FPSTR(modificatorTrackDeactive), LOGLEVEL_NOTICE);
				} else {
					Log_Println((char *) FPSTR(modificatorTrackActive), LOGLEVEL_NOTICE);
				}
				gPlayProperties.repeatCurrentTrack = !gPlayProperties.repeatCurrentTrack;
				#ifdef MQTT_ENABLE
					publishMqtt((char *) FPSTR(topicRepeatModeState), AudioPlayer_GetRepeatMode(), false);
				#endif
				System_IndicateOk();
			}
			break;
		}

		case CMD_DIMM_LEDS_NIGHTMODE: {
			#ifdef MQTT_ENABLE
				publishMqtt((char *) FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
			#endif
			Log_Println((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
			#ifdef NEOPIXEL_ENABLE
				Led_ResetToNightBrightness();
			#endif
			System_IndicateOk();
			break;
		}

		case CMD_TOGGLE_WIFI_STATUS: {
			Wlan_ToggleEnable();
			System_IndicateOk();
			break;
		}

		#ifdef BLUETOOTH_ENABLE
			case CMD_TOGGLE_BLUETOOTH_SINK_MODE: {
				if (System_GetOperationModeFromNvs() == OPMODE_NORMAL) {
					System_IndicateOk();
					System_SetOperationMode(OPMODE_BLUETOOTH_SINK);
				} else if (System_GetOperationModeFromNvs() == OPMODE_BLUETOOTH_SINK) {
					System_IndicateOk();
					System_SetOperationMode(OPMODE_NORMAL);
				} else {
					System_IndicateError();
				}
				break;
			}
			case CMD_TOGGLE_BLUETOOTH_SOURCE_MODE: {
				if (System_GetOperationModeFromNvs() == OPMODE_NORMAL) {
					System_IndicateOk();
					System_SetOperationMode(OPMODE_BLUETOOTH_SOURCE);
				} else if (System_GetOperationModeFromNvs() == OPMODE_BLUETOOTH_SOURCE) {
					System_IndicateOk();
					System_SetOperationMode(OPMODE_NORMAL);
				} else {
					System_IndicateError();
				}
				break;
			}
		#endif

		#ifdef FTP_ENABLE
			case CMD_ENABLE_FTP_SERVER: {
				if (millis() <= 30000) {    // Only allow to enable FTP within the first 30s after start (to prevent children it mess it up)
					Ftp_EnableServer();
				} else {
					Log_Println((char *) FPSTR(ftpEnableTooLate), LOGLEVEL_ERROR);
					System_IndicateError();
				}
				break;
			}
		#endif

		case CMD_TELL_IP_ADDRESS: {
			if (Wlan_IsConnected()) {
				if (!gPlayProperties.pausePlay) {
					AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
				}
				gPlayProperties.tellIpAddress = true;
				gPlayProperties.currentSpeechActive = true;
				gPlayProperties.lastSpeechActive = true;
				System_IndicateOk();
			} else {
				Log_Println(unableToTellIpAddress, LOGLEVEL_ERROR);
				System_IndicateError();
			}
			break;
		}

		case CMD_PLAYPAUSE: {
			if (OPMODE_NORMAL == System_GetOperationMode()) {
				AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
			} else {
				Bluetooth_PlayPauseTrack();
			}
			break;
		}

		case CMD_PREVTRACK: {
			if (OPMODE_NORMAL == System_GetOperationMode()) {
				AudioPlayer_TrackControlToQueueSender(PREVIOUSTRACK);
			} else {
				Bluetooth_PreviousTrack();
			}
			break;
		}

		case CMD_NEXTTRACK: {
			if (OPMODE_NORMAL == System_GetOperationMode()) {
				AudioPlayer_TrackControlToQueueSender(NEXTTRACK);
			} else {
				Bluetooth_NextTrack();
			}
			break;
		}

		case CMD_FIRSTTRACK: {
			AudioPlayer_TrackControlToQueueSender(FIRSTTRACK);
			break;
		}

		case CMD_LASTTRACK: {
			AudioPlayer_TrackControlToQueueSender(LASTTRACK);
			break;
		}

		case CMD_VOLUMEINIT: {
			AudioPlayer_VolumeToQueueSender(AudioPlayer_GetInitVolume(), true);
			break;
		}

		case CMD_VOLUMEUP: {
			if (OPMODE_NORMAL == System_GetOperationMode()) {
				AudioPlayer_VolumeToQueueSender(AudioPlayer_GetCurrentVolume() + 1, true);
			} else {
				Bluetooth_SetVolume(AudioPlayer_GetCurrentVolume() + 1, true);
			}
			break;
		}

		case CMD_VOLUMEDOWN:{
			if (OPMODE_NORMAL == System_GetOperationMode()) {
				AudioPlayer_VolumeToQueueSender(AudioPlayer_GetCurrentVolume() - 1, true);
			} else {
				Bluetooth_SetVolume(AudioPlayer_GetCurrentVolume() - 1, true);
			}
		   break;
		}

		case CMD_MEASUREBATTERY: {
			#ifdef BATTERY_MEASURE_ENABLE
				Battery_LogStatus();
				Battery_PublishMQTT();
				Led_Indicate(LedIndicatorType::Voltage);
			#endif
			break;
		}

		case CMD_SLEEPMODE: {
			System_RequestSleep();
			break;
		}

		case CMD_SEEK_FORWARDS: {
			gPlayProperties.seekmode = SEEK_FORWARDS;
			break;
		}

		case CMD_SEEK_BACKWARDS: {
			gPlayProperties.seekmode = SEEK_BACKWARDS;
			break;
		}

		case CMD_STOP: {
			AudioPlayer_TrackControlToQueueSender(STOP);
			break;
		}

		#ifdef ENABLE_ESPUINO_DEBUG
			case PRINT_TASK_STATS: {
				System_esp_print_tasks();
				break;
			}
		#endif

		default: {
			snprintf(Log_Buffer, Log_BufferLength, "%s %d !", (char *) FPSTR(modificatorDoesNotExist), mod);
			Log_Println(Log_Buffer, LOGLEVEL_ERROR);
			System_IndicateError();
		}
	}
}
