#pragma once

typedef enum class LedIndicator
{
	BootComplete = 0,
	Error,
	Ok,
	PlaylistProgress,
	Rewind,
	Voltage,
	VoltageWarning
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

void Led_Init(void);
void Led_Exit(void);
void Led_Indicate(LedIndicatorType value);
void Led_SetPause(boolean value);
void Led_ResetToInitialBrightness(void);
void Led_ResetToNightBrightness(void);
uint8_t Led_GetBrightness(void);
void Led_SetBrightness(uint8_t value);
uint8_t AudioPlayer_GetInitVolume(void);
void AudioPlayer_SetInitVolume(uint8_t value);
uint8_t AudioPlayer_GetInitVolume(void);
void AudioPlayer_SetInitVolume(uint8_t value);
