#include <Arduino.h>
#include "settings.h"

#include "AudioPlayer.h"
#include "HallEffectSensor.h"
#include "Log.h"
#include "MemX.h"
#include "Port.h"
#include "Queues.h"
#include "Rfid.h"
#include "System.h"

#include <Wire.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>

#ifdef RFID_READER_TYPE_PN5180
	#include <PN5180.h>
	#include <PN5180ISO14443.h>
	#include <PN5180ISO15693.h>
#endif

#define RFID_PN5180_STATE_INIT 0u

#define RFID_PN5180_NFC14443_STATE_RESET	1u
#define RFID_PN5180_NFC14443_STATE_READCARD 2u
#define RFID_PN5180_NFC14443_STATE_ACTIVE	99u

#define RFID_PN5180_NFC15693_STATE_RESET				3u
#define RFID_PN5180_NFC15693_STATE_SETUPRF				4u
#define RFID_PN5180_NFC15693_STATE_GETINVENTORY			5u
#define RFID_PN5180_NFC15693_STATE_DISABLEPRIVACYMODE	6u
#define RFID_PN5180_NFC15693_STATE_GETINVENTORY_PRIVACY 7u
#define RFID_PN5180_NFC15693_STATE_ACTIVE				100u

extern unsigned long Rfid_LastRfidCheckTimestamp;

#if (defined(PORT_EXPANDER_ENABLE) && (RFID_IRQ > 99))
extern TwoWire i2cBusTwo;
#endif

#ifdef RFID_READER_TYPE_PN5180
static void Rfid_Task(void *parameter);
TaskHandle_t rfidTaskHandle;

	#ifdef PN5180_ENABLE_LPCD
void Rfid_EnableLpcd(void);
bool enabledLpcdShutdown = false; // Indicates if LPCD should be activated as part of the shutdown-process

void Rfid_SetLpcdShutdownStatus(bool lpcdStatus) {
	enabledLpcdShutdown = lpcdStatus;
}

bool Rfid_GetLpcdShutdownStatus(void) {
	return enabledLpcdShutdown;
}
	#endif

void Rfid_Init(void) {
	#ifdef PN5180_ENABLE_LPCD
	// Check if wakeup-reason was card-detection (PN5180 only)
	// This only works if RFID.IRQ is connected to a GPIO and not to a port-expander
	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
	if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
		Rfid_WakeupCheck();
	}

		// wakeup-check if IRQ is connected to port-expander, signal arrives as pushbutton
		#if (defined(PORT_EXPANDER_ENABLE) && (RFID_IRQ > 99))
	if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
		// read IRQ state from port-expander
		i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK);
		delay(50);
		Port_Init();
		Port_Cyclic();
		uint8_t irqState = Port_Read(RFID_IRQ);
		if (irqState == LOW) {
			Log_Println("Wakeup caused by low power card-detection on port-expander", LOGLEVEL_NOTICE);
			Rfid_WakeupCheck();
		}
	}
		#endif

	// disable pin hold from deep sleep
	gpio_deep_sleep_hold_dis();
	gpio_hold_dis(gpio_num_t(RFID_CS)); // NSS
	gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
		#if (RFID_IRQ >= 0 && RFID_IRQ <= MAX_GPIO)
	pinMode(RFID_IRQ, INPUT); // Not necessary for port-expander as for pca9555 all pins are configured as input per default
		#endif
	#endif

	xTaskCreatePinnedToCore(
		Rfid_Task, /* Function to implement the task */
		"rfid", /* Name of the task */
		2176, /* Stack size in words */
		NULL, /* Task input parameter */
		2 | portPRIVILEGE_BIT, /* Priority of the task */
		&rfidTaskHandle, /* Task handle. */
		0 /* Core where the task should run */
	);
}

void Rfid_Cyclic(void) {
	// Not necessary as cyclic stuff performed by task Rfid_Task()
}

