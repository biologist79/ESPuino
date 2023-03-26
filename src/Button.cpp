#include <Arduino.h>
#include "settings.h"
#include "Log.h"
#include "Button.h"
#include "Cmd.h"
#include "Port.h"
#include "System.h"

bool gButtonInitComplete = false;

#define BUTTON_IS_GPIO(n)	(CONFIG_BUTTON_##n && (CONFIG_GPIO_BUTTON_##n <= MAX_GPIO))
#define BUTTON_IS_EXPANDER(n)	(CONFIG_BUTTON_##n && (CONFIG_GPIO_BUTTON_##n >= 100))

t_button gButtons[7];         // next + prev + pplay + rotEnc + button4 + button5 + dummy-button
uint8_t gShutdownButton = 99; // Helper used for Neopixel: stores button-number of shutdown-button
uint16_t gLongPressTime = 0;

#ifdef CONFIG_PORT_EXPANDER
	extern bool Port_AllowReadFromPortExpander;
#endif

static volatile SemaphoreHandle_t Button_TimerSemaphore;

hw_timer_t *Button_Timer = NULL;
static void IRAM_ATTR onTimer();
static void Button_DoButtonActions(void);

void Button_Init() {
	#if (WAKEUP_BUTTON >= 0 && WAKEUP_BUTTON <= MAX_GPIO)
		if (ESP_ERR_INVALID_ARG == esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKEUP_BUTTON, 0)) {
			Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, WAKEUP_BUTTON);
		}
	#endif

	#ifdef CONFIG_NEOPIXEL // Try to find button that is used for shutdown via longpress-action (only necessary for Neopixel)
		#ifdef CONFIG_BUTTON_0
			#if (CONFIG_BUTTON_0_LONG == CMD_SLEEPMODE)
				gShutdownButton = 0;
			#endif
		#endif
		#ifdef CONFIG_BUTTON_1
			#if (CONFIG_BUTTON_1_LONG == CMD_SLEEPMODE)
				gShutdownButton = 1;
			#endif
		#endif
		#ifdef CONFIG_BUTTON_2
			#if (CONFIG_BUTTON_2_LONG == CMD_SLEEPMODE)
				gShutdownButton = 2;
			#endif
		#endif
		#ifdef CONFIG_BUTTON_3
			#if (CONFIG_BUTTON_3_LONG == CMD_SLEEPMODE)
				gShutdownButton = 3;
			#endif
		#endif
		#ifdef CONFIG_BUTTON_4
			#if (CONFIG_BUTTON_4_LONG == CMD_SLEEPMODE)
				gShutdownButton = 4;
			#endif
		#endif
		#ifdef CONFIG_BUTTON_5
			#if (CONFIG_BUTTON_5_LONG == CMD_SLEEPMODE)
				gShutdownButton = 5;
			#endif
		#endif
	#endif

	// Activate internal pullups for all enabled buttons connected to GPIOs
	#if BUTTON_IS_GPIO(0)
		pinMode(CONFIG_GPIO_BUTTON_0, INPUT_PULLUP);
	#endif
	#if BUTTON_IS_GPIO(1)
		pinMode(CONFIG_GPIO_BUTTON_1, INPUT_PULLUP);
	#endif
	#if BUTTON_IS_GPIO(2)
		pinMode(CONFIG_GPIO_BUTTON_2, INPUT_PULLUP);
	#endif
	#if BUTTON_IS_GPIO(3)
		pinMode(CONFIG_GPIO_BUTTON_3, INPUT_PULLUP);
	#endif
	#if BUTTON_IS_GPIO(4)
		pinMode(CONFIG_GPIO_BUTTON_4, INPUT_PULLUP);
	#endif
	#if BUTTON_IS_GPIO(5)
		pinMode(CONFIG_GPIO_BUTTON_5, INPUT_PULLUP);
	#endif

	// Create 1000Hz-HW-Timer (currently only used for buttons)
	Button_TimerSemaphore = xSemaphoreCreateBinary();
	Button_Timer = timerBegin(0, 240, true); // Prescaler: CPU-clock in MHz
	timerAttachInterrupt(Button_Timer, &onTimer, true);
	timerAlarmWrite(Button_Timer, 10000, true); // 100 Hz
	timerAlarmEnable(Button_Timer);
}

