#pragma once

#include "Playlist.h"

#include <optional>

#ifndef AUDIOPLAYER_PLAYLIST_SORT_MODE_DEFAULT
	#define AUDIOPLAYER_PLAYLIST_SORT_MODE_DEFAULT playlistSortMode::STRNATCASECMP
#endif

enum class playlistSortMode : uint8_t {
	STRCMP = 1,
	STRNATCMP = 2,
	STRNATCASECMP = 3,
};

typedef struct { // Bit field
	uint8_t playMode : 4; // playMode
	Playlist *playlist; // playlist
	char title[255]; // current title
	bool repeatCurrentTrack		: 1; // If current track should be looped
	bool repeatPlaylist			: 1; // If whole playlist should be looped
	uint16_t currentTrackNumber : 9; // Current tracknumber
	unsigned long startAtFilePos; // Offset to start play (in bytes)
	double currentRelPos; // Current relative playPosition (in %)
	bool sleepAfterCurrentTrack : 1; // If uC should go to sleep after current track
	bool sleepAfterPlaylist		: 1; // If uC should go to sleep after whole playlist
	bool sleepAfter5Tracks		: 1; // If uC should go to sleep after 5 tracks
	bool saveLastPlayPosition	: 1; // If playposition/current track should be saved (for AUDIOBOOK)
	char playRfidTag[13]; // ID of RFID-tag that started playlist
	bool pausePlay				 : 1; // If pause is active
	bool trackFinished			 : 1; // If current track is finished
	bool playlistFinished		 : 1; // If whole playlist is finished
	uint8_t playUntilTrackNumber : 6; // Number of tracks to play after which uC goes to sleep
	uint8_t seekmode			 : 2; // If seekmode is active and if yes: forward or backwards?
	bool newPlayMono			 : 1; // true if mono; false if stereo (helper)
	bool currentPlayMono		 : 1; // true if mono; false if stereo
	bool isWebstream			 : 1; // Indicates if track currenty played is a webstream
	uint8_t tellMode			 : 2; // Tell mode for text to speech announcments
	bool currentSpeechActive	 : 1; // If speech-play is active
	bool lastSpeechActive		 : 1; // If speech-play was active
	bool SavePlayPosRfidChange	 : 1; // Save last play-position
	bool pauseOnMinVolume		 : 1; // When playback is active and volume is changed to zero, playback is paused automatically.
	int8_t gainLowPass = 0; // Low Pass for EQ Control
	int8_t gainBandPass = 0; // Band Pass for EQ Control
	int8_t gainHighPass = 0; // High Pass for EQ Control
	size_t coverFilePos; // current cover file position
	size_t coverFileSize; // current cover file size
	size_t audioFileSize; // file size of current audio file
} playProps;

extern playProps gPlayProperties;

void AudioPlayer_Init(void);
void AudioPlayer_Exit(void);
void AudioPlayer_Cyclic(void);
uint8_t AudioPlayer_GetRepeatMode(void);
void AudioPlayer_VolumeToQueueSender(const int32_t _newVolume, bool reAdjustRotary);
void AudioPlayer_EqualizerToQueueSender(const int8_t gainLowPass, const int8_t gainBandPass, const int8_t gainHighPass);
void AudioPlayer_TrackQueueDispatcher(const char *_itemToPlay, const uint32_t _lastPlayPos, const uint32_t _playMode, const uint16_t _trackLastPlayed);
void AudioPlayer_TrackControlToQueueSender(const uint8_t trackCommand);
void AudioPlayer_PauseOnMinVolume(const uint8_t oldVolume, const uint8_t newVolume);

playlistSortMode AudioPlayer_GetPlaylistSortMode(void);
bool AudioPlayer_SetPlaylistSortMode(playlistSortMode value);
bool AudioPlayer_SetPlaylistSortMode(uint8_t value);
uint8_t AudioPlayer_GetCurrentVolume(void);
void AudioPlayer_SetCurrentVolume(uint8_t value);
uint8_t AudioPlayer_GetMaxVolume(void);
void AudioPlayer_SetMaxVolume(uint8_t value);
uint8_t AudioPlayer_GetMaxVolumeSpeaker(void);
void AudioPlayer_SetMaxVolumeSpeaker(uint8_t value);
uint8_t AudioPlayer_GetMinVolume(void);
void AudioPlayer_SetMinVolume(uint8_t value);
uint8_t AudioPlayer_GetInitVolume(void);
void AudioPlayer_SetInitVolume(uint8_t value);
void AudioPlayer_SetupVolumeAndAmps(void);
bool Audio_Detect_Mode_HP(bool _state);
void Audio_setTitle(const char *format, ...);
time_t AudioPlayer_GetPlayTimeSinceStart(void);
time_t AudioPlayer_GetPlayTimeAllTime(void);
uint32_t AudioPlayer_GetCurrentTime(void);
uint32_t AudioPlayer_GetFileDuration(void);
String AudioPlayer_GetStationLogoUrl(void);
