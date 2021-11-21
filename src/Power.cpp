#include <Arduino.h>
#include "settings.h"
#include "Power.h"



void Power_Init(void){
    pinMode(POWER, OUTPUT);
    digitalWrite(POWER, POWER_OFF);
}

void Power_PeripheralOn(void){
    digitalWrite(POWER, POWER_ON);
}

void Power_PeripheralOff(void){
    digitalWrite(POWER, POWER_OFF);
}