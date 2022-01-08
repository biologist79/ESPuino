#include <Arduino.h>
#include "settings.h"
#include "Rfid.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "System.h"
#include <esp_task_wdt.h>
#include "AudioPlayer.h"

#if defined RFID_READER_TYPE_MFRC522_SPI || defined RFID_READER_TYPE_MFRC522_I2C
    #ifdef RFID_READER_TYPE_MFRC522_SPI
        #include <MFRC522.h>
    #endif
    #if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE)
        #include "Wire.h"
    #endif
    #ifdef RFID_READER_TYPE_MFRC522_I2C
        #include <MFRC522_I2C.h>
    #endif

    extern unsigned long Rfid_LastRfidCheckTimestamp;
    static void Rfid_Task(void *parameter);

    #ifdef RFID_READER_TYPE_MFRC522_I2C
        extern TwoWire i2cBusTwo;
        static MFRC522_I2C mfrc522(MFRC522_ADDR, MFRC522_RST_PIN, &i2cBusTwo);
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
        #if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(RFID_READER_TYPE_MFRC522_SPI)
            mfrc522.PCD_Init();
            mfrc522.PCD_SetAntennaGain(rfidGain);
            delay(10);

            xTaskCreatePinnedToCore(
                Rfid_Task,              /* Function to implement the task */
                "rfid",                 /* Name of the task */
                1536,                   /* Stack size in words */
                NULL,                   /* Task input parameter */
                1 | portPRIVILEGE_BIT,  /* Priority of the task */
                NULL,                   /* Task handle. */
                1          /* Core where the task should run */
            );

            Log_Println((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
        #endif
    }

	void Rfid_Task(void *parameter) {

		uint8_t control;
		byte cardId[cardIdSize];
		String cardIdString;
		byte lastValidcardId[cardIdSize];
		bool cardAppliedCurrentRun;
		bool cardRemovedCurrentRun;
		bool sameCardReapplied;
		bool cardApplied = false;

        for (;;) {
			if ((millis() - Rfid_LastRfidCheckTimestamp) >= RFID_SCAN_INTERVAL) {

				cardAppliedCurrentRun = false;
				cardRemovedCurrentRun = false;
				sameCardReapplied = false;
				Rfid_LastRfidCheckTimestamp = millis();

				// Check Status of Card if New or Removed in actual Run
				// https://github.com/miguelbalboa/rfid/issues/188; voodoo! :-)
				while (true) {
					control=0x00;
					for (uint8_t i=0u; i<3; i++) {
						if (mfrc522.PICC_IsNewCardPresent()) {
							if (mfrc522.PICC_ReadCardSerial()) {
								control |= 0x16;
							}
							if (mfrc522.PICC_ReadCardSerial()) {
								control |= 0x16;
							}
							control += 0x1;
						}
						control += 0x4;
					}
/*
						Serial.print("Card Control is: ");
						Serial.println(control);
*/
					if (control > 30) {
						//card is present
//						Serial.println("Card present in run");
						cardAppliedCurrentRun = true;
						break; // Break Loop 
						}
					else {
						if (cardApplied) {
							//card is removed
//							Serial.println("Card removed in run");
							cardRemovedCurrentRun = true;
							}
						break;
					}
				}

				if (cardAppliedCurrentRun && !cardApplied) {				// Card was just presented

					memcpy(cardId, mfrc522.uid.uidByte, cardIdSize);
					cardApplied = true;

					if (memcmp((const void *)lastValidcardId, (const void *)cardId, cardIdSize) == 0) {
						sameCardReapplied = true;
						}
					else {
						Log_Print((char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE);
						cardIdString = "";
						for (uint8_t i=0u; i < cardIdSize; i++) {
							snprintf(Log_Buffer, Log_BufferLength, "%02x%s", cardId[i], (i < cardIdSize - 1u) ? "-" : "\n");
							Log_Print(Log_Buffer, LOGLEVEL_NOTICE);
							char num[4];
							snprintf(num, sizeof(num), "%03d", cardId[i]);
							cardIdString += num;
						}
						memcpy(lastValidcardId, cardId, cardIdSize);
					}

				if (!sameCardReapplied) {       // Don't allow to send card to queue if it's the same card again...
					xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
				} else {
					Log_Println((char *) FPSTR(rfidTagReapplied), LOGLEVEL_NOTICE);
					// If pause-button was pressed while card was not applied, playback could be active. If so: don't pause when card is reapplied again as the desired functionality would be reversed in this case.
					if (gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH) {
						AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);       // ... play/pause instead (but not for BT)
					} else if (gPlayProperties.playMode == NO_PLAYLIST) {
						xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
					}
				}

				} else {
					if (cardRemovedCurrentRun) {	// Send Pause when no Card is removed
						Log_Println((char *) FPSTR(rfidTagRemoved), LOGLEVEL_NOTICE);

			#ifdef PAUSE_WHEN_RFID_REMOVED
						AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
			#endif

						mfrc522.PICC_HaltA();
						mfrc522.PCD_StopCrypto1();

						cardApplied = false;
					 }
				}
			}

			if (RFID_SCAN_INTERVAL/2 >= 50) {
                vTaskDelay((RFID_SCAN_INTERVAL/2) / portTICK_RATE_MS);
            } else {
               vTaskDelay(50 / portTICK_RATE_MS);
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