#include <Arduino.h>
#include "settings.h"

#include "Power.h"

#include "Port.h"

void Power_Init(void) {
#ifdef POWER
	#if (POWER >= 0 && POWER <= MAX_GPIO)
	pinMode(POWER, OUTPUT); // Only necessary for GPIO. For port-expander it's done (previously) via Port_init()
	#endif
#endif
}

// Switch on peripherals. Please note: meaning of POWER_ON is HIGH per default. But is LOW in case of INVERT_POWER is enabled.
void Power_PeripheralOn(void) {
#ifdef POWER
	Port_Write(POWER, POWER_ON, false);
	#ifdef BUTTONS_LED
	Port_Write(BUTTONS_LED, HIGH, false);
	#endif
	delay(50); // Give peripherals some time to settle down
#endif
}

// Switch off peripherals. Please note: meaning of POWER_OFF is LOW per default. But is HIGH in case of INVERT_POWER is enabled.
void Power_PeripheralOff(void) {
#ifdef POWER
	Port_Write(POWER, POWER_OFF, false);
	#ifdef BUTTONS_LED
	Port_Write(BUTTONS_LED, LOW, false);
	#endif
#endif
}
