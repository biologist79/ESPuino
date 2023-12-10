#include <Arduino.h>
#include "settings.h"

#include "Port.h"

#include "Log.h"

#include <Wire.h>

// Infos:
// PCA9555 has 16 channels that are subdivided into 2 ports with 8 channels each.
// Every channels is represented by a bit.
// Examples for ESPuino-configuration:
// 100 => port 0 channel/bit 0
// 107 => port 0 channel/bit 7
// 108 => port 1 channel/bit 0
// 115 => port 1 channel/bit 7

#ifdef PORT_EXPANDER_ENABLE
extern TwoWire i2cBusTwo;

uint8_t Port_ExpanderPortsInputChannelStatus[2];
static uint8_t Port_ExpanderPortsOutputChannelStatus[2]; // Stores current configuration of output-channels locally
void Port_ExpanderHandler(void);
uint8_t Port_ChannelToBit(const uint8_t _channel);
void Port_WriteInitMaskForOutputChannels(void);
void Port_Test(void);

	#if (PE_INTERRUPT_PIN >= 0 && PE_INTERRUPT_PIN <= MAX_GPIO)
		#define PE_INTERRUPT_PIN_ENABLE
void IRAM_ATTR PORT_ExpanderISR(void);
bool Port_AllowReadFromPortExpander = false;
bool Port_AllowInitReadFromPortExpander = true;
	#endif
#endif

void Port_Init(void) {
#ifdef PORT_EXPANDER_ENABLE
	Port_Test();
	Port_WriteInitMaskForOutputChannels();
	Port_ExpanderHandler();
#endif

#ifdef PE_INTERRUPT_PIN_ENABLE
	pinMode(PE_INTERRUPT_PIN, INPUT_PULLUP);
	attachInterrupt(PE_INTERRUPT_PIN, PORT_ExpanderISR, FALLING);
	Log_Println(portExpanderInterruptEnabled, LOGLEVEL_NOTICE);
#endif
}

void Port_Cyclic(void) {
#ifdef PORT_EXPANDER_ENABLE
	Port_ExpanderHandler();
#endif
}

// Wrapper: reads from GPIOs (via digitalRead()) or from port-expander (if enabled)
// Behaviour like digitalRead(): returns true if not pressed and false if pressed
bool Port_Read(const uint8_t _channel) {
	switch (_channel) {
		case 0 ... MAX_GPIO: // GPIO
			return digitalRead(_channel);

#ifdef PORT_EXPANDER_ENABLE
		case 100 ... 107: // Port-expander (port 0)
			return (Port_ExpanderPortsInputChannelStatus[0] & (1 << (_channel - 100))); // Remove offset 100 (return false if pressed)

		case 108 ... 115: // Port-expander (port 1)
			return (Port_ExpanderPortsInputChannelStatus[1] & (1 << (_channel - 108))); // Remove offset 100 + 8 (return false if pressed)

#endif

		default: // Everything else (doesn't make sense at all) isn't supposed to be pressed
			return true;
	}
}

// Configures OUTPUT-mode for GPIOs (non port-expander)
// Output-mode for port-channels is done via Port_WriteInitMaskForOutputChannels()
void Port_Write(const uint8_t _channel, const bool _newState, const bool _initGpio) {
#ifdef GPIO_PA_EN
	if (_channel == GPIO_PA_EN) {
		if (_newState) {
			Log_Println(paOn, LOGLEVEL_NOTICE);
		} else {
			Log_Println(paOff, LOGLEVEL_NOTICE);
		}
	}
#endif

#ifdef GPIO_HP_EN
	if (_channel == GPIO_HP_EN) {
		if (_newState) {
			Log_Println(hpOn, LOGLEVEL_NOTICE);
		} else {
			Log_Println(hpOff, LOGLEVEL_NOTICE);
		}
	}
#endif

	// Make init only for GPIO but not for PE (because PE is already done earlier)
	if (_initGpio) {
		if (_channel <= MAX_GPIO) {
			pinMode(_channel, OUTPUT);
		}
	}

	switch (_channel) {
		case 0 ... MAX_GPIO: { // GPIO
			digitalWrite(_channel, _newState);
			break;
		}

#ifdef PORT_EXPANDER_ENABLE
		case 100 ... 115: {
			uint8_t portOffset = 0;
			if (_channel >= 108 && _channel <= 115) {
				portOffset = 1;
			}

			uint8_t oldPortBitmask = Port_ExpanderPortsOutputChannelStatus[portOffset];
			uint8_t newPortBitmask;

			i2cBusTwo.beginTransmission(expanderI2cAddress);
			i2cBusTwo.write(0x02); // Pointer to output configuration-register
			if (_newState) {
				newPortBitmask = (oldPortBitmask | (1 << Port_ChannelToBit(_channel)));
				Port_ExpanderPortsOutputChannelStatus[portOffset] = newPortBitmask; // Write back new status
			} else {
				newPortBitmask = (oldPortBitmask & ~(1 << Port_ChannelToBit(_channel)));
				Port_ExpanderPortsOutputChannelStatus[portOffset] = newPortBitmask; // Write back new status
			}
			i2cBusTwo.write(Port_ExpanderPortsOutputChannelStatus[0]);
			i2cBusTwo.write(Port_ExpanderPortsOutputChannelStatus[1]);
			i2cBusTwo.endTransmission();
			break;
		}
#endif

		default: {
			break;
		}
	}
}

