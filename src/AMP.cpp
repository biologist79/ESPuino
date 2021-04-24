#include <Arduino.h>
#include "settings.h"
#include "i2c.h"
#include "AMP.h"

#if (HAL == 2)
    AC101 ac(&i2cBusOne);
#endif

void AC101_Init() {
    #if (HAL == 2)
        while (not ac.begin()) {
            Serial.println(F("AC101 Failed!"));
            delay(1000);
        }
        Serial.println(F("AC101 via I2C - OK!"));

        pinMode(22, OUTPUT);
        digitalWrite(22, HIGH);

        pinMode(GPIO_PA_EN, OUTPUT);
        digitalWrite(GPIO_PA_EN, HIGH);
        Serial.println(F("Built-in amplifier enabled\n"));
    #endif
}