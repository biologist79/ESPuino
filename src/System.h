#pragma once
#include <Preferences.h>

extern Preferences gPrefsRfid;
extern Preferences gPrefsSettings;

void System_Init_Rfid_Prefs(void);
void System_Init(void);
void System_Cyclic(void);
void System_UpdateActivityTimer(void);
void System_RequestSleep(void);
void System_Restart(void);
// True once a restart or sleep/shutdown has been requested but not yet acted
// on (System_Cyclic() only runs the actual restart/sleep from loop() - a
// long-running blocking operation elsewhere, e.g. a MediaHub download, must
// poll this itself and bail out early so the request doesn't just sit there
// until that operation finishes on its own).
bool System_IsRestartOrSleepPending(void);
bool System_SetSleepTimer(uint8_t minutes);
void System_DisableSleepTimer();
bool System_IsSleepTimerEnabled(void);
uint32_t System_GetSleepTimerTimeStamp(void);
bool System_IsSleepPending(void);
uint8_t System_GetSleepTimer(void);
void System_SetLockControls(bool value);
void System_ToggleLockControls(void);
bool System_AreControlsLocked(void);
void System_IndicateError(void);
void System_IndicateOk(void);
bool System_IsWebControlAllowed(void);
void System_SetOperationMode(uint8_t opMode);
uint8_t System_GetOperationMode(void);
uint8_t System_GetOperationModeFromNvs(void);
void System_esp_print_tasks(void);
void System_ShowWakeUpReason();
void System_PauseTasksDuringUpload(bool pause);
