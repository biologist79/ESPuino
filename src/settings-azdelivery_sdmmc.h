#ifndef __ESPUINO_SETTINGS_CUSTOM_H__
#define __ESPUINO_SETTINGS_CUSTOM_H__
    #include "Arduino.h"

    //######################### INFOS ####################################
    /* This is a PCB-specific config-file for *AZ Delivery ESP32 NodeMCU/Devkit C with SD_MMC and PN5180*.
    PCB: https://github.com/biologist79/ESPuino/blob/master/PCBs/AZDelivery_ESP32_NodeMCU/gerber/gerber_rev2.zip
    Forum: https://forum.espuino.de/t/az-delivery-esp32-nodemcu-devkit-c-mit-sd-mmc-und-pn5180-als-rfid-leser/634
    Infos: https://www.amazon.de/AZDelivery-NodeMCU-Development-Nachfolgermodell-ESP8266/dp/B074RGW2VQ
    Schematics PCB: https://github.com/biologist79/ESPuino/blob/master/PCBs/AZDelivery_ESP32_NodeMCU/pictures/Schematics_rev2.pdf
    Caveats: Battery-monitoring is not available and SD ist SD_MMC only
    Status:
        tested with PN5180 + SD_MMC (by biologist79)
    */

    //################## GPIO-configuration ##############################
    // Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
    // So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
    // Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/

    // Channels of port-expander can be read cyclic or interrupt-driven. It's strongly recommended to use the interrupt-way!
    // Infos: https://forum.espuino.de/t/einsatz-des-port-expanders-pca9555/306
    #ifdef CONFIG_PORT_EXPANDER
        // Not supported
    #endif
#endif
