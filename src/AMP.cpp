#include <Arduino.h>
#include "settings.h"
#include "i2c.h"
#include "AMP.h"

void AC101_Init() {
    #if (HAL == 2)
        AC101 ac(&i2cBusOne);
    #endif
}