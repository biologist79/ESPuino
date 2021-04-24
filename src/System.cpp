#include <Arduino.h>
#include "settings.h"
#include "System.h"
#include "AudioPlayer.h"
#include "Rfid.h"
#include "Led.h"
#include "Log.h"
#include "Mqtt.h"
#include "SdCard.h"

constexpr const char prefsRfidNamespace[] PROGMEM = "rfidTags";     // Namespace used to save IDs of rfid-tags
constexpr const char prefsSettingsNamespace[] PROGMEM = "settings"; // Namespace used for generic settings

Preferences gPrefsRfid;
Preferences gPrefsSettings;

unsigned long System_LastTimeActiveTimestamp = 0u;  // Timestamp of last user-interaction
unsigned long System_SleepTimerStartTimestamp = 0u; // Flag if sleep-timer is active
bool System_GoToSleep = false;                      // Flag for turning uC immediately into deepsleep
bool System_Sleeping = false;                       // Flag for turning into deepsleep is in progress
bool System_LockControls = false;                   // Flag if buttons and rotary encoder is locked
uint8_t System_MaxInactivityTime = 10u;             // Time in minutes, after uC is put to deep sleep because of inactivity (and modified later via GUI)
uint8_t System_SleepTimer = 30u;                    // Sleep timer in minutes that can be optionally used (and modified later via MQTT or RFID)

// Operation Mode
volatile uint8_t System_OperationMode;

void System_SleepHandler(void);
void System_DeepSleepManager(void);

void System_Init(void)
{
    srand(esp_random());
    pinMode(POWER, OUTPUT);
    digitalWrite(POWER, HIGH);

    gPrefsRfid.begin((char *)FPSTR(prefsRfidNamespace));
    gPrefsSettings.begin((char *)FPSTR(prefsSettingsNamespace));

    // Get maximum inactivity-time from NVS
    uint32_t nvsMInactivityTime = gPrefsSettings.getUInt("mInactiviyT", 0);
    if (nvsMInactivityTime)
    {
        System_MaxInactivityTime = nvsMInactivityTime;
        snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *)FPSTR(restoredMaxInactivityFromNvs), nvsMInactivityTime);
        Log_Println(Log_Buffer, LOGLEVEL_INFO);
    }
    else
    {
        gPrefsSettings.putUInt("mInactiviyT", System_MaxInactivityTime);
        Log_Println((char *)FPSTR(wroteMaxInactivityToNvs), LOGLEVEL_ERROR);
    }

    System_OperationMode = gPrefsSettings.getUChar("operationMode", OPMODE_NORMAL);
}

void System_Cyclic(void)
{
    System_SleepHandler();
    System_DeepSleepManager();
}

void System_UpdateActivityTimer(void)
{
    System_LastTimeActiveTimestamp = millis();
}

void System_RequestSleep(void)
{
    System_GoToSleep = true;
}

bool System_IsSleepRequested(void)
{
    return System_GoToSleep;
}

bool System_SetSleepTimer(uint8_t minutes)
{
    bool sleepTimerEnabled = false;

    if (System_SleepTimerStartTimestamp && (System_SleepTimer == minutes))
    {
        System_SleepTimerStartTimestamp = 0u;
        Led_ResetToInitialBrightness();
        Log_Println((char *)FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
        //publishMqtt((char *)FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
    }
    else
    {
        System_SleepTimer = minutes;
        System_SleepTimerStartTimestamp = millis();
        sleepTimerEnabled = true;

        Led_ResetToNightBrightness();
        Log_Println((char *)FPSTR(modificatorSleepTimer60), LOGLEVEL_NOTICE);
        //publishMqtt((char *)FPSTR(topicSleepTimerState), System_SleepTimer, false);
        //publishMqtt((char *)FPSTR(topicLedBrightnessState), Led_GetBrightness(), false);
    }

    return sleepTimerEnabled;
}

void System_DisableSleepTimer(void)
{
    System_SleepTimerStartTimestamp = 0u;
}

bool System_IsSleepTimerEnabled(void)
{
    return (System_SleepTimerStartTimestamp > 0u);
}

uint32_t System_GetSleepTimerTimeStamp(void)
{
    return System_SleepTimerStartTimestamp;
}

bool System_IsSleepPending(void)
{
    return System_Sleeping;
}

uint8_t System_GetSleepTimer(void)
{
    return System_SleepTimer;
}

void System_SetLockControls(bool value)
{
    System_LockControls = value;
}

void System_ToggleLockControls(void)
{
    System_LockControls = !System_LockControls;

    if (System_LockControls)
    {
        Log_Println((char *)FPSTR(modificatorAllButtonsLocked), LOGLEVEL_NOTICE);
        //publishMqtt((char *)FPSTR(topicLockControlsState), "ON", false);
        System_IndicateOk();
    }
    else
    {
        Log_Println((char *)FPSTR(modificatorAllButtonsUnlocked), LOGLEVEL_NOTICE);
        //publishMqtt((char *)FPSTR(topicLockControlsState), "OFF", false);
    }
}

bool System_AreControlsLocked(void)
{
    return System_LockControls;
}

void System_IndicateError(void)
{
    Led_Indicate(LedIndicatorType::Error);
}
void System_IndicateOk(void)
{
    Led_Indicate(LedIndicatorType::Ok);
}

// Writes to NVS, if bluetooth or "normal" mode is desired
void System_SetOperationMode(uint8_t opMode)
{
    uint8_t currentOperationMode = gPrefsSettings.getUChar("operationMode", OPMODE_NORMAL);
    if (currentOperationMode != opMode)
    {
        if (gPrefsSettings.putUChar("operationMode", opMode))
        {
            ESP.restart();
        }
    }
}

uint8_t System_GetOperationMode(void)
{
    return System_OperationMode;
}

// Reads from NVS, if bluetooth or "normal" mode is desired
uint8_t System_GetOperationModeFromNvs(void)
{
    return gPrefsSettings.getUChar("operationMode", OPMODE_NORMAL);
}

// Sets deep-sleep-flag if max. inactivity-time is reached
void System_SleepHandler(void)
{
    unsigned long m = millis();
    if (m >= System_LastTimeActiveTimestamp && (m - System_LastTimeActiveTimestamp >= (System_MaxInactivityTime * 1000u * 60u)))
    {
        Log_Println((char *)FPSTR(goToSleepDueToIdle), LOGLEVEL_INFO);
        System_RequestSleep();
    }
    else if (System_SleepTimerStartTimestamp > 00)
    {
        if (m - System_SleepTimerStartTimestamp >= (System_SleepTimer * 1000u * 60u))
        {
            Log_Println((char *)FPSTR(goToSleepDueToTimer), LOGLEVEL_INFO);
            System_RequestSleep();
        }
    }
}

// Puts uC to deep-sleep if flag is set
void System_DeepSleepManager(void)
{
    if (System_GoToSleep)
    {
        if (System_Sleeping)
        {
            return;
        }

        System_Sleeping = true;
        Log_Println((char *)FPSTR(goToSleepNow), LOGLEVEL_NOTICE);

        Mqtt_Exit();
        Led_Exit();

#ifdef USE_LAST_VOLUME_AFTER_REBOOT
        gPrefsSettings.putUInt("previousVolume", AudioPlayer_GetCurrentVolume());
#endif
        SdCard_Exit();

        Serial.flush();
        // switch off power
        digitalWrite(POWER, LOW);
        delay(200);
        Rfid_Exit();
        Serial.println(F("deep-sleep, good night......."));
        esp_deep_sleep_start();
    }
}