#ifdef PORT_EXPANDER_ENABLE
// Translates digitalWrite-style "GPIO" to bit
uint8_t Port_ChannelToBit(const uint8_t _channel) {
	switch (_channel) {
		case 100:
		case 108:
			return 0;
			break;
		case 101:
		case 109:
			return 1;
			break;
		case 102:
		case 110:
			return 2;
			break;
		case 103:
		case 111:
			return 3;
			break;
		case 104:
		case 112:
			return 4;
			break;
		case 105:
		case 113:
			return 5;
			break;
		case 106:
		case 114:
			return 6;
			break;
		case 107:
		case 115:
			return 7;
			break;

		default:
			return 255; // not valid!
	}
}

// Writes initial port-configuration (I/O) for port-expander PCA9555
// If no output-channel is necessary, nothing has to be configured as all channels are in input-mode as per default (255)
// So every bit representing an output-channel needs to be set to 0.
void Port_WriteInitMaskForOutputChannels(void) {
	const uint8_t portBaseValueBitMask = 255;
	const uint8_t portsToWrite = 2;
	uint8_t OutputBitMaskInOutAsPerPort[portsToWrite] = {portBaseValueBitMask, portBaseValueBitMask}; // 255 => all channels set to input; [0]: port0, [1]: port1

	// init status cache with values from HW
	i2cBusTwo.beginTransmission(expanderI2cAddress);
	i2cBusTwo.write(0x02); // Pointer to first output-register
	i2cBusTwo.endTransmission(false);
	i2cBusTwo.requestFrom(expanderI2cAddress, static_cast<size_t>(portsToWrite), true); // ...and read the contents
	if (i2cBusTwo.available()) {
		for (uint8_t i = 0; i < portsToWrite; i++) {
			Port_ExpanderPortsOutputChannelStatus[i] = i2cBusTwo.read();
		}
	}

	#ifdef GPIO_PA_EN // Set as output to enable/disable amp for loudspeaker
	if (GPIO_PA_EN >= 100 && GPIO_PA_EN <= 107) {
		// Bits of channels to be configured as input are 1 by default.
		// So in order to change I/O-direction to output we need to set those bits to 0.
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(GPIO_PA_EN));
	} else if (GPIO_PA_EN >= 108 && GPIO_PA_EN <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(GPIO_PA_EN));
	}
	#endif

	#ifdef GPIO_HP_EN // Set as output to enable/disable amp for headphones
	if (GPIO_HP_EN >= 100 && GPIO_HP_EN <= 107) {
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(GPIO_HP_EN));
	} else if (GPIO_HP_EN >= 108 && GPIO_HP_EN <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(GPIO_HP_EN));
	}
	#endif

	#ifdef POWER // Set as output to trigger mosfet/power-pin for powering peripherals. Hint: logic is inverted if INVERT_POWER is enabled.
	if (POWER >= 100 && POWER <= 107) {
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(POWER));
	} else if (POWER >= 108 && POWER <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(POWER));
	}
	#endif

	#ifdef BUTTONS_LED
	if (BUTTONS_LED >= 100 && BUTTONS_LED <= 107) {
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(BUTTONS_LED));
	} else if (BUTTONS_LED >= 108 && BUTTONS_LED <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(BUTTONS_LED));
	}
	#endif

	// Only change port-config if necessary (at least bitmask changed from base-default for one port)
	if ((OutputBitMaskInOutAsPerPort[0] != portBaseValueBitMask) || (OutputBitMaskInOutAsPerPort[1] != portBaseValueBitMask)) {
		// all outputs to LOW
		Port_ExpanderPortsOutputChannelStatus[0] &= OutputBitMaskInOutAsPerPort[0];
		Port_ExpanderPortsOutputChannelStatus[1] &= OutputBitMaskInOutAsPerPort[1];

		i2cBusTwo.beginTransmission(expanderI2cAddress);
		i2cBusTwo.write(0x06); // Pointer to configuration of input/output
		for (uint8_t i = 0; i < portsToWrite; i++) {
			i2cBusTwo.write(OutputBitMaskInOutAsPerPort[i]);
			// Serial.printf("Register %u - Mask: %u\n", 0x06+i, OutputBitMaskInOutAsPerPort[i]);
		}
		i2cBusTwo.endTransmission();

		// Write low/high-config to all output-channels. Channels that are configured as input are silently/automatically ignored by PCA9555
		i2cBusTwo.beginTransmission(expanderI2cAddress);
		i2cBusTwo.write(0x02); // Pointer to configuration of output-channels (high/low)
		i2cBusTwo.write(Port_ExpanderPortsOutputChannelStatus, static_cast<size_t>(portsToWrite));
		i2cBusTwo.endTransmission();
	}
}