void Rfid_Task(void *parameter) {
	static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
	static PN5180ISO15693 nfc15693(RFID_CS, RFID_BUSY, RFID_RST);
	uint32_t lastTimeDetected14443 = 0;
	uint32_t lastTimeDetected15693 = 0;
	#ifdef PAUSE_WHEN_RFID_REMOVED
	byte lastValidcardId[cardIdSize];
	bool cardAppliedCurrentRun = false;
	bool cardAppliedLastRun = false;
	#endif
	uint8_t stateMachine = RFID_PN5180_STATE_INIT;
	static byte cardId[cardIdSize], lastCardId[cardIdSize];
	uint8_t uid[10];
	bool showDisablePrivacyNotification = true;

	// wait until queues are created
	while (gRfidCardQueue == NULL) {
		Log_Println(waitingForTaskQueues, LOGLEVEL_DEBUG);
		vTaskDelay(50);
	}

	for (;;) {
		vTaskDelay(portTICK_PERIOD_MS * 10u);
	#ifdef PN5180_ENABLE_LPCD
		if (Rfid_GetLpcdShutdownStatus()) {
			Rfid_EnableLpcd();
			Rfid_SetLpcdShutdownStatus(false); // give feedback that execution is complete
			while (true) {
				vTaskDelay(portTICK_PERIOD_MS * 100u); // there's no way back if shutdown was initiated
			}
		}
	#endif
		String cardIdString;
		bool cardReceived = false;
	#ifdef PAUSE_WHEN_RFID_REMOVED
		bool sameCardReapplied = false;
	#endif

		if (RFID_PN5180_STATE_INIT == stateMachine) {
			nfc14443.begin();
			nfc14443.reset();
			// show PN5180 reader version
			uint8_t firmwareVersion[2];
			nfc14443.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
			Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmwareVersion[1], firmwareVersion[0]);

			// activate RF field
			delay(4u);
			Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

			// 1. check for an ISO-14443 card
		} else if (RFID_PN5180_NFC14443_STATE_RESET == stateMachine) {
			nfc14443.reset();
			// Log_Printf(LOGLEVEL_DEBUG, "%u", uxTaskGetStackHighWaterMark(NULL));
		} else if (RFID_PN5180_NFC14443_STATE_READCARD == stateMachine) {

			if (nfc14443.readCardSerial(uid) >= 4) {
				cardReceived = true;
				stateMachine = RFID_PN5180_NFC14443_STATE_ACTIVE;
				lastTimeDetected14443 = millis();
	#ifdef PAUSE_WHEN_RFID_REMOVED
				cardAppliedCurrentRun = true;
	#endif
			} else {
				// Reset to dummy-value if no card is there
				// Necessary to differentiate between "card is still applied" and "card is re-applied again after removal"
				// lastTimeDetected14443 is used to prevent "new card detection with old card" with single events where no card was detected
				if (!lastTimeDetected14443 || (millis() - lastTimeDetected14443 >= 1000)) {
					lastTimeDetected14443 = 0;
	#ifdef PAUSE_WHEN_RFID_REMOVED
					cardAppliedCurrentRun = false;
	#endif
					for (uint8_t i = 0; i < cardIdSize; i++) {
						lastCardId[i] = 0;
					}
				} else {
					stateMachine = RFID_PN5180_NFC14443_STATE_ACTIVE; // Still consider first event as "active"
				}
			}

			// 2. check for an ISO-15693 card
		} else if (RFID_PN5180_NFC15693_STATE_RESET == stateMachine) {
			nfc15693.reset();
		} else if (RFID_PN5180_NFC15693_STATE_SETUPRF == stateMachine) {
			nfc15693.setupRF();
		} else if (RFID_PN5180_NFC15693_STATE_DISABLEPRIVACYMODE == stateMachine) {
			// check for ICODE-SLIX2 password protected tag
			// put your privacy password here, e.g.:
			// https://de.ifixit.com/Antworten/Ansehen/513422/nfc+Chips+f%C3%BCr+tonies+kaufen
			//
			// default factory password for ICODE-SLIX2 is {0x0F, 0x0F, 0x0F, 0x0F}
			//
			const uint8_t password[] = {0x0F, 0x0F, 0x0F, 0x0F};
			ISO15693ErrorCode myrc = nfc15693.disablePrivacyMode(password);
			if (ISO15693_EC_OK == myrc) {
				if (showDisablePrivacyNotification) {
					showDisablePrivacyNotification = false;
					Log_Println("disabling privacy-mode successful", LOGLEVEL_NOTICE);
				} else {
					// no privacy mode or disabling failed, skip next steps & restart state machine
					stateMachine = RFID_PN5180_NFC15693_STATE_GETINVENTORY_PRIVACY;
				}
			}
		} else if ((RFID_PN5180_NFC15693_STATE_GETINVENTORY == stateMachine) || (RFID_PN5180_NFC15693_STATE_GETINVENTORY_PRIVACY == stateMachine)) {
			// try to read ISO15693 inventory
			ISO15693ErrorCode rc = nfc15693.getInventory(uid);
			if (rc == ISO15693_EC_OK) {
				cardReceived = true;
				stateMachine = RFID_PN5180_NFC15693_STATE_ACTIVE;
				lastTimeDetected15693 = millis();
	#ifdef PAUSE_WHEN_RFID_REMOVED
				cardAppliedCurrentRun = true;
	#endif
			} else {
				// lastTimeDetected15693 is used to prevent "new card detection with old card" with single events where no card was detected
				if (!lastTimeDetected15693 || (millis() - lastTimeDetected15693 >= 400)) {
					lastTimeDetected15693 = 0;
	#ifdef PAUSE_WHEN_RFID_REMOVED
					cardAppliedCurrentRun = false;
	#endif
					for (uint8_t i = 0; i < cardIdSize; i++) {
						lastCardId[i] = 0;
					}
				} else {
					stateMachine = RFID_PN5180_NFC15693_STATE_ACTIVE;
				}
			}
		}

	#ifdef PAUSE_WHEN_RFID_REMOVED
		if (!cardAppliedCurrentRun && cardAppliedLastRun && !gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) { // Card removed => pause
			AudioPlayer_TrackControlToQueueSender(PAUSEPLAY);
			Log_Println(rfidTagRemoved, LOGLEVEL_NOTICE);
		}
		cardAppliedLastRun = cardAppliedCurrentRun;
	#endif

		// send card to queue
		if (cardReceived) {
			memcpy(cardId, uid, cardIdSize);

			// check for different card id
			if (memcmp((const void *) cardId, (const void *) lastCardId, sizeof(cardId)) == 0) {
				// reset state machine
				if (RFID_PN5180_NFC14443_STATE_ACTIVE == stateMachine) {
					stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
					continue;
				} else if (RFID_PN5180_NFC15693_STATE_ACTIVE == stateMachine) {
					stateMachine = RFID_PN5180_NFC15693_STATE_RESET;
					continue;
				}
			}

			memcpy(lastCardId, cardId, cardIdSize);
			showDisablePrivacyNotification = true;

	#ifdef HALLEFFECT_SENSOR_ENABLE
			cardId[cardIdSize - 1] = cardId[cardIdSize - 1] + gHallEffectSensor.waitForState(HallEffectWaitMS);
	#endif

	#ifdef PAUSE_WHEN_RFID_REMOVED
			if (memcmp((const void *) lastValidcardId, (const void *) cardId, sizeof(cardId)) == 0) {
				sameCardReapplied = true;
			}
	#endif

			String hexString;
			for (uint8_t i = 0u; i < cardIdSize; i++) {
				char str[4];
				snprintf(str, sizeof(str), "%02x%c", cardId[i], (i < cardIdSize - 1u) ? '-' : ' ');
				hexString += str;
			}
			Log_Printf(LOGLEVEL_NOTICE, rfidTagDetected, hexString.c_str());
			Log_Printf(LOGLEVEL_NOTICE, "Card type: %s", (RFID_PN5180_NFC14443_STATE_ACTIVE == stateMachine) ? "ISO-14443" : "ISO-15693");

			for (uint8_t i = 0u; i < cardIdSize; i++) {
				char num[4];
				snprintf(num, sizeof(num), "%03d", cardId[i]);
				cardIdString += num;
			}

	#ifdef PAUSE_WHEN_RFID_REMOVED
		#ifdef ACCEPT_SAME_RFID_AFTER_TRACK_END
			if (!sameCardReapplied || gPlayProperties.trackFinished || gPlayProperties.playlistFinished) { // Don't allow to send card to queue if it's the same card again if track or playlist is unfnished
		#else
			if (!sameCardReapplied) { // Don't allow to send card to queue if it's the same card again...
		#endif
				xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
			} else {
				// If pause-button was pressed while card was not applied, playback could be active. If so: don't pause when card is reapplied again as the desired functionality would be reversed in this case.
				if (gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) {
					AudioPlayer_TrackControlToQueueSender(PAUSEPLAY); // ... play/pause instead
					Log_Println(rfidTagReapplied, LOGLEVEL_NOTICE);
				}
			}
			memcpy(lastValidcardId, uid, cardIdSize);
	#else
			xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0); // If PAUSE_WHEN_RFID_REMOVED isn't active, every card-apply leads to new playlist-generation
	#endif
		}

		if (RFID_PN5180_NFC14443_STATE_ACTIVE == stateMachine) { // If 14443 is active, bypass 15693 as next check (performance)
			stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
		} else if (RFID_PN5180_NFC15693_STATE_ACTIVE == stateMachine) { // If 15693 is active, bypass 14443 as next check (performance)
			stateMachine = RFID_PN5180_NFC15693_STATE_RESET;
		} else {
			stateMachine++;
			if (stateMachine > RFID_PN5180_NFC15693_STATE_GETINVENTORY_PRIVACY) {
				stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
			}
		}
	}
}

