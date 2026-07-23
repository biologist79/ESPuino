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

#include <SPI.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <freertos/task.h>

#if defined(RFID_READER_TYPE_RUNTIME)
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

// Silent wedge-vs-removal disambiguation before declaring a card removed.
// The PN5180 can start returning corrupted responses and then stop answering until it is reset
// (per the PN5180-Library author). A wedged reader is indistinguishable from a removed card - reads
// just fail - so declaring removal purely on the pn5180Debounce timeout would pause playback on a
// card that is still present. When reads have failed for the whole debounce window, run ONE silent
// re-init sweep before declaring removal to tell the two apart: it re-runs the reader's own
// RESET/SETUPRF/read states (the same sequence the post-removal path uses), steered so the full
// cross-protocol sweep completes before the wedged protocol is re-read, with removal suppressed
// throughout. Re-find the card -> it was a wedge: continue silently (the same-card fast path leaves
// lastCardId intact, so no re-queue or pause). Sweep finds nothing -> real removal: declare it. One
// sweep per failure episode; a removed card is declared removed one sweep after the debounce.

extern unsigned long Rfid_LastRfidCheckTimestamp;
extern TaskHandle_t rfidTaskHandle;

#if (defined(PORT_EXPANDER_ENABLE) && (RFID_IRQ > 99))
extern TwoWire i2cBusTwo;
#endif

#if defined(RFID_READER_TYPE_RUNTIME)
static void RfidPn5180_Task(void *parameter);
uint8_t stateMachine = RFID_PN5180_STATE_INIT;

static bool Rfid_Pn5180LpcdEnabled(void) {
	return gPrefsRfid.getBool("pn5180Lpcd", false);
}

void Rfid_EnableLpcd(void);
void RfidPn5180_WakeupCheck(void);
bool enabledLpcdShutdown = false; // Indicates if LPCD should be activated as part of the shutdown-process

void Rfid_SetLpcdShutdownStatus(bool lpcdStatus) {
	enabledLpcdShutdown = lpcdStatus;
}

bool Rfid_GetLpcdShutdownStatus(void) {
	return enabledLpcdShutdown;
}

// Handles a possible deep-sleep wakeup via card-detection and releases GPIO pin holds left
// over from a previous LPCD-armed deep-sleep. Must run early (before other peripherals are
// powered), since a wakeup without a known card can send the device straight back to sleep
// via RfidPn5180_WakeupCheck(). Does NOT start the scanning task: RfidPn5180_Task performing
// SPI traffic concurrently with the rest of setup()'s peripheral init (Power_PeripheralOn(),
// Led_Init(), SdCard_Init(), ...) was found to cause an intermittent full boot-hang, so task
// creation is deferred to RfidPn5180_StartTask(), called once peripherals are up.
void RfidPn5180_WakeupHandling(void) {
	if (Rfid_Pn5180LpcdEnabled()) {
		// Check if wakeup-reason was card-detection (PN5180 only)
		// This only works if RFID.IRQ is connected to a GPIO and not to a port-expander
		esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
		if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
			RfidPn5180_WakeupCheck();
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
				RfidPn5180_WakeupCheck();
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
	}
}

void RfidPn5180_StartTask(void) {
	if (rfidTaskHandle == NULL) {
		xTaskCreatePinnedToCore(
			RfidPn5180_Task, /* Function to implement the task */
			"rfid", /* Name of the task */
			3072, /* Stack size in words */
			NULL, /* Task input parameter */
			2 | portPRIVILEGE_BIT, /* Priority of the task */
			&rfidTaskHandle, /* Task handle. */
			0 /* Core where the task should run */
		);
	}
}

void RfidPn5180_Init(void) {
	RfidPn5180_WakeupHandling();
	RfidPn5180_StartTask();
}

void RfidPn5180_Cyclic(void) {
	// Not necessary as cyclic stuff performed by task Rfid_Task()
}

void RfidPn5180_TaskReset(void) {
	stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
}

