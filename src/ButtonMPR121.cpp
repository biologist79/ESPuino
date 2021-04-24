#include <Arduino.h>
#include "settings.h"
//#include "Log.h"
#include "i2c.h"
#include "ButtonMPR121.h"
#include <MPR121.h>
#include <Wire.h>

// Init MPR121 Touch-Buttons
#ifdef PORT_TOUCHMPR121_ENABLE
MPR121_type touchsensor = MPR121_type();

void ButtonMPR121_Init() {
#ifdef PORT_TOUCHMPR121_ENABLE
  if (!touchsensor.begin(MPR121_I2C_ADR, 40, 20, MPR121_IRQ_PIN, &i2cBusTwo)) {
    Serial.println("error setting up MPR121");
    while (1);
    }
    Serial.println("MPR121 initialized");

//    touchsensor.setInterruptPin(MPR121_IRQ_PIN);
    touchsensor.setTouchThreshold(40);
    touchsensor.setReleaseThreshold(20);
    touchsensor.setFFI(FFI_10);
    touchsensor.setSFI(SFI_10);
    touchsensor.setGlobalCDT(CDT_4US);  // reasonable for larger capacitances
    
    touchsensor.autoSetElectrodes();  // autoset all electrode settings
    touchsensor.updateTouchData();
  #endif
}

void ButtonMPR121_Cyclic() {
#ifdef PORT_TOUCHMPR121_ENABLE
    if (touchsensor.touchStatusChanged()) {
        Serial.println("MPRHandler called  ->  IRQ LOW");
        touchsensor.updateTouchData();
        for (int i = 0; i < numElectrodes; i++) {
            if (touchsensor.isNewTouch(i)) {
                Serial.print(i, DEC);
                Serial.println(" was just touched");
            } else if (touchsensor.isNewRelease(i)) {
                Serial.print(i, DEC);
                Serial.println(" was just released");
            }
        }
    }
#endif
}

#endif