void Rfid_Exit(void) {
	#ifdef PN5180_ENABLE_LPCD
	Rfid_SetLpcdShutdownStatus(true);
	while (Rfid_GetLpcdShutdownStatus()) { // Make sure init of LPCD is complete!
		vTaskDelay(portTICK_PERIOD_MS * 10u);
	}
	#endif
	vTaskDelete(rfidTaskHandle);
}

// Handles activation of LPCD (while shutdown is in progress)
void Rfid_EnableLpcd(void) {
	// goto low power card detection mode
	#ifdef PN5180_ENABLE_LPCD
	static PN5180 nfc(RFID_CS, RFID_BUSY, RFID_RST);
	nfc.begin();
	nfc.reset();
	// show PN5180 reader version
	uint8_t firmwareVersion[2];
	nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
	Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmwareVersion[1], firmwareVersion[0]);

	// check firmware version: PN5180 firmware < 4.0 has several bugs preventing the LPCD mode
	// you can flash latest firmware with this project: https://github.com/abidxraihan/PN5180_Updater_ESP32
	if (firmwareVersion[1] < 4) {
		Log_Println("This PN5180 firmware does not work with LPCD! use firmware >= 4.0", LOGLEVEL_ERROR);
		return;
	}
	Log_Println("prepare low power card detection...", LOGLEVEL_NOTICE);
	uint8_t irqConfig = 0b0000000; // Set IRQ active low + clear IRQ-register
	nfc.writeEEprom(IRQ_PIN_CONFIG, &irqConfig, 1);
	/*
	nfc.readEEprom(IRQ_PIN_CONFIG, &irqConfig, 1);
	Log_Printf("IRQ_PIN_CONFIG=0x%02X", irqConfig)
	*/
	nfc.prepareLPCD();
	Log_Printf(LOGLEVEL_DEBUG, "PN5180 IRQ PIN (%d) state: %d", RFID_IRQ, Port_Read(RFID_IRQ));
	// turn on LPCD
	uint16_t wakeupCounterInMs = 0x3FF; //  must be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
	if (nfc.switchToLPCD(wakeupCounterInMs)) {
		Log_Println("switch to low power card detection: success", LOGLEVEL_NOTICE);
		// configure wakeup pin for deep-sleep wake-up, use ext1. For a real GPIO only, not PE
		#if (RFID_IRQ >= 0 && RFID_IRQ <= MAX_GPIO)
		if (ESP_ERR_INVALID_ARG == esp_sleep_enable_ext1_wakeup((1ULL << (RFID_IRQ)), ESP_EXT1_WAKEUP_ALL_LOW)) {
			Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, RFID_IRQ);
		}
		#endif
		// freeze pin states in deep sleep
		gpio_hold_en(gpio_num_t(RFID_CS)); // CS/NSS
		gpio_hold_en(gpio_num_t(RFID_RST)); // RST
		gpio_deep_sleep_hold_en();
	} else {
		Log_Println("switchToLPCD failed", LOGLEVEL_ERROR);
	}
	#endif
}