// Reads a Type-A card like PN5180ISO14443::readCardSerial(), but polls with WUPA (activateTypeA
// kind=1, short frame 0x52) instead of REQA (kind=0, 0x26). Per ISO-14443-3 an ACTIVE PICC - the
// state a card is left in once activateTypeA has selected it - ignores REQA; it answers REQA only
// from IDLE. WUPA is answered from both IDLE and HALT. So polling with WUPA and parking the card in
// HALT (mifareHalt) after each read re-reads a resting card on every poll with no RF field
// power-cycle: first detection (a freshly placed card is IDLE) and re-detection (parked in HALT) are
// both covered. A removed card answers neither, so activateTypeA returns <=0 and the caller sees a
// clean miss. readCardSerial() hardcodes REQA and leaves mifareHalt() unused, so both are driven
// here; the UID validation below matches readCardSerial() so the set of accepted UIDs is unchanged.
static int8_t Rfid_Pn5180ReadCardWupa(PN5180ISO14443 &nfc, uint8_t *buffer) {
	uint8_t response[10] = {0};
	int8_t uidLength = nfc.activateTypeA(response, 1); // 1 = WUPA (wakes IDLE and HALT); 0 would be REQA (IDLE only)
	if (uidLength <= 0) {
		return uidLength;
	}
	if (uidLength < 4) {
		return 0;
	}
	if ((response[0] == 0xFF) && (response[1] == 0xFF)) {
		uidLength = 0;
	}
	if (response[3] == 0x00) {
		uidLength = 0;
	}
	bool validUID = false;
	for (int i = 1; i < uidLength; i++) {
		if ((response[i + 3] != 0x00) && (response[i + 3] != 0xFF)) {
			validUID = true;
			break;
		}
	}
	if (uidLength == 4) {
		if ((response[3] == 0x88)) {
			validUID = false;
		}
	}
	if (uidLength == 7) {
		if ((response[6] == 0x88)) {
			validUID = false;
		}
		if ((response[6] == 0x00) && (response[7] == 0x00) && (response[8] == 0x00) && (response[9] == 0x00)) {
			validUID = false;
		}
		if ((response[6] == 0xFF) && (response[7] == 0xFF) && (response[8] == 0xFF) && (response[9] == 0xFF)) {
			validUID = false;
		}
	}
	if (validUID) {
		for (int i = 0; i < uidLength; i++) {
			buffer[i] = response[i + 3];
		}
		return uidLength;
	}
	return 0;
}

