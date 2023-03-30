#include <Arduino.h>
#include "settings.h"
#include "Power.h"
#include "Port.h"



void Power_Init(void) {
	#if (CONFIG_GPIO_POWER >= 0 && CONFIG_GPIO_POWER <= MAX_GPIO)
		pinMode(CONFIG_GPIO_POWER, OUTPUT);     // Only necessary for GPIO. For port-expander it's done (previously) via Port_init()
	#endif
}

// Switch on peripherals. Please note: meaning of POWER_ON is HIGH per default. But is LOW in case of CONFIG_GPIO_POWER_ACTIVE_LOW is enabled.
void Power_PeripheralOn(void) {
	Port_Write(CONFIG_GPIO_POWER, POWER_ON, false);
	#ifdef CONFIG_BUTTON_LEDS
		Port_Write(CONFIG_GPIO_BUTTON_LEDS, HIGH, false);
	#endif
	delay(50);  // Give peripherals some time to settle down
}

// Switch off peripherals. Please note: meaning of POWER_OFF is LOW per default. But is HIGH in case of CONFIG_GPIO_POWER_ACTIVE_LOW is enabled.
void Power_PeripheralOff(void) {
	Port_Write(CONFIG_GPIO_POWER, POWER_OFF, false);
	#ifdef CONFIG_BUTTON_LEDS
		Port_Write(CONFIG_GPIO_BUTTON_LEDS, LOW, false);
	#endif
}
