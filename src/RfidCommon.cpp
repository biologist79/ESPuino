#include <Arduino.h>
#include "settings.h"
#include "Rfid.h"
#include "AudioPlayer.h"
#include "Cmd.h"
#include "Common.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "Queues.h"
#include "System.h"
#include "Web.h"

unsigned long Rfid_LastRfidCheckTimestamp = 0;
char *gCurrentRfidTagId = NULL;

// Tries to lookup RFID-tag-string in NVS and extracts parameter from it if found
void Rfid_PreferenceLookupHandler(void) {
    #if defined (RFID_READER_TYPE_MFRC522_SPI) || defined (RFID_READER_TYPE_MFRC522_I2C) || defined(RFID_READER_TYPE_PN5180)
        BaseType_t rfidStatus;
        char rfidTagId[cardIdStringSize];
        char _file[255];
        uint32_t _lastPlayPos = 0;
        uint16_t _trackLastPlayed = 0;
        uint32_t _playMode = 1;

        rfidStatus = xQueueReceive(gRfidCardQueue, &rfidTagId, 0);
        if (rfidStatus == pdPASS) {
            System_UpdateActivityTimer();
            free(gCurrentRfidTagId);
            gCurrentRfidTagId = x_strdup(rfidTagId);
            snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(rfidTagReceived), gCurrentRfidTagId);
            Web_SendWebsocketData(0, 10); // Push new rfidTagId to all websocket-clients
            Log_Println(Log_Buffer, LOGLEVEL_INFO);

            String s = gPrefsRfid.getString(gCurrentRfidTagId, "-1"); // Try to lookup rfidId in NVS
            if (!s.compareTo("-1")) {
                Log_Println((char *) FPSTR(rfidTagUnknownInNvs), LOGLEVEL_ERROR);
                System_IndicateError();
                return;
            }

            char *token;
            uint8_t i = 1;
            token = strtok((char *)s.c_str(), stringDelimiter);
            while (token != NULL) { // Try to extract data from string after lookup
                if (i == 1) {
                    strncpy(_file, token, sizeof(_file) / sizeof(_file[0]));
                } else if (i == 2) {
                    _lastPlayPos = strtoul(token, NULL, 10);
                } else if (i == 3) {
                    _playMode = strtoul(token, NULL, 10);
                } else if (i == 4) {
                    _trackLastPlayed = strtoul(token, NULL, 10);
                }
                i++;
                token = strtok(NULL, stringDelimiter);
            }

            if (i != 5) {
                Log_Println((char *) FPSTR(errorOccuredNvs), LOGLEVEL_ERROR);
                System_IndicateError();
            } else {
                // Only pass file to queue if strtok revealed 3 items
                if (_playMode >= 100) {
                    // Modification-cards can change some settings (e.g. introducing track-looping or sleep after track/playlist).
                    Cmd_Action(_playMode);
                } else {
                    #ifdef MQTT_ENABLE
                        publishMqtt((char *) FPSTR(topicRfidState), gCurrentRfidTagId, false);
                    #endif

                    #ifdef BLUETOOTH_ENABLE
                        // if music rfid was read, go back to normal mode
                        if (System_GetOperationMode() == OPMODE_BLUETOOTH) {
                            System_SetOperationMode(OPMODE_NORMAL);
                        }
                    #endif

                    AudioPlayer_TrackQueueDispatcher(_file, _lastPlayPos, _playMode, _trackLastPlayed);
                }
            }
        }
    #endif
}