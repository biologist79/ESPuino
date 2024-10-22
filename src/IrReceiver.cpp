#include <Arduino.h>
#include "settings.h"

#include "IrReceiver.h"

#include "AudioPlayer.h"
#include "Cmd.h"
#include "Log.h"
#include "Queues.h"
#include "System.h"

#ifdef IR_CONTROL_ENABLE
	#include <IRremote.hpp>
#endif

// HW-Timer
#ifdef IR_CONTROL_ENABLE
uint32_t IrReceiver_LastRcCmdTimestamp = 0u;
#endif

void IrReceiver_Init() {
#ifdef IR_CONTROL_ENABLE
	IrReceiver.begin(IRLED_PIN);
#endif
}

void IrReceiver_Cyclic() {
#ifdef IR_CONTROL_ENABLE
	static uint8_t lastVolume = 0;

	if (IrReceiver.decode()) {

		// Print a short summary of received data
		IrReceiver.printIRResultShort(&Serial);
		Serial.println();
		IrReceiver.resume(); // Enable receiving of the next value
		bool rcActionOk = false;
		if (millis() - IrReceiver_LastRcCmdTimestamp >= IR_DEBOUNCE) {
			rcActionOk = true; // not used for volume up/down
			IrReceiver_LastRcCmdTimestamp = millis();
		}

		switch (IrReceiver.decodedIRData.command) {
			case RC_PLAY: {
				if (rcActionOk) {
					Cmd_Action(CMD_PLAYPAUSE);
					Log_Println("RC: Play", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_PAUSE: {
				if (rcActionOk) {
					Cmd_Action(CMD_PLAYPAUSE);
					Log_Println("RC: Pause", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_NEXT: {
				if (rcActionOk) {
					Cmd_Action(CMD_NEXTTRACK);
					Log_Println("RC: Next", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_PREVIOUS: {
				if (rcActionOk) {
					Cmd_Action(CMD_PREVTRACK);
					Log_Println("RC: Previous", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_FIRST: {
				if (rcActionOk) {
					Cmd_Action(CMD_FIRSTTRACK);
					Log_Println("RC: First", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_LAST: {
				if (rcActionOk) {
					Cmd_Action(CMD_LASTTRACK);
					Log_Println("RC: Last", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_MUTE: {
				if (rcActionOk) {
					if (AudioPlayer_GetCurrentVolume() > 0) {
						lastVolume = AudioPlayer_GetCurrentVolume();
						AudioPlayer_SetCurrentVolume(0u);
					} else {
						AudioPlayer_SetCurrentVolume(lastVolume); // Remember last volume if mute is pressed again
					}

					uint8_t currentVolume = AudioPlayer_GetCurrentVolume();
					xQueueSend(gVolumeQueue, &currentVolume, 0);
					Log_Println("RC: Mute", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_BLUETOOTH: {
				if (rcActionOk) {
					Cmd_Action(CMD_TOGGLE_BLUETOOTH_SINK_MODE);
					Log_Println("RC: Bluetooth sink", LOGLEVEL_NOTICE);
				}
				break;
			}
			// +++ todo: bluetooth source mode +++
			case RC_FTP: {
				if (rcActionOk) {
					Cmd_Action(CMD_ENABLE_FTP_SERVER);
					Log_Println("RC: FTP", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_SHUTDOWN: {
				if (rcActionOk) {
					System_RequestSleep();
					Log_Println("RC: Shutdown", LOGLEVEL_NOTICE);
				}
				break;
			}
			case RC_VOL_DOWN: {
				Cmd_Action(CMD_VOLUMEDOWN);
				Log_Println("RC: Volume down", LOGLEVEL_NOTICE);
				break;
			}
			case RC_VOL_UP: {
				Cmd_Action(CMD_VOLUMEUP);
				Log_Println("RC: Volume up", LOGLEVEL_NOTICE);
				break;
			}
			default: {
				if (rcActionOk) {
					Log_Println("RC: unknown", LOGLEVEL_NOTICE);
				}
			}
		}
	}
#endif
}
