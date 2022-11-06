#include <Arduino.h>
#include "settings.h"
#include "IrReceiver.h"
#include "AudioPlayer.h"
#include "Cmd.h"
#include "Queues.h"
#include "System.h"

#ifdef IR_CONTROL_ENABLE
    #include <IRremote.h>
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

        if (IrReceiver.decode()){

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
                        Serial.println(F("RC: Play"));
                    }
                    break;
                }
                case RC_PAUSE: {
                    if (rcActionOk) {
                        Cmd_Action(CMD_PLAYPAUSE);
                        Serial.println(F("RC: Pause"));
                    }
                    break;
                }
                case RC_NEXT: {
                    if (rcActionOk) {
                        Cmd_Action(CMD_NEXTTRACK);
                        Serial.println(F("RC: Next"));
                    }
                    break;
                }
                case RC_PREVIOUS: {
                    if (rcActionOk) {
                        Cmd_Action(CMD_PREVTRACK);
                        Serial.println(F("RC: Previous"));
                    }
                    break;
                }
                case RC_FIRST: {
                    if (rcActionOk) {
                        Cmd_Action(CMD_FIRSTTRACK);
                        Serial.println(F("RC: First"));
                    }
                    break;
                }
                case RC_LAST: {
                    if (rcActionOk) {
                        Cmd_Action(CMD_LASTTRACK);
                        Serial.println(F("RC: Last"));
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
                        Serial.println(F("RC: Mute"));
                    }
                    break;
                }
                case RC_BLUETOOTH: {
                    if (rcActionOk) {
                        Cmd_Action(CMD_TOGGLE_BLUETOOTH_SINK_MODE);
                        Serial.println(F("RC: Bluetooth sink"));
                    }
                    break;
                }
                // +++ todo: bluetooth source mode +++
                case RC_FTP: {
                    if (rcActionOk) {
                        Cmd_Action(CMD_ENABLE_FTP_SERVER);
                        Serial.println(F("RC: FTP"));
                    }
                    break;
                }
                case RC_SHUTDOWN: {
                    if (rcActionOk) {
                        System_RequestSleep();
                        Serial.println(F("RC: Shutdown"));
                    }
                    break;
                }
                case RC_VOL_DOWN: {
                    Cmd_Action(CMD_VOLUMEDOWN);
                    Serial.println(F("RC: Volume down"));
                    break;
                }
                case RC_VOL_UP: {
                    Cmd_Action(CMD_VOLUMEUP);
                    Serial.println(F("RC: Volume up"));
                    break;
                }
                default: {
                    if (rcActionOk) {
                        Serial.println(F("RC: unknown"));
                    }
                }
            }
        }
    #endif
}