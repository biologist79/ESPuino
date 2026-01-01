#include <Arduino.h>
#include "settings.h"

#include "Button.h"

#include "Cmd.h"
#include "Log.h"
#include "Port.h"
#include "System.h"

bool gButtonInitComplete = false;

// Only enable those buttons that are not disabled (99 or >115)
// 0 -> 39: GPIOs
// 100 -> 115: Port-expander
#if (NEXT_BUTTON >= 0 && NEXT_BUTTON <= MAX_GPIO)
	#define BUTTON_0_ENABLE
#elif (NEXT_BUTTON >= 100 && NEXT_BUTTON <= 115)
	#define EXPANDER_0_ENABLE
#endif
#if (PREVIOUS_BUTTON >= 0 && PREVIOUS_BUTTON <= MAX_GPIO)
	#define BUTTON_1_ENABLE
#elif (PREVIOUS_BUTTON >= 100 && PREVIOUS_BUTTON <= 115)
	#define EXPANDER_1_ENABLE
#endif
#if (PAUSEPLAY_BUTTON >= 0 && PAUSEPLAY_BUTTON <= MAX_GPIO)
	#define BUTTON_2_ENABLE
#elif (PAUSEPLAY_BUTTON >= 100 && PAUSEPLAY_BUTTON <= 115)
	#define EXPANDER_2_ENABLE
#endif
#if (ROTARYENCODER_BUTTON >= 0 && ROTARYENCODER_BUTTON <= MAX_GPIO)
	#define BUTTON_3_ENABLE
#elif (ROTARYENCODER_BUTTON >= 100 && ROTARYENCODER_BUTTON <= 115)
	#define EXPANDER_3_ENABLE
#endif
#if (BUTTON_4 >= 0 && BUTTON_4 <= MAX_GPIO)
	#define BUTTON_4_ENABLE
#elif (BUTTON_4 >= 100 && BUTTON_4 <= 115)
	#define EXPANDER_4_ENABLE
#endif
#if (BUTTON_5 >= 0 && BUTTON_5 <= MAX_GPIO)
	#define BUTTON_5_ENABLE
#elif (BUTTON_5 >= 100 && BUTTON_5 <= 115)
	#define EXPANDER_5_ENABLE
#endif

// Allocate gButtons in PSRAM if available
EXT_RAM_BSS_ATTR t_button gButtons[7]; // next + prev + pplay + rotEnc + button4 + button5 + dummy-button
uint8_t gShutdownButton = 99; // Helper used for Neopixel: stores button-number of shutdown-button
uint16_t gLongPressTime = 0;

#ifdef PORT_EXPANDER_ENABLE
extern bool Port_AllowReadFromPortExpander;
#endif

static volatile SemaphoreHandle_t Button_TimerSemaphore;

hw_timer_t *Button_Timer = NULL;
#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 3))
static void IRAM_ATTR onTimer();
#else
static void onTimer();
#endif
static void Button_DoButtonActions(void);

void Button_Init() {
#if (WAKEUP_BUTTON >= 0 && WAKEUP_BUTTON <= MAX_GPIO)
	if (ESP_ERR_INVALID_ARG == esp_sleep_enable_ext0_wakeup((gpio_num_t) WAKEUP_BUTTON, 0)) {
		Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, WAKEUP_BUTTON);
	}
#endif

#ifdef NEOPIXEL_ENABLE // Try to find button that is used for shutdown via longpress-action (only necessary for Neopixel)
	#if (defined(BUTTON_0_ENABLE) || defined(EXPANDER_0_ENABLE)) && (BUTTON_0_LONG == CMD_SLEEPMODE)
		gShutdownButton = 0;
	#elif (defined(BUTTON_1_ENABLE) || defined(EXPANDER_1_ENABLE)) && (BUTTON_1_LONG == CMD_SLEEPMODE)
		gShutdownButton = 1;
	#elif (defined(BUTTON_2_ENABLE) || defined(EXPANDER_2_ENABLE)) && (BUTTON_2_LONG == CMD_SLEEPMODE)
		gShutdownButton = 2;
	#elif (defined(BUTTON_3_ENABLE) || defined(EXPANDER_3_ENABLE)) && (BUTTON_3_LONG == CMD_SLEEPMODE)
		gShutdownButton = 3;
	#elif (defined(BUTTON_4_ENABLE) || defined(EXPANDER_4_ENABLE)) && (BUTTON_4_LONG == CMD_SLEEPMODE)
		gShutdownButton = 4;
	#elif (defined(BUTTON_5_ENABLE) || defined(EXPANDER_5_ENABLE)) && (BUTTON_5_LONG == CMD_SLEEPMODE)
		gShutdownButton = 5;
	#endif
#endif

