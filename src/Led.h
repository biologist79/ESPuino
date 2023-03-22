#pragma once

typedef enum class LedIndicator
{
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
enum class LedAnimationType
{
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

enum class LedPlaylistProgressStates
{
	FillBar = 0,
	Wait,
	EmptyBar,
	EmptyBarToTarget,
	Done
};

struct AnimationReturnType {
    bool animationActive;
    int32_t animationDelay;

	void clear(){
		animationActive = false;
		animationDelay = 0;
	}
	AnimationReturnType() :animationActive(false), animationDelay(0) {}
	AnimationReturnType(bool active, int32_t delay) :animationActive(active), animationDelay(delay) {}
};

void Led_Init(void);
void Led_Exit(void);
void Led_Indicate(LedIndicatorType value);
void Led_SetPause(boolean value);
void Led_ResetToInitialBrightness(void);
void Led_ResetToNightBrightness(void);
uint8_t Led_GetBrightness(void);
void Led_SetBrightness(uint8_t value);