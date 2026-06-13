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
enum class LedAnimationType : uint8_t {
	None = 0,
	Boot,
	Shutdown,
	Error,
	Ok,
	VoltageWarning,
	Volume,
	Battery,
	BatteryMeasurement,
	Webstream,
	Rewind,
	Playlist,
	Progress,
	Idle,
	Busy,
	Speech,
	Pause,
	NoNewAnimation
};

typedef struct {
	bool animationActive : 1;
	int32_t animationDelay;
	bool animationRefresh : 1;
} AnimationReturnType;

#ifdef NEOPIXEL_ENABLE
	#define LED_INITIAL_BRIGHTNESS		 16u
	#define LED_INITIAL_NIGHT_BRIGHTNESS 2u

	#define FASTLED_ESP32_USE_CLOCKLESS_SPI 1

	#include <FastLED.h>

enum class LedPlaylistProgressStates : uint8_t {
	FillBar = 0,
	Wait,
	EmptyBar,
	EmptyBarToTarget,
	Done
};

struct LedSettings {
	uint8_t numIndicatorLeds = NUM_INDICATOR_LEDS;
	uint8_t numControlLeds = NUM_CONTROL_LEDS;
	std::vector<uint32_t> controlLedColors;
	uint8_t numIdleDots = NUM_LEDS_IDLE_DOTS;
	uint32_t idleColor = IDLE_COLOR;
	uint8_t idleAnimation = 0; // 0 = standard idle dots, 1 = cyberpunk "Data Drop"
	bool offsetLedPause = OFFSET_PAUSE_LEDS;
	uint32_t progressColorStart = PROGRESS_COLOR_START;
	uint32_t progressColorEnd = PROGRESS_COLOR_END;
	int16_t atmoHue = ATMO_HUE;
	int16_t atmoSaturation = ATMO_SATURATION;
	uint8_t dimmableStates = DIMMABLE_STATES;
	bool neopixelReverseRotation;
	uint8_t ledOffset;
	bool Led_Pause;

	bool Led_NightMode = false;
	bool Led_AmbientLight = false;
	uint8_t Led_InitialBrightness = LED_INITIAL_BRIGHTNESS;
	uint8_t Led_Brightness = LED_INITIAL_BRIGHTNESS;
	uint8_t Led_NightBrightness = LED_INITIAL_NIGHT_BRIGHTNESS;
	uint8_t Led_AmbientBrightness = LED_INITIAL_BRIGHTNESS;
};

bool Led_GetNightmode();
bool Led_GetAmbientLight();
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
void Led_ToggleNightmode();
void Led_SetAmbientLight(bool enabled);
void Led_ToggleAmbientLight();
