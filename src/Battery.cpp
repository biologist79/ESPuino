#include <Arduino.h>
#include "settings.h"
#include "Log.h"
#include "Battery.h"
#include "Mqtt.h"
#include "Led.h"
#include "System.h"

constexpr uint16_t maxAnalogValue = 4095u; // Highest value given by analogRead(); don't change!

float warningLowVoltage = s_warningLowVoltage;
uint8_t voltageCheckInterval = s_voltageCheckInterval;
float voltageIndicatorLow = s_voltageIndicatorLow;
float voltageIndicatorHigh = s_voltageIndicatorHigh;

void Battery_Init()
{
#ifdef MEASURE_BATTERY_VOLTAGE
    // Get voltages from NVS for Neopixel
    float vLowIndicator = gPrefsSettings.getFloat("vIndicatorLow", 999.99);
    if (vLowIndicator <= 999)
    {
        voltageIndicatorLow = vLowIndicator;
        snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(voltageIndicatorLowFromNVS), vLowIndicator);
        Log_Println(Log_Buffer, LOGLEVEL_INFO);
    }
    else
    { // preseed if not set
        gPrefsSettings.putFloat("vIndicatorLow", voltageIndicatorLow);
    }

    float vHighIndicator = gPrefsSettings.getFloat("vIndicatorHigh", 999.99);
    if (vHighIndicator <= 999)
    {
        voltageIndicatorHigh = vHighIndicator;
        snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(voltageIndicatorHighFromNVS), vHighIndicator);
        Log_Println(Log_Buffer, LOGLEVEL_INFO);
    }
    else
    {
        gPrefsSettings.putFloat("vIndicatorHigh", voltageIndicatorHigh);
    }

    float vLowWarning = gPrefsSettings.getFloat("wLowVoltage", 999.99);
    if (vLowWarning <= 999)
    {
        warningLowVoltage = vLowWarning;
        snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(warningLowVoltageFromNVS), vLowWarning);
        Log_Println(Log_Buffer, LOGLEVEL_INFO);
    }
    else
    {
        gPrefsSettings.putFloat("wLowVoltage", warningLowVoltage);
    }

    uint32_t vInterval = gPrefsSettings.getUInt("vCheckIntv", 17777);
    if (vInterval != 17777)
    {
        voltageCheckInterval = vInterval;
        snprintf(Log_Buffer, Log_BufferLength, "%s: %u Minuten", (char *)FPSTR(voltageCheckIntervalFromNVS), vInterval);
        Log_Println(Log_Buffer, LOGLEVEL_INFO);
    }
    else
    {
        gPrefsSettings.putUInt("vCheckIntv", voltageCheckInterval);
    }
#endif
}

// The average of several analog reads will be taken to reduce the noise (Note: One analog read takes ~10µs)
float Battery_GetVoltage(void)
{
#ifdef MEASURE_BATTERY_VOLTAGE
    float factor = 1 / ((float)rdiv2 / (rdiv2 + rdiv1));
    float averagedAnalogValue = 0;
    uint8_t i;
    for (i = 0; i <= 19; i++)
    {
        averagedAnalogValue += (float)analogRead(VOLTAGE_READ_PIN);
    }
    averagedAnalogValue /= 20.0;
    return (averagedAnalogValue / maxAnalogValue) * referenceVoltage * factor + offsetVoltage;
#endif
}

// Measures voltage of a battery as per interval or after bootup (after allowing a few seconds to settle down)
void Battery_Cyclic(void)
{
#ifdef MEASURE_BATTERY_VOLTAGE
    static uint32_t lastVoltageCheckTimestamp = 0;

    if ((millis() - lastVoltageCheckTimestamp >= voltageCheckInterval * 60000) || (!lastVoltageCheckTimestamp && millis() >= 10000))
    {
        float voltage = Battery_GetVoltage();

        if (voltage <= warningLowVoltage)
        {
            snprintf(Log_Buffer, Log_BufferLength, "%s: (%.2f V)", (char *)FPSTR(voltageTooLow), voltage);
            Log_Println(Log_Buffer, LOGLEVEL_ERROR);
            Led_Indicate(LedIndicatorType::VoltageWarning);
        }

#ifdef MQTT_ENABLE
        char vstr[6];
        snprintf(vstr, 6, "%.2f", voltage);
        publishMqtt((char *)FPSTR(topicBatteryVoltage), vstr, false);
#endif
        snprintf(Log_Buffer, Log_BufferLength, "%s: %.2f V", (char *)FPSTR(currentVoltageMsg), voltage);
        Log_Println(Log_Buffer, LOGLEVEL_INFO);
        lastVoltageCheckTimestamp = millis();
    }
#endif
}