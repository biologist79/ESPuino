#pragma once

typedef enum class LedIndicator {
	BootComplete = 0,
	Error,
	Ok,
	PlaylistProgress,
	Rewind,
	Voltage,
	VoltageWarning,
	VolumeChange
} LedIndicatorType;

// ordered by priority
enum class LedAnimationType {
	Boot = 0,
	Shutdown,
	Error,
	Ok,
	VoltageWarning,
	Volume,
	BatteryMeasurement,
	Rewind,
	Playlist,
	Speech,
	Pause,
	Progress,
	Webstream,
	Idle,
	Busy,
	NoNewAnimation
};

enum class LedPlaylistProgressStates {
	FillBar = 0,
	Wait,
	EmptyBar,
	EmptyBarToTarget,
	Done
};

struct AnimationReturnType {
	bool animationActive;
	int32_t animationDelay;
	bool animationRefresh;

	void clear() {
		animationActive = false;
		animationDelay = 0;
	}
	AnimationReturnType()
		: animationActive(false)
		, animationDelay(0)
		, animationRefresh(false) { }
	AnimationReturnType(bool active, int32_t delay, bool refresh = false)
		: animationActive(active)
		, animationDelay(delay)
		, animationRefresh(refresh) { }
};

#ifdef NEOPIXEL_ENABLE
	#define LED_INITIAL_BRIGHTNESS		 16u
	#define LED_INITIAL_NIGHT_BRIGHTNESS 2u

	#define FASTLED_WS2812_T1 350 // original 250 - changed
	#define FASTLED_WS2812_T2 700 // original 625 - changed
	#define FASTLED_WS2812_T3 150 // original 375 - changed

	#include <FastLED.h>

struct LedSettings {
	uint8_t numIndicatorLeds = NUM_INDICATOR_LEDS;
	uint8_t numControlLeds = NUM_CONTROL_LEDS;
	std::vector<uint32_t> controlLedColors;
	uint8_t numIdleDots = NUM_LEDS_IDLE_DOTS;
	bool offsetLedPause = OFFSET_PAUSE_LEDS;
	int16_t progressHueStart = PROGRESS_HUE_START;
	int16_t progressHueEnd = PROGRESS_HUE_END;
	uint8_t dimmableStates = DIMMABLE_STATES;
	bool neopixelReverseRotation;
	uint8_t ledOffset;
	bool Led_Pause = false; // Used to pause Neopixel-signalisation (while NVS-writes as this leads to exceptions; don't know why)
	bool Led_NightMode = false;
	uint8_t Led_InitialBrightness = LED_INITIAL_BRIGHTNESS;
	uint8_t Led_Brightness = LED_INITIAL_BRIGHTNESS;
	uint8_t Led_NightBrightness = LED_INITIAL_NIGHT_BRIGHTNESS;
};
#endif

void Led_Init(void);
void Led_Exit(void);
void Led_Indicate(LedIndicatorType value);
void Led_SetPause(boolean value);
void Led_ResetToInitialBrightness(void);
void Led_ResetToNightBrightness(void);
uint8_t Led_GetBrightness(void);
void Led_SetBrightness(uint8_t value);
void Led_TaskPause(void);
void Led_TaskResume(void);

void Led_SetNightmode(bool enabled);
bool Led_GetNightmode();
void Led_ToggleNightmode();
