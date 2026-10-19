#pragma once

// Short feedback sounds for power-on, power-off and volume changes.
//
// Every sound is an ordinary audio file on the SD card; a sound is active exactly when its file exists,
// so dropping the files into /ESPuino/sounds/ switches the feature on and deleting them switches it off.
// A master switch ("systemSounds" in NVS, default true) disables everything without touching the card.
// See sounds/README.md for the files and for how to create your own.

// Plays the power-on sound. Blocks for the length of the sound (well under a second); call once at the end
// of setup() when the player is ready.
void SystemSounds_PlayStartup(void);

// Plays the power-off sound. If a story is running it is paused first, which also saves its position the
// same way a button press would. Blocks until the sound is over, so the caller must not cut the power
// before it returns. Call from the main loop task, right before System_PreparePowerDown().
void SystemSounds_PlayShutdown(void);

// Tells the module that the volume changed. Safe to call from any task (the web server and MQTT call
// AudioPlayer_SetVolume() too): it only records the request, SystemSounds_Cyclic() plays it.
// atLimit marks a change that was clamped by the minimum/maximum volume.
void SystemSounds_VolumeChanged(bool atLimit);

// Plays a pending volume sound from the main loop. Only sounds while nothing is playing: with music
// running the changing loudness is feedback enough, and interrupting a track for a click would cost
// position accuracy.
void SystemSounds_Cyclic(void);
