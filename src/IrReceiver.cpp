#include <Arduino.h>
#include "../config.h"
#include "values.h"
#include "IrReceiver.h"
#include "AudioPlayer.h"
#include "Cmd.h"
#include "Queues.h"
#include "System.h"

#ifdef CONFIG_IR_CONTROL
	#include <IRremote.h>
#endif

// HW-Timer
#ifdef CONFIG_IR_CONTROL
	uint32_t IrReceiver_LastRcCmdTimestamp = 0u;
#endif

void IrReceiver_Init() {
	#ifdef CONFIG_IR_CONTROL
		IrReceiver.begin(CONFIG_GPIO_IR_LED);
	#endif
}

void IrReceiver_Cyclic() {
	#ifdef CONFIG_IR_CONTROL
		static uint8_t lastVolume = 0;

		if (IrReceiver.decode()) {

			// Print a short summary of received data
			IrReceiver.printIRResultShort(&Serial);
			Serial.println();
			IrReceiver.resume(); // Enable receiving of the next value
			bool rcActionOk = false;
			if (millis() - IrReceiver_LastRcCmdTimestamp >= CONFIG_IR_DEBOUNCE_INTERVAL) {
				rcActionOk = true; // not used for volume up/down
				IrReceiver_LastRcCmdTimestamp = millis();
			}

			switch (IrReceiver.decodedIRData.command) {
				case CONFIG_IR_RC_PLAY: {
					if (rcActionOk) {
						Cmd_Action(CMD_PLAYPAUSE);
						Log_Println((char *) F("RC: Play"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_PAUSE: {
					if (rcActionOk) {
						Cmd_Action(CMD_PLAYPAUSE);
						Log_Println((char *) F("RC: Pause"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_NEXT: {
					if (rcActionOk) {
						Cmd_Action(CMD_NEXTTRACK);
						Log_Println((char *) F("RC: Next"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_PREVIOUS: {
					if (rcActionOk) {
						Cmd_Action(CMD_PREVTRACK);
						Log_Println((char *) F("RC: Previous"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_FIRST: {
					if (rcActionOk) {
						Cmd_Action(CMD_FIRSTTRACK);
						Log_Println((char *) F("RC: First"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_LAST: {
					if (rcActionOk) {
						Cmd_Action(CMD_LASTTRACK);
						Log_Println((char *) F("RC: Last"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_MUTE: {
					if (rcActionOk) {
						if (AudioPlayer_GetCurrentVolume() > 0) {
							lastVolume = AudioPlayer_GetCurrentVolume();
							AudioPlayer_SetCurrentVolume(0u);
						} else {
							AudioPlayer_SetCurrentVolume(lastVolume); // Remember last volume if mute is pressed again
						}

						uint8_t currentVolume = AudioPlayer_GetCurrentVolume();
						xQueueSend(gVolumeQueue, &currentVolume, 0);
						Log_Println((char *) F("RC: Mute"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_BLUETOOTH: {
					if (rcActionOk) {
						Cmd_Action(CMD_TOGGLE_BLUETOOTH_SINK_MODE);
						Log_Println((char *) F("RC: Bluetooth sink"), LOGLEVEL_NOTICE);
					}
					break;
				}
				// +++ todo: bluetooth source mode +++
				case CONFIG_IR_RC_FTP: {
					if (rcActionOk) {
						Cmd_Action(CMD_ENABLE_FTP_SERVER);
						Log_Println((char *) F("RC: FTP"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_SHUTDOWN: {
					if (rcActionOk) {
						System_RequestSleep();
						Log_Println((char *) F("RC: Shutdown"), LOGLEVEL_NOTICE);
					}
					break;
				}
				case CONFIG_IR_RC_VOL_DOWN: {
					Cmd_Action(CMD_VOLUMEDOWN);
						Log_Println((char *) F("RC: Volume down"), LOGLEVEL_NOTICE);
					break;
				}
				case CONFIG_IR_RC_VOL_UP: {
					Cmd_Action(CMD_VOLUMEUP);
						Log_Println((char *) F("RC: Volume up"), LOGLEVEL_NOTICE);
					break;
				}
				default: {
					if (rcActionOk) {
						Log_Println((char *) F("RC: unknown"), LOGLEVEL_NOTICE);
					}
				}
			}
		}
	#endif
}