// wake up from LPCD, check card is present in NVS
void Rfid_WakeupCheck(void) {
	#ifdef PN5180_ENABLE_LPCD
	// disable pin hold from deep sleep
	gpio_deep_sleep_hold_dis();
	gpio_hold_dis(gpio_num_t(RFID_CS)); // NSS
	gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
		#if (RFID_IRQ >= 0 && RFID_IRQ <= MAX_GPIO)
	pinMode(RFID_IRQ, INPUT);
		#endif

	// read card serial to check NVS for entries
	uint8_t uid[10];
	bool isCardPresent;
	bool cardInNVS = false;

	static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
	nfc14443.begin();
	nfc14443.reset();
	// enable RF field
	nfc14443.setupRF();

	// 1. check for ISO-14443 card present
	isCardPresent = nfc14443.readCardSerial(uid) >= 4;

	// 2. check for ISO-15693 card present
	if (!isCardPresent) {
		static PN5180ISO15693 nfc15693(RFID_CS, RFID_BUSY, RFID_RST);
		nfc15693.begin();
		nfc15693.setupRF();
		// do not handle encrypted cards in any way here
		isCardPresent = nfc15693.getInventory(uid) == ISO15693_EC_OK;
	}

	// check for card id in NVS
	if (isCardPresent) {
		// uint8_t[10] -> char[cardIdStringSize]
		char tagId[cardIdStringSize];
		size_t pos = 0;
		for (size_t i = 0; i < 10; i++) {
			pos += snprintf(tagId + pos, cardIdStringSize - pos, "%03d", uid[i]);
		}
		tagId[cardIdStringSize - 1] = '\0';

		// Try to lookup tagId in NVS
		if (gPrefsRfid.isKey(tagId)) {
			cardInNVS = gPrefsRfid.getString(tagId, "-1").compareTo("-1");
		}
	}

	if (!cardInNVS) {
		// no card found or card not in NVS, go back to deep sleep
		nfc14443.reset();
		uint16_t wakeupCounterInMs = 0x3FF; //  needs to be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
		if (nfc14443.switchToLPCD(wakeupCounterInMs)) {
			Log_Println(lowPowerCardSuccess, LOGLEVEL_INFO);

		// configure wakeup pin for deep-sleep wake-up, use ext1
		#if (RFID_IRQ >= 0 && RFID_IRQ <= MAX_GPIO)
			// configure wakeup pin for deep-sleep wake-up, use ext1. For a real GPIO only, not PE
			esp_sleep_enable_ext1_wakeup((1ULL << (RFID_IRQ)), ESP_EXT1_WAKEUP_ALL_LOW);
		#endif
		#if (defined(PORT_EXPANDER_ENABLE) && (RFID_IRQ > 99))
			// reset IRQ state on port-expander
			Port_Exit();
		#endif

			// freeze pin states in deep sleep
			gpio_hold_en(gpio_num_t(RFID_CS)); // CS/NSS
			gpio_hold_en(gpio_num_t(RFID_RST)); // RST
			gpio_deep_sleep_hold_en();
			Log_Println(wakeUpRfidNoCard, LOGLEVEL_ERROR);
			esp_deep_sleep_start();
		} else {
			Log_Println("switchToLPCD failed", LOGLEVEL_ERROR);
		}
	}
	nfc14443.end();
	#endif
}
#endif
