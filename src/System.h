#pragma once
#include <Preferences.h>

extern Preferences gPrefsRfid;
extern Preferences gPrefsSettings;
extern TaskHandle_t AudioTaskHandle;

void System_Init(void);
void System_Cyclic(void);
void System_UpdateActivityTimer(void);
void System_RequestSleep(void);
void System_Restart(void);
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
void System_SetOperationMode(uint8_t opMode);
uint8_t System_GetOperationMode(void);
uint8_t System_GetOperationModeFromNvs(void);
void System_esp_print_tasks(void);
void System_ShowWakeUpReason();
