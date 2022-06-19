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
    #ifdef RFID_READER_TYPE_MFRC522_I2C
		// Handling von I2C zentral
		#include "i2c.h"
        #include <MFRC522_I2C.h>
    #endif

    extern unsigned long Rfid_LastRfidCheckTimestamp;
	bool cardAppliedCurrentRun;
	bool cardRemovedCurrentRun;
	bool sameCardReapplied;
	bool cardApplied = false;
	bool checkRun = true;
    static void Rfid_Task(void *parameter);
	void scanRFID();
//	void stopScan();

    #ifdef RFID_READER_TYPE_MFRC522_I2C
        static MFRC522_I2C mfrc522(MFRC522_ADDR, MFRC522_RST_PIN, &i2cBusTwo);
    #endif
    #ifdef RFID_READER_TYPE_MFRC522_SPI
        static MFRC522 mfrc522(RFID_CS, RST_PIN);
    #endif

	void Rfid_Init_func(void) {
            mfrc522.PCD_Init();
            mfrc522.PCD_SetAntennaGain(rfidGain);
	}

    void Rfid_Init(void) {
        #ifdef RFID_READER_TYPE_MFRC522_SPI
            SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
            SPI.setFrequency(1000000);
        #endif

        // Init RC522 Card-Reader
        #if defined(RFID_READER_TYPE_MFRC522_I2C) 
            i2c_tsafe_execute(Rfid_Init_func,3);
		#elif RFID_READER_TYPE_MFRC522_SPI
        	Rfid_Init_func();
		#endif
            delay(10);


            xTaskCreatePinnedToCore(
                Rfid_Task,              /* Function to implement the task */
                "rfid",                 /* Name of the task */
                1536,                   /* Stack size in words */
                NULL,                   /* Task input parameter */
                1 | portPRIVILEGE_BIT,  /* Priority of the task */
                NULL,                   /* Task handle. */
                0          /* Core where the task should run */
            );

            Log_Println((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
        #endif
    }

	void Rfid_Task(void *parameter) {
		TickType_t xLastWakeTime;
		const TickType_t xFrequency = RFID_SCAN_INTERVAL;
		byte cardId[cardIdSize];
		String cardIdString;
		byte lastValidcardId[cardIdSize];
		sameCardReapplied = false;

		xLastWakeTime = xTaskGetTickCount();

        for (;;) {
			// Run Task at specific Frequency
			vTaskDelayUntil( &xLastWakeTime, xFrequency );
//			Serial.print("RFID Task last run since (ms):" + String(millis() - Rfid_LastRfidCheckTimestamp));
//			Rfid_LastRfidCheckTimestamp = millis();
        #if defined(RFID_READER_TYPE_MFRC522_I2C) 
            i2c_tsafe_execute(scanRFID,3);
		#elif RFID_READER_TYPE_MFRC522_SPI
        	scanRFID();
		#endif

			if (cardAppliedCurrentRun) {				// Card was just presented

				memcpy(cardId, mfrc522.uid.uidByte, cardIdSize);

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
					sameCardReapplied = false;
				}
		#ifdef PAUSE_WHEN_RFID_REMOVED		// Same-Card Check only makes Sense with Feature active
			if (!sameCardReapplied) {       // Don't allow to send card to queue if it's the same card again...
		#endif
				xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
		#ifdef PAUSE_WHEN_RFID_REMOVED		// Same-Card Check only makes Sense with Feature active
			} else {
				Log_Println((char *) FPSTR(rfidTagReapplied), LOGLEVEL_NOTICE);
				// If pause-button was pressed while card was not applied, playback could be active. If so: don't pause when card is reapplied again as the desired functionality would be reversed in this case.
				if (gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH) {
					AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);       // ... play/pause instead (but not for BT)
				}
				else if (gPlayProperties.playMode == NO_PLAYLIST) {
					xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
				}
			}
		#endif

			}
			if (cardRemovedCurrentRun) {
					Log_Println((char *) FPSTR(rfidTagRemoved), LOGLEVEL_NOTICE);

		#ifdef PAUSE_WHEN_RFID_REMOVED
					// Send Pause when Card is removed
					// Serial.print("Send PAUSEPLAY");
					AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
		#endif
			}
//			i2c_tsafe_execute(stopScan,3);
//			Serial.println("RFID Task needed (ms):" + String(millis() - Rfid_LastRfidCheckTimestamp);
		}
	}

	/**
	 * Helper Function returns True als long Card is Present
	 */
	bool PICC_CardPresent() {
		byte bufferATQA[2];
		byte bufferSize = sizeof(bufferATQA);
		byte result = mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);
		return (result == 1 || 0);
	} // End PICC_IsNewCardPresent()

	void scanRFID() {
		uint8_t control = 0x00;
		cardAppliedCurrentRun = false;
		cardRemovedCurrentRun = false;
//		Serial.println("RFID Scan: ");
		checkRun = !checkRun;

		if (!cardApplied) {
//		Serial.println("  card not applied");
			mfrc522.PICC_HaltA();
			mfrc522.PCD_StopCrypto1();
			if (mfrc522.PICC_IsNewCardPresent()) {
				if (mfrc522.PICC_ReadCardSerial()) {
				cardAppliedCurrentRun = true;
				cardApplied = true;
//			Serial.println("     card found");
				}
			}
			mfrc522.PICC_HaltA();
		}
		else if (checkRun) {		// Card Applied -> Check only every second run
//		Serial.println("  Check for card applied");
			while (1) {
			for (uint8_t i=0u; i<4; i++) {
				if (! PICC_CardPresent()) {
//					Serial.println("     card no longer present");
					control += 0x5;
					}
				control += 0x1;
				delay(5);
				}
//			Serial.print("  Control is: ");
			Serial.println(control);
			if (control <20 ) {
				// Card still present
				break;
				}
			else if (control > 47 ){
				// really no card Present
				cardApplied = false;
				cardRemovedCurrentRun = true;
				mfrc522.PICC_HaltA();
				mfrc522.PCD_StopCrypto1();
				break;
				}
			else if ((control > 19)) {
				// Communication Lost? Try reinitialization
				mfrc522.PCD_Init();
            	mfrc522.PCD_SetAntennaGain(rfidGain);
				}
			}
		}

	}

/*	void stopScan() {
		mfrc522.PICC_HaltA();
		mfrc522.PCD_StopCrypto1();
	}

    void Rfid_Cyclic(void) {
        // Not necessary as cyclic stuff performed by task Rfid_Task()
    }
*/
    void Rfid_Exit(void) {
		mfrc522.PCD_WriteRegister(mfrc522.CommandReg, mfrc522.PCD_NoCmdChange | 0x10);
    }

    void Rfid_WakeupCheck(void) {
    }

#endif