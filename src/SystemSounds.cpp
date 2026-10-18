#include <Arduino.h>
#include "settings.h"

#include "SystemSounds.h"

#include "Audio.h"
#include "AudioPlayer.h"
#include "Log.h"
#include "Port.h"
#include "SdCard.h"
#include "System.h"

#include <atomic>

extern Audio *audio; // owned by AudioPlayer.cpp

namespace {

constexpr const char *pathStartup = "/ESPuino/sounds/startup.wav";
constexpr const char *pathShutdown = "/ESPuino/sounds/shutdown.wav";
constexpr const char *pathVolume = "/ESPuino/sounds/volume.wav";
constexpr const char *pathVolumeLimit = "/ESPuino/sounds/volume-limit.wav";

// Backstop for a file that opens but never reports its end: better a missing sound than a box that
// hangs while starting or shutting down.
constexpr uint32_t maxSoundMs = 3000;
// Volume sounds are rate limited so that turning the knob fast does not turn into a stutter.
constexpr uint32_t volumeSoundIntervalMs = 250;

// Volume changes during boot (restored volume, headphone limit) are not user actions; counting starts
// once the startup sound had its turn.
std::atomic<bool> ready {false};
std::atomic<uint8_t> volumeRequest {0}; // 0 = none, 1 = normal step, 2 = clamped at a limit
uint32_t lastVolumeSoundAt = 0;

bool enabled() {
	return gPrefsSettings.getBool("systemSounds", true);
}

// Plays one file to its end on the audio decoder and returns. Needs an idle decoder (or one whose
// current track may be dropped) and runs the decoder loop itself, so it must be called from the main
// loop task like everything else that touches the audio object.
bool playBlocking(const char *path) {
	if (audio == nullptr || System_GetOperationMode() != OPMODE_NORMAL || !enabled()) {
		return false;
	}
	// A missing file is the normal "feature off" state, so it is not an error and stays out of the log.
	if (!gFSystem.exists(path)) {
		return false;
	}
	if (!audio->connecttoFS(gFSystem, gFSystem.rawPath(path).c_str())) {
		Log_Printf(LOGLEVEL_ERROR, "System sound could not be played: %s", path);
		return false;
	}
	Log_Printf(LOGLEVEL_DEBUG, "Playing system sound: %s", path);
	const uint32_t startedAt = millis();
	while (audio->isRunning() && (millis() - startedAt) < maxSoundMs) {
		audio->loop();
		vTaskDelay(1);
	}
	audio->stopSong();
	// The decoder reports the end of this file like the end of a track; nobody is waiting for it.
	gPlayProperties.trackFinished = false;
	gPlayProperties.currentSpeechActive = false;
	return true;
}

} // namespace

void SystemSounds_PlayStartup(void) {
#ifdef HEADPHONE_ADJUST_ENABLE
	// Right after boot the headphone jack has not been read yet, so the player assumes headphones and keeps
	// the loudspeaker amplifier off until the detection settled (AudioPlayer_Cyclic() switches it on after the
	// debounce time). A sound played earlier would be silent.
	if (enabled() && gFSystem.exists(pathStartup)) {
		while (millis() < headphoneLastDetectionDebounce + 300u) {
			Port_Cyclic();
			AudioPlayer_Cyclic();
			vTaskDelay(5);
		}
	}
#endif
	playBlocking(pathStartup);
	volumeRequest.store(0);
	ready.store(true);
}

void SystemSounds_PlayShutdown(void) {
	if (audio == nullptr || !enabled() || !gFSystem.exists(pathShutdown)) {
		return;
	}
	// Pause a running story first: that is the regular path which also stores the play position.
	const bool storyRunning = gPlayProperties.playMode != NO_PLAYLIST && gPlayProperties.playMode != BUSY && !gPlayProperties.pausePlay && !gPlayProperties.playlistFinished;
	if (storyRunning) {
		AudioPlayer_SetTrackControl(PAUSEPLAY);
		AudioPlayer_Loop();
	}
	playBlocking(pathShutdown);
}

void SystemSounds_VolumeChanged(bool atLimit) {
	if (!ready.load()) {
		return;
	}
	volumeRequest.store(atLimit ? 2 : 1);
}

void SystemSounds_Cyclic(void) {
	const uint8_t request = volumeRequest.load();
	if (request == 0) {
		return;
	}
	// Only when completely idle. Paused or finished playback keeps its file loaded and would lose it.
	if (gPlayProperties.playMode != NO_PLAYLIST || gPlayProperties.announcementActive) {
		volumeRequest.store(0);
		return;
	}
	if ((millis() - lastVolumeSoundAt) < volumeSoundIntervalMs) {
		return; // keep the request, it is played as soon as the interval is over
	}
	volumeRequest.store(0);
	lastVolumeSoundAt = millis();
	if (request == 2 && gFSystem.exists(pathVolumeLimit)) {
		playBlocking(pathVolumeLimit);
	} else {
		playBlocking(pathVolume);
	}
}