// Some channels are configured as output before shutdown in order to avoid unwanted interrupts while ESP32 sleeps
void Port_MakeSomeChannelsOutputForShutdown(void) {
	const uint8_t portBaseValueBitMask = 255;
	const uint8_t portsToWrite = 2;
	uint8_t OutputBitMaskInOutAsPerPort[portsToWrite] = {portBaseValueBitMask, portBaseValueBitMask}; // 255 => all channels set to input; [0]: port0, [1]: port1
	uint8_t OutputBitMaskLowHighAsPerPort[portsToWrite] = {0x00, 0x00};

	#ifdef HP_DETECT // https://forum.espuino.de/t/lolin-d32-pro-mit-sd-mmc-pn5180-max-fuenf-buttons-und-port-expander-smd/638/33
	if (HP_DETECT >= 100 && HP_DETECT <= 107) {
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(HP_DETECT));
	} else if (HP_DETECT >= 108 && HP_DETECT <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(HP_DETECT));
	}
	#endif

	// There's no possibility to get current I/O-status from PCA9555. So we just re-set it again for OUTPUT-pins.
	#ifdef GPIO_PA_EN
	if (GPIO_PA_EN >= 100 && GPIO_PA_EN <= 107) {
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(GPIO_PA_EN));
	} else if (GPIO_PA_EN >= 108 && GPIO_PA_EN <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(GPIO_PA_EN));
	}
	#endif

	#ifdef GPIO_HP_EN
	if (GPIO_HP_EN >= 100 && GPIO_HP_EN <= 107) {
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(GPIO_HP_EN));
	} else if (GPIO_HP_EN >= 108 && GPIO_HP_EN <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(GPIO_HP_EN));
	}
	#endif

	#ifdef POWER // Set as output to trigger mosfet/power-pin for powering peripherals. Hint: logic is inverted if INVERT_POWER is enabled.
	if (POWER >= 100 && POWER <= 107) {
		#ifdef INVERT_POWER
		OutputBitMaskLowHighAsPerPort[0] |= (1 << Port_ChannelToBit(POWER));
		#else
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(POWER));
		#endif
	} else if (POWER >= 108 && POWER <= 115) {
		#ifdef INVERT_POWER
		OutputBitMaskLowHighAsPerPort[1] |= (1 << Port_ChannelToBit(POWER));
		#else
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(POWER));
		#endif
	}
	#endif

	#ifdef BUTTONS_LED
	if (BUTTONS_LED >= 100 && BUTTONS_LED <= 107) {
		OutputBitMaskInOutAsPerPort[0] &= ~(1 << Port_ChannelToBit(BUTTONS_LED));
	} else if (BUTTONS_LED >= 108 && BUTTONS_LED <= 115) {
		OutputBitMaskInOutAsPerPort[1] &= ~(1 << Port_ChannelToBit(BUTTONS_LED));
	}
	#endif

	// Only change port-config if necessary (at least bitmask changed from base-default for one port)
	if ((OutputBitMaskInOutAsPerPort[0] != portBaseValueBitMask) || (OutputBitMaskInOutAsPerPort[1] != portBaseValueBitMask)) {
		i2cBusTwo.beginTransmission(expanderI2cAddress);
		i2cBusTwo.write(0x06); // Pointer to configuration of input/output
		for (uint8_t i = 0; i < portsToWrite; i++) {
			i2cBusTwo.write(OutputBitMaskInOutAsPerPort[i]);
		}
		i2cBusTwo.endTransmission();

		// Write low/high-config to all output-channels. Channels that are configured as input are silently/automatically ignored by PCA9555
		i2cBusTwo.beginTransmission(expanderI2cAddress);
		i2cBusTwo.write(0x02); // Pointer to configuration of output-channels (high/low)
		i2cBusTwo.write(OutputBitMaskLowHighAsPerPort[0]); // port0
		i2cBusTwo.write(OutputBitMaskLowHighAsPerPort[1]); // port1
		i2cBusTwo.endTransmission();
	}
}