// Activate internal pullups for all enabled buttons connected to GPIOs
#ifdef BUTTON_0_ENABLE
	if (BUTTON_0_ACTIVE_STATE) {
		pinMode(NEXT_BUTTON, INPUT);
	} else {
		pinMode(NEXT_BUTTON, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_1_ENABLE
	if (BUTTON_1_ACTIVE_STATE) {
		pinMode(PREVIOUS_BUTTON, INPUT);
	} else {
		pinMode(PREVIOUS_BUTTON, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_2_ENABLE
	if (BUTTON_2_ACTIVE_STATE) {
		pinMode(PAUSEPLAY_BUTTON, INPUT);
	} else {
		pinMode(PAUSEPLAY_BUTTON, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_3_ENABLE
	if (BUTTON_3_ACTIVE_STATE) {
		pinMode(ROTARYENCODER_BUTTON, INPUT);
	} else {
		pinMode(ROTARYENCODER_BUTTON, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_4_ENABLE
	if (BUTTON_4_ACTIVE_STATE) {
		pinMode(BUTTON_4, INPUT);
	} else {
		pinMode(BUTTON_4, INPUT_PULLUP);
	}
#endif
#ifdef BUTTON_5_ENABLE
	if (BUTTON_5_ACTIVE_STATE) {
		pinMode(BUTTON_5, INPUT);
	} else {
		pinMode(BUTTON_5, INPUT_PULLUP);
	}
#endif

	// Create 1000Hz-HW-Timer (currently only used for buttons)
	Button_TimerSemaphore = xSemaphoreCreateBinary();
#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3))
	Button_Timer = timerBegin(1000000); // Prescaler: CPU-clock in MHz
	timerAttachInterrupt(Button_Timer, &onTimer);
	timerAlarm(Button_Timer, 10000, true, 0); // 100 Hz
#else
	Button_Timer = timerBegin(0, 240, true); // Prescaler: CPU-clock in MHz
	timerAttachInterrupt(Button_Timer, &onTimer, true);
	timerAlarmWrite(Button_Timer, 10000, true); // 100 Hz
	timerAlarmEnable(Button_Timer);
#endif
}

// Read current state of all enabled buttons
static void Button_ReadAllStates(void) {
#if defined(BUTTON_0_ENABLE) || defined(EXPANDER_0_ENABLE)
	gButtons[0].currentState = Port_Read(NEXT_BUTTON) ^ BUTTON_0_ACTIVE_STATE;
#endif
#if defined(BUTTON_1_ENABLE) || defined(EXPANDER_1_ENABLE)
	gButtons[1].currentState = Port_Read(PREVIOUS_BUTTON) ^ BUTTON_1_ACTIVE_STATE;
#endif
#if defined(BUTTON_2_ENABLE) || defined(EXPANDER_2_ENABLE)
	gButtons[2].currentState = Port_Read(PAUSEPLAY_BUTTON) ^ BUTTON_2_ACTIVE_STATE;
#endif
#if defined(BUTTON_3_ENABLE) || defined(EXPANDER_3_ENABLE)
	gButtons[3].currentState = Port_Read(ROTARYENCODER_BUTTON) ^ BUTTON_3_ACTIVE_STATE;
#endif
#if defined(BUTTON_4_ENABLE) || defined(EXPANDER_4_ENABLE)
	gButtons[4].currentState = Port_Read(BUTTON_4) ^ BUTTON_4_ACTIVE_STATE;
#endif
#if defined(BUTTON_5_ENABLE) || defined(EXPANDER_5_ENABLE)
	gButtons[5].currentState = Port_Read(BUTTON_5) ^ BUTTON_5_ACTIVE_STATE;
#endif
}

// Update press/release state for a single button with debouncing
static void Button_UpdateState(t_button &btn, unsigned long currentTimestamp) {
	bool const stateChanged = btn.currentState != btn.lastState;
	bool const debounceElapsed = currentTimestamp - btn.lastPressedTimestamp > buttonDebounceInterval;

	if (stateChanged && debounceElapsed) {
		bool const buttonPressed = !btn.currentState;
		if (buttonPressed) {
			btn.isPressed = true;
			btn.lastPressedTimestamp = currentTimestamp;
			if (!btn.firstPressedTimestamp) {
				btn.firstPressedTimestamp = currentTimestamp;
			}
		} else {
			btn.isReleased = true;
			btn.lastReleasedTimestamp = currentTimestamp;
			btn.firstPressedTimestamp = 0;
		}
	}
	btn.lastState = btn.currentState;
}

// If timer-semaphore is set, read buttons (unless controls are locked)
void Button_Cyclic() {
	if (xSemaphoreTake(Button_TimerSemaphore, 0) != pdTRUE) {
		return;
	}

	unsigned long currentTimestamp = millis();

#ifdef PORT_EXPANDER_ENABLE
	Port_Cyclic();
#endif

	if (System_AreControlsLocked()) {
		return;
	}

	Button_ReadAllStates();

	for (uint8_t i = 0; i < sizeof(gButtons) / sizeof(gButtons[0]); i++) {
		Button_UpdateState(gButtons[i], currentTimestamp);
	}

	gButtonInitComplete = true;
	Button_DoButtonActions();
}

// Multi-button combination configuration: {btn1, btn2, prefsKey, defaultCmd}
static const struct {
	uint8_t btn1;
	uint8_t btn2;
	const char *prefsKey;
	uint8_t defaultCmd;
} multiButtonCombos[] = {
	{0, 1, "btnMulti01", BUTTON_MULTI_01},
	{0, 2, "btnMulti02", BUTTON_MULTI_02},
	{0, 3, "btnMulti03", BUTTON_MULTI_03},
	{0, 4, "btnMulti04", BUTTON_MULTI_04},
	{0, 5, "btnMulti05", BUTTON_MULTI_05},
	{1, 2, "btnMulti12", BUTTON_MULTI_12},
	{1, 3, "btnMulti13", BUTTON_MULTI_13},
	{1, 4, "btnMulti14", BUTTON_MULTI_14},
	{1, 5, "btnMulti15", BUTTON_MULTI_15},
	{2, 3, "btnMulti23", BUTTON_MULTI_23},
	{2, 4, "btnMulti24", BUTTON_MULTI_24},
	{2, 5, "btnMulti25", BUTTON_MULTI_25},
	{3, 4, "btnMulti34", BUTTON_MULTI_34},
	{3, 5, "btnMulti35", BUTTON_MULTI_35},
	{4, 5, "btnMulti45", BUTTON_MULTI_45},
};

// Check for multi-button combinations and execute corresponding action
static bool Button_HandleMultiButtonPress(void) {
	for (const auto &combo : multiButtonCombos) {
		if (gButtons[combo.btn1].isPressed && gButtons[combo.btn2].isPressed) {
			gButtons[combo.btn1].isPressed = false;
			gButtons[combo.btn2].isPressed = false;
			Cmd_Action(gPrefsSettings.getUChar(combo.prefsKey, combo.defaultCmd));
			return true;
		}
	}
	return false;
}

// Button command configuration: {prefsKeyShort, prefsKeyLong, defaultShort, defaultLong}
static const struct {
	const char *prefsKeyShort;
	const char *prefsKeyLong;
	uint8_t defaultShort;
	uint8_t defaultLong;
} buttonCmdConfig[] = {
	{"btnShort0", "btnLong0", BUTTON_0_SHORT, BUTTON_0_LONG},
	{"btnShort1", "btnLong1", BUTTON_1_SHORT, BUTTON_1_LONG},
	{"btnShort2", "btnLong2", BUTTON_2_SHORT, BUTTON_2_LONG},
	{"btnShort3", "btnLong3", BUTTON_3_SHORT, BUTTON_3_LONG},
	{"btnShort4", "btnLong4", BUTTON_4_SHORT, BUTTON_4_LONG},
	{"btnShort5", "btnLong5", BUTTON_5_SHORT, BUTTON_5_LONG},
};

// Handle a single button's short/long press action
static void Button_HandleSinglePress(uint8_t i, unsigned long currentTimestamp) {
	uint8_t Cmd_Short = gPrefsSettings.getUChar(buttonCmdConfig[i].prefsKeyShort, buttonCmdConfig[i].defaultShort);
	uint8_t Cmd_Long = gPrefsSettings.getUChar(buttonCmdConfig[i].prefsKeyLong, buttonCmdConfig[i].defaultLong);
	unsigned long const pressDuration = currentTimestamp - gButtons[i].lastPressedTimestamp;
	bool const wasReleased = gButtons[i].lastReleasedTimestamp > gButtons[i].lastPressedTimestamp;

	// Handle button release (short or long press completed)
	if (wasReleased) {
		unsigned long const releaseDuration = gButtons[i].lastReleasedTimestamp - gButtons[i].lastPressedTimestamp;
		bool const wasShortPress = releaseDuration < intervalToLongPress;

		if (wasShortPress) {
			Cmd_Action(Cmd_Short);
		} else if (Cmd_Long == CMD_SLEEPMODE) {
			// Sleep-mode only triggers on release to prevent immediate wake-up
			Cmd_Action(Cmd_Long);
		}

		gButtons[i].isPressed = false;
		return;
	}

	// Handle volume buttons with repeat functionality
	if (Cmd_Long == CMD_VOLUMEUP || Cmd_Long == CMD_VOLUMEDOWN) {
		if (pressDuration <= intervalToLongPress) {
			return;
		}
		uint16_t remainder = pressDuration % intervalToLongPress;
		if (remainder < gLongPressTime) {
			Cmd_Action(Cmd_Long);
		}
		gLongPressTime = remainder;
		return;
	}

	// Handle other long-press actions (except sleep mode which triggers on release)
	if (Cmd_Long != CMD_SLEEPMODE && pressDuration > intervalToLongPress) {
		gButtons[i].isPressed = false;
		Cmd_Action(Cmd_Long);
	}
}

// Do corresponding actions for all buttons
void Button_DoButtonActions(void) {
	if (Button_HandleMultiButtonPress()) {
		return;
	}

	unsigned long currentTimestamp = millis();
	for (uint8_t i = 0; i < 7; i++) {
		if (gButtons[i].isPressed) {
			Button_HandleSinglePress(i, currentTimestamp);
		}
	}
}

#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 3))
void IRAM_ATTR onTimer() {
#else
void onTimer() {
#endif
	xSemaphoreGiveFromISR(Button_TimerSemaphore, NULL);
}
