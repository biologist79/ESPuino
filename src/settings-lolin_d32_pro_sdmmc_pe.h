#ifndef __ESPUINO_SETTINGS_LOLIN_D32_PRO_H__
#define __ESPUINO_SETTINGS_LOLIN_D32_PRO_H__
    #include "Arduino.h"

    //######################### INFOS ####################################
    /* This is a PCB-specific config-file for *Wemos Lolin32 D32 pro with port-expander PCA9555PW and SD_MMC*.
    PCB: t.b.a.
    Forum: https://forum.espuino.de/t/espuino-minid32-pro-lolin-d32-pro-mit-sd-mmc-und-port-expander-smd/866
    Infos: https://www.wemos.cc/en/latest/d32/d32_pro.html
    Schematics Lolin D32 pro: https://www.wemos.cc/en/latest/_static/files/sch_d32_pro_v2.0.0.pdf
    Schematics PCB: t.b.a.
    Caveats: GPIO35 (battery monitoring) + don't use internal SD-slot as it's to capable of SD_MMC because of its pin-layout!
    Status:
        tested with PN5180 + SD_MMC (by biologist79)
    */

    //################## GPIO-configuration ##############################
    // Please note: GPIOs 34, 35, 36, 39 are input-only and don't have internal pullup-resistors.
    // So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
    // Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
    // GPIOs 16+17 are not available for D32 pro as they're used to internal purposes (PSRAM).
    // All GPIOs >=100 and <= 115 are connected to a port-expander
#endif