// Reads input-registers from port-expander and writes output into global cache-array
// Datasheet: https://www.nxp.com/docs/en/data-sheet/PCA9555.pdf
void Port_ExpanderHandler(void) {
	static bool verifyChangeOfInputRegister[2] = {false, false}; // Used to debounce once in case of register-change
	static uint8_t inputRegisterBuffer[2];

	// If interrupt-handling is active, only read port-expander's registers if interrupt was fired
	#ifdef PE_INTERRUPT_PIN_ENABLE
	if (!Port_AllowReadFromPortExpander && !Port_AllowInitReadFromPortExpander && !verifyChangeOfInputRegister[0] && !verifyChangeOfInputRegister[1]) {
		return;
	} else if (Port_AllowInitReadFromPortExpander) {
		Port_AllowInitReadFromPortExpander = false;
	} else if (Port_AllowReadFromPortExpander || Port_AllowInitReadFromPortExpander) {
		Port_AllowReadFromPortExpander = false;
	}
	#endif

	for (uint8_t i = 0; i < 2; i++) {
		i2cBusTwo.beginTransmission(expanderI2cAddress);
		i2cBusTwo.write(0x00 + i); // Pointer to input-register...
		uint8_t error = i2cBusTwo.endTransmission();
		if (error != 0) {
			Log_Printf(LOGLEVEL_ERROR, "Error in endTransmission(): %d", error);
			return;
		}
		i2cBusTwo.requestFrom(expanderI2cAddress, 1u); // ...and read its byte

		if (i2cBusTwo.available()) {
			inputRegisterBuffer[i] = i2cBusTwo.read(); // Cache current readout
			// Check if input-register changed. If so, don't use the value immediately
			// but wait another cycle instead (=> rudimentary debounce).
			// Added because there've been "ghost"-events occasionally with Arduino2 (https://forum.espuino.de/t/aktueller-stand-esp32-arduino-2/1389/55)
			if (inputRegisterBuffer[i] != Port_ExpanderPortsInputChannelStatus[i] && millis() >= 10000 && !verifyChangeOfInputRegister[i]) {
				verifyChangeOfInputRegister[i] = true;
				// Serial.println("Verify set!");
			} else {
				verifyChangeOfInputRegister[i] = false;
				Port_ExpanderPortsInputChannelStatus[i] = inputRegisterBuffer[i];
			}
			// Serial.printf("%u Debug: PE-Port: %u  Status: %u\n", millis(), i, Port_ExpanderPortsInputChannelStatus[i]);
		}
	}
}

// Make sure ports are read finally at shutdown in order to clear any active IRQs that could cause re-wakeup immediately
void Port_Exit(void) {
	Port_MakeSomeChannelsOutputForShutdown();
	for (uint8_t i = 0; i < 2; i++) {
		i2cBusTwo.beginTransmission(expanderI2cAddress);
		i2cBusTwo.write(0x00 + i); // Pointer to input-register...
		i2cBusTwo.endTransmission();
		i2cBusTwo.requestFrom(expanderI2cAddress, 1u); // ...and read its byte

		if (i2cBusTwo.available()) {
			Port_ExpanderPortsInputChannelStatus[i] = i2cBusTwo.read();
		}
	}
}

// Tests if port-expander can be detected at address configured
void Port_Test(void) {
	i2cBusTwo.beginTransmission(expanderI2cAddress);
	i2cBusTwo.write(0x02);
	if (!i2cBusTwo.endTransmission()) {
		Log_Println(portExpanderFound, LOGLEVEL_NOTICE);
	} else {
		Log_Println(portExpanderNotFound, LOGLEVEL_ERROR);
	}
}

	#ifdef PE_INTERRUPT_PIN_ENABLE
void IRAM_ATTR PORT_ExpanderISR(void) {
	Port_AllowReadFromPortExpander = true;
}
	#endif
#endif
