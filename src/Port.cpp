#include <Arduino.h>
#include <Wire.h>
#include "settings.h"
#include "Port.h"
#include "i2c.h"

#ifdef PORT_EXPANDER_ENABLE

uint8_t Port_ExpanderPorts[portsToRead];
bool Port_ExpanderHandler(void);
#endif

void Port_Init(void)
{
}

void Port_Cyclic(void)
{
#ifdef PORT_EXPANDER_ENABLE
    Port_ExpanderHandler();
#endif
}

// Wrapper: reads from GPIOs (via digitalRead()) or from port-expander (if enabled)
// Behaviour like digitalRead(): returns true if not pressed and false if pressed
bool Port_Read(const uint8_t _channel)
{
    switch (_channel)
    {
    case 0 ... 39: // GPIO
        return digitalRead(_channel);

#ifdef PORT_EXPANDER_ENABLE
    case 100 ... 107:                                        // Port-expander (port 0)
        return (Port_ExpanderPorts[0] & (1 << (_channel - 100))); // Remove offset 100 (return false if pressed)

    case 108 ... 115: // Port-expander (port 1)
        if (portsToRead == 2)
        {                                                        // Make sure portsToRead != 1 when channel > 107
            return (Port_ExpanderPorts[1] & (1 << (_channel - 108))); // Remove offset 100 + 8 (return false if pressed)
        }
        else
        {
            return true;
        }

#endif

    default: // Everything else (doesn't make sense at all) isn't supposed to be pressed
        return true;
    }
}

#ifdef PORT_EXPANDER_ENABLE
// Reads input from port-expander and writes output into global array
// Datasheet: https://www.nxp.com/docs/en/data-sheet/PCA9555.pdf
bool Port_ExpanderHandler()
{
    i2cBusTwo.beginTransmission(expanderI2cAddress);
    for (uint8_t i = 0; i < portsToRead; i++)
    {
        i2cBusTwo.write(0x00 + i); // Go to input-register...
        i2cBusTwo.endTransmission();
        i2cBusTwo.requestFrom(expanderI2cAddress, 1); // ...and read its byte

        if (i2cBusTwo.available())
        {
            Port_ExpanderPorts[i] = i2cBusTwo.read();
        }
        else
        {
            return false;
        }
    }
    return false;
}
#endif