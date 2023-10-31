#pragma once

void Bluetooth_Init(void);
void Bluetooth_Cyclic(void);

// AVRC commands, see https://github.com/pschatzmann/ESP32-A2DP/wiki/Controlling-your-Phone-with-AVRC-Commands

// Support for AVRC Commands starting from ESP32 Release 1.0.6
void Bluetooth_PlayPauseTrack(void);
void Bluetooth_NextTrack(void);
void Bluetooth_PreviousTrack(void);

// Support for AVRC Commands starting from ESP32 Release 2.0.0
void Bluetooth_SetVolume(const int32_t _newVolume, bool reAdjustRotary);

bool Bluetooth_Source_SendAudioData(uint32_t *sample);
bool Bluetooth_Device_Connected();
