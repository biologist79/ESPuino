#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "HallEffectSensor.h"
#include "Log.h"
#include "MemX.h"
#include "Queues.h"
#include "Rfid.h"
#include "RfidConfig.h"
#include "System.h"

#include <esp_task_wdt.h>

#if defined(RFID_READER_TYPE_RUNTIME) || defined(RFID_READER_TYPE_MFRC522_SPI) || defined(RFID_READER_TYPE_MFRC522_I2C)
	#include <MFRC522.h>
	#define MFRC522_firmware_referenceV0_0
	#define MFRC522_firmware_referenceV1_0
	#define MFRC522_firmware_referenceV2_0
	#define FM17522_firmware_reference
	#if defined(RFID_READER_TYPE_RUNTIME) || defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE)
		#include "Wire.h"
	#endif
	#include <MFRC522_I2C.h>

extern unsigned long Rfid_LastRfidCheckTimestamp;
extern TaskHandle_t rfidTaskHandle;
static void RfidMfrc522_Task(void *parameter);

	#if defined(RFID_READER_TYPE_RUNTIME) || defined(RFID_READER_TYPE_MFRC522_I2C)
extern TwoWire i2cBusTwo;
static MFRC522_I2C mfrc522I2C(MFRC522_ADDR, RST_PIN, &i2cBusTwo);
	#endif
	#if defined(RFID_READER_TYPE_RUNTIME) || defined(RFID_READER_TYPE_MFRC522_SPI)
static MFRC522 mfrc522(RFID_CS, RST_PIN);
	#endif

void RfidMfrc522_Init(uint8_t readerType) {
	if (readerType == 1) {
		SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);
		SPI.setFrequency(1000000);
		mfrc522.PCD_Init();
		delay(10);
		// byte firmwareVersion = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
		// Log_Printf(LOGLEVEL_DEBUG, "RC522 firmware version=%#lx", firmwareVersion);
		mfrc522.PCD_SetAntennaGain(rfidGain);
	} else if (readerType == 2) {
	#if defined(I2C_2_ENABLE)
		mfrc522I2C.PCD_Init();
		delay(10);
		// byte firmwareVersion = mfrc522I2C.PCD_ReadRegister(MFRC522_I2C::VersionReg);
		// Log_Printf(LOGLEVEL_DEBUG, "RC522 I2C firmware version=%#lx", firmwareVersion);
		mfrc522I2C.PCD_SetAntennaGain(rfidGain);
	#endif
	} else {
		Log_Println("RfidMfrc522_Init: unsupported reader type", LOGLEVEL_ERROR);
		return;
	}

	delay(50);
	Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

	xTaskCreatePinnedToCore(
		RfidMfrc522_Task, /* Function to implement the task */
		"rfid", /* Name of the task */
		3072, /* Stack size in words */
		NULL, /* Task input parameter */
		2 | portPRIVILEGE_BIT, /* Priority of the task */
		&rfidTaskHandle, /* Task handle. */
		0 /* Core where the task should run */
	);
}

void RfidMfrc522_TaskReset(void) {
	Rfid_LastRfidCheckTimestamp = millis();
}

