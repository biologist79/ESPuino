#include <Arduino.h>
#include "settings.h"
#include "Rfid.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "System.h"
#include <esp_task_wdt.h>
#include "AudioPlayer.h"


#if defined RFID_READER_TYPE_PN532_SPI || defined RFID_READER_TYPE_PN532_I2C
    #ifdef RFID_READER_TYPE_PN532_SPI
        #include "PN532/PN532.h"
        #include "PN532_SPI/PN532_SPI.h"
    #endif
    #if defined(RFID_READER_TYPE_PN532_I2C) || defined(PORT_EXPANDER_ENABLE)
        #include "Wire.h"
    #endif
    #ifdef RFID_READER_TYPE_PN532_I2C
        #include "PN532/PN532.h"
        #include "PN532_I2C/PN532_I2C.h"
    #endif

    extern unsigned long Rfid_LastRfidCheckTimestamp;
    static void Rfid_Task(void *parameter);

    #ifdef RFID_READER_TYPE_PN532_I2C
        extern TwoWire i2cBusTwo;
        static PN532_I2C pn532_i2c(i2cBusTwo);
        static PN532 nfc(pn532_i2c);
    #endif
    #ifdef RFID_READER_TYPE_MFRC522_SPI
        static MFRC522 mfrc522(RFID_CS, RST_PIN);
    #endif

    void Rfid_Init(void) {
        #ifdef RFID_READER_TYPE_MFRC522_SPI
            SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
            SPI.setFrequency(1000000);
        #endif

        // Init RC522 Card-Reader
        #if defined(RFID_READER_TYPE_PN532_I2C) || defined(RFID_READER_TYPE_PN532_SPI)
            nfc.begin();
            uint32_t versiondata = nfc.getFirmwareVersion();

            if (!versiondata) {
                Serial.print("Didn't Find PN53x Module HALT!!!");

                while (1); // Halt
            }

            nfc.SAMConfig();

            delay(50);
            Log_Println((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);

            xTaskCreatePinnedToCore(
                Rfid_Task,              /* Function to implement the task */
                "rfid",                 /* Name of the task */
                1536,                   /* Stack size in words */
                NULL,                   /* Task input parameter */
                2 | portPRIVILEGE_BIT,  /* Priority of the task */
                NULL,                   /* Task handle. */
                1                       /* Core where the task should run */
            );
        #endif
    }

	void Rfid_Task(void *parameter) {
		uint8_t control = 0x00;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
        uint8_t uidLength;      

        for (;;) {
			if (RFID_SCAN_INTERVAL/2 >= 20) {
                vTaskDelay(portTICK_RATE_MS * (RFID_SCAN_INTERVAL/2));
            } else {
               vTaskDelay(portTICK_RATE_MS * 20);
            }
			byte cardId[cardIdSize];
			String cardIdString;
			#ifdef PAUSE_WHEN_RFID_REMOVED
				byte lastValidcardId[cardIdSize];
				bool cardAppliedCurrentRun = false;
				bool sameCardReapplied = false;
			#endif
			if ((millis() - Rfid_LastRfidCheckTimestamp) >= RFID_SCAN_INTERVAL) {
                //snprintf(Log_Buffer, Log_BufferLength, "%u", uxTaskGetStackHighWaterMark(NULL));
                //Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

				Rfid_LastRfidCheckTimestamp = millis();
				// Reset the loop if no new card is present on the sensor/reader. This saves the entire process when idle.

                boolean success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

                if (!success) {
					continue;
				}

				#ifdef PAUSE_WHEN_RFID_REMOVED
                    cardAppliedCurrentRun = true;
                #endif

				#ifdef PAUSE_WHEN_RFID_REMOVED
					if (memcmp((const void *)lastValidcardId, (const void *)cardId, sizeof(cardId)) == 0) {
						sameCardReapplied = true;
					}
				#endif

				Log_Print((char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE);
				for (uint8_t i=0u; i < cardIdSize; i++) {
					snprintf(Log_Buffer, Log_BufferLength, "%02x%s", uid[i], (i < cardIdSize - 1u) ? "-" : "\n");
					Log_Print(Log_Buffer, LOGLEVEL_NOTICE);
				}

				for (uint8_t i=0u; i < cardIdSize; i++) {
					char num[4];
					snprintf(num, sizeof(num), "%03d", uid[i]);
					cardIdString += num;
				}

				#ifdef PAUSE_WHEN_RFID_REMOVED
					if (!sameCardReapplied) {       // Don't allow to send card to queue if it's the same card again...
						xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
					} else {
						// If pause-button was pressed while card was not applied, playback could be active. If so: don't pause when card is reapplied again as the desired functionality would be reversed in this case.
						if (gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH) {
							AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);       // ... play/pause instead (but not for BT)
						}
					}
					memcpy(lastValidcardId, mfrc522.uid.uidByte, cardIdSize);
				#else
					xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);        // If PAUSE_WHEN_RFID_REMOVED isn't active, every card-apply leads to new playlist-generation
				#endif
			}
		}
	}

    void Rfid_Cyclic(void) {
        // Not necessary as cyclic stuff performed by task Rfid_Task()
    }

    void Rfid_Exit(void) {
    }

    void Rfid_WakeupCheck(void) {
    }

#endif