// If timer-semaphore is set, read buttons (unless controls are locked)
void Button_Cyclic() {
	if (xSemaphoreTake(Button_TimerSemaphore, 0) == pdTRUE) {
		unsigned long currentTimestamp = millis();
		#ifdef CONFIG_PORT_EXPANDER
			Port_Cyclic();
		#endif

		if (System_AreControlsLocked()) {
			return;
		}

		// Buttons can be mixed between GPIO and port-expander.
		// But at the same time only one of them can be for example CONFIG_GPIO_BUTTON_0
		#ifdef CONFIG_BUTTON_0
				gButtons[0].currentState = Port_Read(CONFIG_GPIO_BUTTON_0);
		#endif
		#ifdef CONFIG_BUTTON_1
				gButtons[1].currentState = Port_Read(CONFIG_GPIO_BUTTON_1);
		#endif
		#ifdef CONFIG_BUTTON_2
				gButtons[2].currentState = Port_Read(CONFIG_GPIO_BUTTON_2);
		#endif
		#ifdef CONFIG_BUTTON_3
				gButtons[3].currentState = Port_Read(CONFIG_GPIO_BUTTON_3);
		#endif
		#ifdef CONFIG_BUTTON_4
				gButtons[4].currentState = Port_Read(CONFIG_GPIO_BUTTON_4);
		#endif
		#ifdef CONFIG_BUTTON_5
				gButtons[5].currentState = Port_Read(CONFIG_GPIO_BUTTON_5);
		#endif

		// Iterate over all buttons in struct-array
		for (uint8_t i = 0; i < sizeof(gButtons) / sizeof(gButtons[0]); i++) {
			if (gButtons[i].currentState != gButtons[i].lastState && currentTimestamp - gButtons[i].lastPressedTimestamp > CONFIG_BUTTON_DEBOUNCE_INTERVAL) {
				if (!gButtons[i].currentState) {
					gButtons[i].isPressed = true;
					gButtons[i].lastPressedTimestamp = currentTimestamp;
					if (!gButtons[i].firstPressedTimestamp) {
						gButtons[i].firstPressedTimestamp = currentTimestamp;
					}
				} else {
					gButtons[i].isReleased = true;
					gButtons[i].lastReleasedTimestamp = currentTimestamp;
					gButtons[i].firstPressedTimestamp = 0;
				}
			}
			gButtons[i].lastState = gButtons[i].currentState;
		}
	}
	gButtonInitComplete = true;
	Button_DoButtonActions();
}

