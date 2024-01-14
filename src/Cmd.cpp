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

static void Cmd_HandleSleepAction(bool enable, const char *enLogMsg, const char *enMqttMsg) {
	Led_SetNightmode(enable);
	if (enable) {
		Log_Println(enLogMsg, LOGLEVEL_INFO);
#ifdef MQTT_ENABLE
		publishMqtt(topicSleepTimerState, enMqttMsg, false);
#endif
	} else {
		System_DisableSleepTimer();
		Log_Println(modificatorSleepd, LOGLEVEL_INFO);
#ifdef MQTT_ENABLE
		publishMqtt(topicSleepTimerState, "0", false);
#endif
	}
}

void Cmd_Action(const uint16_t mod) {
	switch (mod) {
		case CMD_LOCK_BUTTONS_MOD: { // Locks/unlocks all buttons
			System_ToggleLockControls();
			if (System_AreControlsLocked()) {
				Log_Println(modificatorAllButtonsLocked, LOGLEVEL_NOTICE);
#ifdef MQTT_ENABLE
				publishMqtt(topicLockControlsState, "ON", false);
#endif
			} else {
				Log_Println(modificatorAllButtonsUnlocked, LOGLEVEL_NOTICE);
#ifdef MQTT_ENABLE
				publishMqtt(topicLockControlsState, "OFF", false);
#endif
			}
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_15: { // Enables/disables sleep after 15 minutes
			System_SetSleepTimer(15u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false; // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_30: { // Enables/disables sleep after 30 minutes
			System_SetSleepTimer(30u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false; // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_60: { // Enables/disables sleep after 60 minutes
			System_SetSleepTimer(60u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false; // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_TIMER_MOD_120: { // Enables/disables sleep after 2 hrs
			System_SetSleepTimer(120u);

			gPlayProperties.sleepAfterCurrentTrack = false; // deactivate/overwrite if already active
			gPlayProperties.sleepAfterPlaylist = false; // deactivate/overwrite if already active
			gPlayProperties.playUntilTrackNumber = 0;
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_AFTER_END_OF_TRACK: { // Puts uC to sleep after end of current track
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_NOTICE);
				System_IndicateError();
				return;
			}

			gPlayProperties.sleepAfterPlaylist = false;
			gPlayProperties.playUntilTrackNumber = 0;
			gPlayProperties.sleepAfterCurrentTrack = !gPlayProperties.sleepAfterCurrentTrack;

			Cmd_HandleSleepAction(gPlayProperties.sleepAfterCurrentTrack, modificatorSleepAtEOT, "EOT");
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_AFTER_END_OF_PLAYLIST: { // Puts uC to sleep after end of whole playlist (can take a while :->)
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_NOTICE);
				System_IndicateError();
				return;
			}
			gPlayProperties.sleepAfterCurrentTrack = false;
			gPlayProperties.playUntilTrackNumber = 0;
			gPlayProperties.sleepAfterPlaylist = !gPlayProperties.sleepAfterPlaylist;

			Cmd_HandleSleepAction(gPlayProperties.sleepAfterCurrentTrack, modificatorSleepAtEOP, "EOP");
			System_IndicateOk();
			break;
		}

		case CMD_SLEEP_AFTER_5_TRACKS: {
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_NOTICE);
				System_IndicateError();
				return;
			}

			gPlayProperties.sleepAfterCurrentTrack = false;
			gPlayProperties.sleepAfterPlaylist = false;
			gPlayProperties.sleepAfter5Tracks = !gPlayProperties.sleepAfter5Tracks;

			if (gPlayProperties.sleepAfter5Tracks) {
				if (gPlayProperties.currentTrackNumber + 5 > gPlayProperties.numberOfTracks) {
					// execute a sleep after end of playlist
					Cmd_Action(CMD_SLEEP_AFTER_END_OF_PLAYLIST);
					break;
				}
			}
			Cmd_HandleSleepAction(gPlayProperties.sleepAfter5Tracks, sleepTimerEO5, "EO5T");
			System_IndicateOk();
			break;
		}

		case CMD_REPEAT_PLAYLIST: {
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_NOTICE);
				System_IndicateError();
			} else {
				if (gPlayProperties.repeatPlaylist) {
					Log_Println(modificatorPlaylistLoopDeactive, LOGLEVEL_NOTICE);
				} else {
					Log_Println(modificatorPlaylistLoopActive, LOGLEVEL_NOTICE);
				}
				gPlayProperties.repeatPlaylist = !gPlayProperties.repeatPlaylist;
#ifdef MQTT_ENABLE
				publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
#endif
				System_IndicateOk();
			}
			break;
		}

		case CMD_REPEAT_TRACK: { // Introduces looping for track-mode
			if (gPlayProperties.playMode == NO_PLAYLIST) {
				Log_Println(modificatorNotallowedWhenIdle, LOGLEVEL_NOTICE);
				System_IndicateError();
			} else {
				if (gPlayProperties.repeatCurrentTrack) {
					Log_Println(modificatorTrackDeactive, LOGLEVEL_NOTICE);
				} else {
					Log_Println(modificatorTrackActive, LOGLEVEL_NOTICE);
				}
				gPlayProperties.repeatCurrentTrack = !gPlayProperties.repeatCurrentTrack;
#ifdef MQTT_ENABLE
				publishMqtt(topicRepeatModeState, AudioPlayer_GetRepeatMode(), false);
#endif
				System_IndicateOk();
			}
			break;
		}

		case CMD_DIMM_LEDS_NIGHTMODE: {
			Led_ToggleNightmode();
			System_IndicateOk();
			break;
		}

		case CMD_TOGGLE_WIFI_STATUS: {
			System_SetOperationMode(OPMODE_NORMAL); // escape from BT-mode, WiFi cannot coexist with BT and can cause a crash
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
		case CMD_TOGGLE_MODE: {
			if (System_GetOperationModeFromNvs() == OPMODE_NORMAL) {
				System_IndicateOk();
				System_SetOperationMode(OPMODE_BLUETOOTH_SINK);
			} else if (System_GetOperationModeFromNvs() == OPMODE_BLUETOOTH_SINK) {
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
			if (millis() <= 30000) { // Only allow to enable FTP within the first 30s after start (to prevent children it mess it up)
				Ftp_EnableServer();
			} else {
				Log_Println(ftpEnableTooLate, LOGLEVEL_ERROR);
				System_IndicateError();
			}
			break;
		}
#endif

		case CMD_TELL_IP_ADDRESS: {
			if (Wlan_IsConnected()) {
				gPlayProperties.tellMode = TTS_IP_ADDRESS;
				gPlayProperties.currentSpeechActive = true;
				gPlayProperties.lastSpeechActive = true;
				System_IndicateOk();
			} else {
				Log_Println(unableToTellIpAddress, LOGLEVEL_ERROR);
				System_IndicateError();
			}
			break;
		}

		case CMD_TELL_CURRENT_TIME: {
			if (Wlan_IsConnected()) {
				gPlayProperties.tellMode = TTS_CURRENT_TIME;
				gPlayProperties.currentSpeechActive = true;
				gPlayProperties.lastSpeechActive = true;
				System_IndicateOk();
			} else {
				Log_Println(unableToTellTime, LOGLEVEL_ERROR);
				System_IndicateError();
			}
			break;
		}

		case CMD_PLAYPAUSE: {
			if ((OPMODE_NORMAL == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
				AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
			} else {
				Bluetooth_PlayPauseTrack();
			}
			break;
		}

		case CMD_PREVTRACK: {
			if ((OPMODE_NORMAL == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
				AudioPlayer_TrackControlToQueueSender(PREVIOUSTRACK);
			} else {
				Bluetooth_PreviousTrack();
			}
			break;
		}

		case CMD_NEXTTRACK: {
			if ((OPMODE_NORMAL == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
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
			if ((OPMODE_NORMAL == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
				AudioPlayer_VolumeToQueueSender(AudioPlayer_GetCurrentVolume() + 1, true);
			} else {
				Bluetooth_SetVolume(AudioPlayer_GetCurrentVolume() + 1, true);
			}
			break;
		}

		case CMD_VOLUMEDOWN: {
			if ((OPMODE_NORMAL == System_GetOperationMode()) || (OPMODE_BLUETOOTH_SOURCE == System_GetOperationMode())) {
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

		case CMD_RESTARTSYSTEM: {
			System_Restart();
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

		case PRINT_TASK_STATS: {
			System_esp_print_tasks();
			break;
		}

		default: {
			Log_Printf(LOGLEVEL_ERROR, modificatorDoesNotExist, mod);
			System_IndicateError();
		}
	}
}