template <typename Reader>
static void RfidMfrc522_TaskImpl(Reader &reader) {
	uint8_t control = 0x00;

	for (;;) {
		if (RFID_SCAN_INTERVAL / 2 >= 20) {
			vTaskDelay(portTICK_PERIOD_MS * (RFID_SCAN_INTERVAL / 2));
		} else {
			vTaskDelay(portTICK_PERIOD_MS * 20);
		}
		byte cardId[cardIdSize];
		String cardIdString;
		byte lastValidcardId[cardIdSize];
		bool sameCardReapplied = false;
		if ((millis() - Rfid_LastRfidCheckTimestamp) >= RFID_SCAN_INTERVAL) {
			// Log_Printf(LOGLEVEL_DEBUG, "%u", uxTaskGetStackHighWaterMark(NULL));

			Rfid_LastRfidCheckTimestamp = millis();
			// Reset the loop if no new card is present on the sensor/reader. This saves the entire process when idle.

			if (!reader.PICC_IsNewCardPresent()) {
				continue;
			}

			// Select one of the cards
			if (!reader.PICC_ReadCardSerial()) {
				continue;
			}

			if (!gPlayProperties.pauseIfRfidRemoved) {
				reader.PICC_HaltA();
				reader.PCD_StopCrypto1();
			}

			memcpy(cardId, reader.uid.uidByte, cardIdSize);

	#ifdef HALLEFFECT_SENSOR_ENABLE
			cardId[cardIdSize - 1] = cardId[cardIdSize - 1] + gHallEffectSensor.waitForState(HallEffectWaitMS);
	#endif

			if (memcmp((const void *) lastValidcardId, (const void *) cardId, sizeof(cardId)) == 0) {
				sameCardReapplied = true;
			}

			String hexString;
			for (uint8_t i = 0u; i < cardIdSize; i++) {
				char str[4];
				snprintf(str, sizeof(str), "%02x%c", cardId[i], (i < cardIdSize - 1u) ? '-' : ' ');
				hexString += str;
			}
			Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, hexString.c_str());

			for (uint8_t i = 0u; i < cardIdSize; i++) {
				char num[4];
				snprintf(num, sizeof(num), "%03d", cardId[i]);
				cardIdString += num;
			}

			if (gPlayProperties.pauseIfRfidRemoved) {
	#ifdef ACCEPT_SAME_RFID_AFTER_TRACK_END
				if (!sameCardReapplied || gPlayProperties.trackFinished || gPlayProperties.playlistFinished) { // Don't allow to send card to queue if it's the same card again if track or playlist is unfnished
	#else
				if (!sameCardReapplied) { // Don't allow to send card to queue if it's the same card again...
	#endif
					xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
				} else {
					// If pause-button was pressed while card was not applied, playback could be active. If so: don't pause when card is reapplied again as the desired functionality would be reversed in this case.
					if (gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) {
						AudioPlayer_SetTrackControl(PAUSEPLAY); // ... play/pause instead (but not for BT)
					}
				}
				memcpy(lastValidcardId, reader.uid.uidByte, cardIdSize);
			} else {
				xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0); // If pauseIfRfidRemoved isn't active, every card-apply leads to new playlist-generation
			}

			if (gPlayProperties.pauseIfRfidRemoved) {
				// https://github.com/miguelbalboa/rfid/issues/188; voodoo! :-)
				while (true) {
					if (RFID_SCAN_INTERVAL / 2 >= 20) {
						vTaskDelay(portTICK_PERIOD_MS * (RFID_SCAN_INTERVAL / 2));
					} else {
						vTaskDelay(portTICK_PERIOD_MS * 20);
					}
					control = 0;
					for (uint8_t i = 0u; i < 3; i++) {
						if (!reader.PICC_IsNewCardPresent()) {
							if (reader.PICC_ReadCardSerial()) {
								control |= 0x16;
							}
							if (reader.PICC_ReadCardSerial()) {
								control |= 0x16;
							}
							control += 0x1;
						}
						control += 0x4;
					}

					if (control == 13 || control == 14) {
						// card is still there
					} else {
						break;
					}
				}

				Log_Println(rfidTagRemoved, LOGLEVEL_NOTICE);
				if (!gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) {
					AudioPlayer_SetTrackControl(PAUSEPLAY);
					Log_Println(rfidTagReapplied, LOGLEVEL_NOTICE);
				}
				reader.PICC_HaltA();
				reader.PCD_StopCrypto1();
			}
		}
	}
}

void RfidMfrc522_Task(void *parameter) {
	if (RfidConfig_GetReaderType() == RfidReaderType::TYPE_MFRC522_I2C) {
	#if defined(I2C_2_ENABLE)
		RfidMfrc522_TaskImpl(mfrc522I2C);
	#endif
	} else {
		RfidMfrc522_TaskImpl(mfrc522);
	}
}

void RfidMfrc522_Cyclic(void) {
	// Not necessary as cyclic stuff performed by task Rfid_Task()
}

void RfidMfrc522_Exit(void) {
	if (RfidConfig_GetReaderType() != RfidReaderType::TYPE_MFRC522_I2C) {
		mfrc522.PCD_SoftPowerDown();
	} else {
	#if defined(I2C_2_ENABLE)
		mfrc522I2C.PCD_Reset();
	#endif
	}
}

void RfidMfrc522_WakeupCheck(void) {
}

#endif