// Do corresponding actions for all buttons
void Button_DoButtonActions(void) {
	if (gButtons[0].isPressed && gButtons[1].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[1].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_01
			Cmd_Action(CONFIG_BUTTON_MULTI_01);
		#endif
	} else if (gButtons[0].isPressed && gButtons[2].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[2].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_02
			Cmd_Action(CONFIG_BUTTON_MULTI_02);
		#endif
	} else if (gButtons[0].isPressed && gButtons[3].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[3].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_03
			Cmd_Action(CONFIG_BUTTON_MULTI_03);
		#endif
	} else if (gButtons[0].isPressed && gButtons[4].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[4].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_04
			Cmd_Action(CONFIG_BUTTON_MULTI_04);
		#endif
	} else if (gButtons[0].isPressed && gButtons[5].isPressed) {
		gButtons[0].isPressed = false;
		gButtons[5].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_05
			Cmd_Action(CONFIG_BUTTON_MULTI_05);
		#endif
	} else if (gButtons[1].isPressed && gButtons[2].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[2].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_12
			Cmd_Action(CONFIG_BUTTON_MULTI_12);
		#endif
	} else if (gButtons[1].isPressed && gButtons[3].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[3].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_13
			Cmd_Action(CONFIG_BUTTON_MULTI_13);
		#endif
	} else if (gButtons[1].isPressed && gButtons[4].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[4].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_14
			Cmd_Action(CONFIG_BUTTON_MULTI_14);
		#endif
	} else if (gButtons[1].isPressed && gButtons[5].isPressed) {
		gButtons[1].isPressed = false;
		gButtons[5].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_15
			Cmd_Action(CONFIG_BUTTON_MULTI_15);
		#endif
	} else if (gButtons[2].isPressed && gButtons[3].isPressed) {
		gButtons[2].isPressed = false;
		gButtons[3].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_23
			Cmd_Action(CONFIG_BUTTON_MULTI_23);
		#endif
	} else if (gButtons[2].isPressed && gButtons[4].isPressed) {
		gButtons[2].isPressed = false;
		gButtons[4].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_24
			Cmd_Action(CONFIG_BUTTON_MULTI_24);
		#endif
	} else if (gButtons[2].isPressed && gButtons[5].isPressed) {
		gButtons[2].isPressed = false;
		gButtons[5].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_25
			Cmd_Action(CONFIG_BUTTON_MULTI_25);
		#endif
	} else if (gButtons[3].isPressed && gButtons[4].isPressed) {
		gButtons[3].isPressed = false;
		gButtons[4].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_34
			Cmd_Action(CONFIG_BUTTON_MULTI_34);
		#endif
	} else if (gButtons[3].isPressed && gButtons[5].isPressed) {
		gButtons[3].isPressed = false;
		gButtons[5].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_35
			Cmd_Action(CONFIG_BUTTON_MULTI_35);
		#endif
	} else if (gButtons[4].isPressed && gButtons[5].isPressed) {
		gButtons[4].isPressed = false;
		gButtons[5].isPressed = false;
		#ifdef CONFIG_BUTTON_MULTI_45
			Cmd_Action(CONFIG_BUTTON_MULTI_45);
		#endif
	} else {
		for (uint8_t i = 0; i <= 5; i++) {
			if (gButtons[i].isPressed) {
				uint8_t Cmd_Short = 0;
				uint8_t Cmd_Long = 0;

				switch (i) { // Long-press-actions
					#ifdef CONFIG_BUTTON_0
					case 0:

						Cmd_Short = CONFIG_BUTTON_0_SHORT;
						Cmd_Long = CONFIG_BUTTON_0_LONG;
						break;
					#endif
					#ifdef CONFIG_BUTTON_1
					case 1:
						Cmd_Short = CONFIG_BUTTON_1_SHORT;
						Cmd_Long = CONFIG_BUTTON_1_LONG;
						break;
					#endif
					#ifdef CONFIG_BUTTON_2
					case 2:
						Cmd_Short = CONFIG_BUTTON_2_SHORT;
						Cmd_Long = CONFIG_BUTTON_2_LONG;
						break;
					#endif
					#ifdef CONFIG_BUTTON_3
					case 3:
						Cmd_Short = CONFIG_BUTTON_3_SHORT;
						Cmd_Long = CONFIG_BUTTON_3_LONG;
						break;
					#endif
					#ifdef CONFIG_BUTTON_4
					case 4:
						Cmd_Short = CONFIG_BUTTON_4_SHORT;
						Cmd_Long = CONFIG_BUTTON_4_LONG;
						break;
					#endif
					#ifdef CONFIG_BUTTON_5
					case 5:
						Cmd_Short = CONFIG_BUTTON_5_SHORT;
						Cmd_Long = CONFIG_BUTTON_5_LONG;
						break;
					#endif
					default:
						break;
				}

				if (gButtons[i].lastReleasedTimestamp > gButtons[i].lastPressedTimestamp) {
					if (gButtons[i].lastReleasedTimestamp - gButtons[i].lastPressedTimestamp < CONFIG_BUTTON_LONGPRESS_INTERVAL) {
						Cmd_Action(Cmd_Short);
					} else {
						// if not volume buttons than start action after button release
						if (Cmd_Long != CMD_VOLUMEUP && Cmd_Long != CMD_VOLUMEDOWN) {
							Cmd_Action(Cmd_Long);
						}
					}

					gButtons[i].isPressed = false;
				} else if (Cmd_Long == CMD_VOLUMEUP || Cmd_Long == CMD_VOLUMEDOWN) {
					unsigned long currentTimestamp = millis();

					// only start action if CONFIG_BUTTON_LONGPRESS_INTERVAL has been reached
					if (currentTimestamp - gButtons[i].lastPressedTimestamp > CONFIG_BUTTON_LONGPRESS_INTERVAL) {

						// calculate remainder
						uint16_t remainder = (currentTimestamp - gButtons[i].lastPressedTimestamp) % CONFIG_BUTTON_LONGPRESS_INTERVAL;

						// trigger action if remainder rolled over
						if (remainder < gLongPressTime) {
							Cmd_Action(Cmd_Long);
						}

						gLongPressTime = remainder;
					}
				}
			}
		}
	}
}

void IRAM_ATTR onTimer() {
	xSemaphoreGiveFromISR(Button_TimerSemaphore, NULL);
}