void RfidPn5180_Task(void *parameter) {
	static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
	static PN5180ISO15693 nfc15693(RFID_CS, RFID_BUSY, RFID_RST);
	uint32_t lastTimeDetected14443 = 0;
	uint32_t lastTimeDetected15693 = 0;
	const uint16_t debounceMs = gPrefsRfid.getUShort("pn5180Debounce", 500);
	byte lastValidcardId[cardIdSize] = {0}; // "same card reapplied" is decided by comparing against it
	bool cardAppliedCurrentRun = false;
	bool cardAppliedLastRun = false;
	static byte cardId[cardIdSize], lastCardId[cardIdSize];
	uint8_t uid[10];
	bool showDisablePrivacyNotification = true;
	bool silentHealActive = false; // a full re-init sweep is in progress to un-wedge a still-present card
	uint32_t silentHealStartMs = 0; // millis() when the current heal started (for the recover/give-up logs)
	uint8_t silentHealGiveUpState = 0; // the wedged protocol's read state; the sweep concludes when it is re-reached

	// wait until queues are created
	while (gRfidCardQueue == NULL) {
		Log_Println(waitingForTaskQueues, LOGLEVEL_DEBUG);
		vTaskDelay(50);
	}

	for (;;) {
		vTaskDelay(portTICK_PERIOD_MS * 10u);
		if (Rfid_ConsumeLastTagReset()) {
			// An assignment changed (or the web UI started playback): the card on/near the reader may now
			// mean something else, so it must not be treated as "same card re-applied" any more.
			memset(lastValidcardId, 0, sizeof(lastValidcardId));
		}
		if (Rfid_Pn5180LpcdEnabled() && Rfid_GetLpcdShutdownStatus()) {
			Rfid_EnableLpcd();
			Rfid_SetLpcdShutdownStatus(false); // give feedback that execution is complete
			while (true) {
				vTaskDelay(portTICK_PERIOD_MS * 100u); // there's no way back if shutdown was initiated
			}
		}
		String cardIdString;
		bool cardReceived = false;
		bool sameCardReapplied = false;

		if (RFID_PN5180_STATE_INIT == stateMachine) {
			nfc14443.begin();
			nfc14443.reset();
			// Lower than the library default (500ms): transceiveCommand()'s BUSY-pin wait loops
			// should normally resolve in well under this, so fail/retry faster if the chip is
			// genuinely unresponsive instead of stalling the polling loop for half a second.
			nfc14443.commandTimeout = 100;
			nfc15693.commandTimeout = 100;
			// show PN5180 reader version
			// uint8_t firmwareVersion[2];
			// nfc14443.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
			// Log_Printf(LOGLEVEL_DEBUG, "PN5180 firmware version=%d.%d", firmwareVersion[1], firmwareVersion[0]);

			// activate RF field
			delay(4u);
			Log_Println(rfidScannerReady, LOGLEVEL_DEBUG);

			// 1. check for an ISO-14443 card
		} else if (RFID_PN5180_NFC14443_STATE_RESET == stateMachine) {
			nfc14443.reset();
			// Log_Printf(LOGLEVEL_DEBUG, "%u", uxTaskGetStackHighWaterMark(NULL));
		} else if (RFID_PN5180_NFC14443_STATE_READCARD == stateMachine) {

			if (Rfid_Pn5180ReadCardWupa(nfc14443, uid) >= 4) {
				// Park the just-read card in HALT so the next poll's WUPA can wake and re-read it
				// (see Rfid_Pn5180ReadCardWupa). mifareHalt() is valid from the ACTIVE state that
				// activateTypeA leaves the card in.
				nfc14443.mifareHalt();
				if (silentHealActive) {
					// The re-init sweep re-found the still-present card: end the heal silently. The
					// same-card fast path below (lastCardId was left intact) suppresses re-queue/pause.
					Log_Printf(LOGLEVEL_DEBUG, "PN5180 silent heal recovered ISO-14443 card after %u ms", millis() - silentHealStartMs);
					silentHealActive = false;
				}
				cardReceived = true;
				stateMachine = RFID_PN5180_NFC14443_STATE_ACTIVE;
				lastTimeDetected14443 = millis();
				cardAppliedCurrentRun = true;
			} else if (silentHealActive) {
				// A heal sweep is running. Conclude it only when the ISO-14443 read is the one being
				// healed and it fails after a full re-init sweep -> the card is really gone. Otherwise
				// this failed 14443 read is just a step in an ISO-15693 heal sweep: keep walking
				// (bottom-of-loop state++), do not declare removal or re-enter the fast path.
				if (silentHealGiveUpState == RFID_PN5180_NFC14443_STATE_READCARD) {
					Log_Printf(LOGLEVEL_DEBUG, "PN5180 silent heal gave up after %u ms (ISO-14443 card not re-found)", millis() - silentHealStartMs);
					silentHealActive = false;
					lastTimeDetected14443 = 0;
					cardAppliedCurrentRun = false;
					for (uint8_t i = 0; i < cardIdSize; i++) {
						lastCardId[i] = 0;
					}
				}
			} else {
				// Reset to dummy-value if no card is there
				// Necessary to differentiate between "card is still applied" and "card is re-applied again after removal"
				// lastTimeDetected14443 is used to prevent "new card detection with old card" with single events where no card was detected
				if (!lastTimeDetected14443 || (millis() - lastTimeDetected14443 >= debounceMs)) {
					if (lastTimeDetected14443) {
						// A card was present and reads have failed for the whole debounce window: either
						// the card was removed or the reader has wedged. Run ONE silent re-init sweep
						// before declaring removal to tell them apart. Steer to the ISO-15693 RESET so
						// the full cross-protocol sweep runs before the ISO-14443 read that concludes the
						// heal; continue so RESET executes next. Removal stays suppressed until it ends.
						silentHealActive = true;
						silentHealStartMs = millis();
						silentHealGiveUpState = RFID_PN5180_NFC14443_STATE_READCARD;
						Log_Printf(LOGLEVEL_DEBUG, "PN5180 wedge/removal suspected (%u ms since last good ISO-14443 read): silent re-init", millis() - lastTimeDetected14443);
						stateMachine = RFID_PN5180_NFC15693_STATE_RESET;
						continue;
					}
					lastTimeDetected14443 = 0;
					cardAppliedCurrentRun = false;
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
			// Unlike the ISO14443 path, PN5180ISO15693::issueISO15693Command() (called by
			// getInventory()) never clears the chip's IRQ-status register before checking for a
			// fresh response. Without this, a stale RX-IRQ flag left over from an earlier
			// successful read could make the next poll immediately look like "new data arrived"
			// and return leftover buffer content as a phantom card - even with no tag present.
			nfc15693.clearIRQStatus(0xffffffff);
			// try to read ISO15693 inventory
			ISO15693ErrorCode rc = nfc15693.getInventory(uid);
			if (rc == ISO15693_EC_OK) {
				if (silentHealActive) {
					Log_Printf(LOGLEVEL_DEBUG, "PN5180 silent heal recovered ISO-15693 card after %u ms", millis() - silentHealStartMs);
					silentHealActive = false;
				}
				cardReceived = true;
				stateMachine = RFID_PN5180_NFC15693_STATE_ACTIVE;
				lastTimeDetected15693 = millis();
				cardAppliedCurrentRun = true;
			} else if (silentHealActive) {
				// Heal sweep running (see ISO-14443 path). Conclude only when the ISO-15693 read is
				// the one being healed and it failed after a full sweep -> card really gone. Otherwise
				// this failed 15693 read is just a step in an ISO-14443 heal sweep: keep walking.
				if (silentHealGiveUpState == RFID_PN5180_NFC15693_STATE_GETINVENTORY) {
					Log_Printf(LOGLEVEL_DEBUG, "PN5180 silent heal gave up after %u ms (ISO-15693 card not re-found)", millis() - silentHealStartMs);
					silentHealActive = false;
					lastTimeDetected15693 = 0;
					cardAppliedCurrentRun = false;
					for (uint8_t i = 0; i < cardIdSize; i++) {
						lastCardId[i] = 0;
					}
				}
			} else {
				// lastTimeDetected15693 is used to prevent "new card detection with old card" with single events where no card was detected
				if (!lastTimeDetected15693 || (millis() - lastTimeDetected15693 >= debounceMs)) {
					if (lastTimeDetected15693) {
						// Real wedge or real removal (see ISO-14443 path): try ONE silent re-init sweep
						// before declaring removal. Steer to the ISO-14443 RESET so the full cross-
						// protocol sweep runs first and the ISO-15693 read concludes the heal; continue.
						silentHealActive = true;
						silentHealStartMs = millis();
						silentHealGiveUpState = RFID_PN5180_NFC15693_STATE_GETINVENTORY;
						Log_Printf(LOGLEVEL_DEBUG, "PN5180 wedge/removal suspected (%u ms since last good ISO-15693 read): silent re-init", millis() - lastTimeDetected15693);
						stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
						continue;
					}
					lastTimeDetected15693 = 0;
					cardAppliedCurrentRun = false;
					for (uint8_t i = 0; i < cardIdSize; i++) {
						lastCardId[i] = 0;
					}
				} else {
					stateMachine = RFID_PN5180_NFC15693_STATE_ACTIVE;
				}
			}
		}

		if (gPlayProperties.pauseIfRfidRemoved) {
			// Only pause if there's actually something to pause -- otherwise removing a card after the
			// playlist has already finished naturally queues a PAUSEPLAY that AudioPlayer_Cyclic() then
			// rejects with "no playmode change while idle", which is a confusing error for a normal action.
			if (!cardAppliedCurrentRun && cardAppliedLastRun && !gPlayProperties.pausePlay && !gPlayProperties.playlistFinished && gPlayProperties.playMode != NO_PLAYLIST && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) { // Card removed => pause
				AudioPlayer_SetTrackControl(PAUSEPLAY);
				Log_Println(rfidTagRemoved, LOGLEVEL_NOTICE);
			}
			cardAppliedLastRun = cardAppliedCurrentRun;
		}

		// send card to queue
		if (cardReceived) {
			memcpy(cardId, uid, cardIdSize);

			// check for different card id
			if (memcmp((const void *) cardId, (const void *) lastCardId, sizeof(cardId)) == 0) {
				// Same card as last poll (the common case while a tag sits continuously on the
				// reader): skip the "new card" processing below (logging/queueing) and poll again
				// directly, without a RESET in between - see the comment on the ACTIVE-bypass at
				// the end of this loop for why avoiding the hardware reset here matters.
				if (RFID_PN5180_NFC14443_STATE_ACTIVE == stateMachine) {
					stateMachine = RFID_PN5180_NFC14443_STATE_READCARD;
					continue;
				} else if (RFID_PN5180_NFC15693_STATE_ACTIVE == stateMachine) {
					stateMachine = RFID_PN5180_NFC15693_STATE_GETINVENTORY;
					continue;
				}
			}

			memcpy(lastCardId, cardId, cardIdSize);
			showDisablePrivacyNotification = true;

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
			Log_Printf(LOGLEVEL_NOTICE, "Card type: %s", (RFID_PN5180_NFC14443_STATE_ACTIVE == stateMachine) ? "ISO-14443" : "ISO-15693");

			for (uint8_t i = 0u; i < cardIdSize; i++) {
				char num[4];
				snprintf(num, sizeof(num), "%03d", cardId[i]);
				cardIdString += num;
			}

			if (gPlayProperties.pauseIfRfidRemoved) {
				if (!sameCardReapplied || gPlayProperties.trackFinished || gPlayProperties.playlistFinished) { // Don't allow to send card to queue if it's the same card again if track or playlist is unfnished
					xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0);
				} else {
					// If pause-button was pressed while card was not applied, playback could be active. If so: don't pause when card is reapplied again as the desired functionality would be reversed in this case.
					if (gPlayProperties.pausePlay && System_GetOperationMode() != OPMODE_BLUETOOTH_SINK) {
						AudioPlayer_SetTrackControl(PAUSEPLAY); // ... play/pause instead
						Log_Println(rfidTagReapplied, LOGLEVEL_NOTICE);
					}
				}
				memcpy(lastValidcardId, uid, cardIdSize);
			} else {
				xQueueSend(gRfidCardQueue, cardIdString.c_str(), 0); // If pauseIfRfidRemoved isn't active, every card-apply leads to new playlist-generation
			}
		}

		if (RFID_PN5180_NFC14443_STATE_ACTIVE == stateMachine) {
			// Poll again directly, without a RESET in between: nfc14443.reset() is a full hardware
			// reset of the PN5180 (RST-pin toggle + reboot wait) that collapses the RF field, and
			// readCardSerial() re-establishes the field on every call anyway. Cycling a hard reset
			// on every ~20ms poll while a card is already confirmed present forces marginally-coupled
			// tags (e.g. a case with a larger antenna-to-card gap) to fully re-power from scratch each
			// time, which is a plausible cause of intermittent "card not recognized" blips while a tag
			// sits untouched on the reader. A RESET is still done for the initial scan (state machine
			// starts at RESET) and after the tag is confirmed gone (see debounce below).
			stateMachine = RFID_PN5180_NFC14443_STATE_READCARD;
		} else if (RFID_PN5180_NFC15693_STATE_ACTIVE == stateMachine) {
			// Same reasoning as above, for ISO15693.
			stateMachine = RFID_PN5180_NFC15693_STATE_GETINVENTORY;
		} else {
			stateMachine++;
			if (stateMachine > RFID_PN5180_NFC15693_STATE_GETINVENTORY_PRIVACY) {
				stateMachine = RFID_PN5180_NFC14443_STATE_RESET;
			}
		}
	}
}

void RfidPn5180_Exit(void) {
	Log_Println("shutdown PN5180..", LOGLEVEL_NOTICE);
	if (Rfid_Pn5180LpcdEnabled()) {
		Rfid_SetLpcdShutdownStatus(true);
		while (Rfid_GetLpcdShutdownStatus()) { // Make sure init of LPCD is complete!
			vTaskDelay(portTICK_PERIOD_MS * 10u);
		}
	}
	if (rfidTaskHandle != NULL) {
		vTaskDelete(rfidTaskHandle);
		rfidTaskHandle = NULL;
	}
}

// Handles activation of LPCD (while shutdown is in progress)
void Rfid_EnableLpcd(void) {
	if (!Rfid_Pn5180LpcdEnabled()) {
		return;
	}
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
}

// wake up from LPCD, check card is present in NVS
void RfidPn5180_WakeupCheck(void) {
	if (!Rfid_Pn5180LpcdEnabled()) {
		return;
	}
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
		// Only the first cardIdSize bytes of the UID are used, matching the same ID scheme used for
		// NVS lookups everywhere else (see RfidPn5180_Task below, memcpy(cardId, uid, cardIdSize)).
		// Formatting all 10 raw UID bytes here previously overflowed this cardIdStringSize-sized
		// stack buffer (cardIdStringSize is sized for exactly cardIdSize formatted bytes): once the
		// write position exceeded the buffer, "cardIdStringSize - pos" (both size_t/unsigned) wrapped
		// to a huge value, so snprintf kept writing well past the end of tagId.
		char tagId[cardIdStringSize];
		size_t pos = 0;
		for (size_t i = 0; i < cardIdSize; i++) {
			const int written = snprintf(tagId + pos, cardIdStringSize - pos, "%03d", uid[i]);
			if (written < 0 || static_cast<size_t>(written) >= cardIdStringSize - pos) {
				break;
			}
			pos += written;
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
			// configure wakeup pin for deep-sleep wakeup, use ext1. For a real GPIO only, not PE
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
}
#endif
