#ifndef __ESPUINO_SETTINGS_LOLIN_D32_PRO_H__
#define __ESPUINO_SETTINGS_LOLIN_D32_PRO_H__
    #include "Arduino.h"

    //######################### INFOS ####################################
    /* This is a develboard-specific config-file for *Wemos Lolin32*. Specific doesn't mean it's only working with this board.
    Lolin32 is the predecessor of Lolin D32.
    PCB: None so far
    Infos: https://www.wemos.cc/en/latest/d32/d32_pro.html
    Schematics: https://www.wemos.cc/en/latest/_static/files/sch_d32_pro_v2.0.0.pdf
    Caveats: GPIO35 (battery monitoring) + SD can't be changed, it's built in (and because of the SD-pinout used, it is not compatible with MMC-mode)
    Status:
        tested with 2xSPI: RC522 & SD (by biologist79)
    */

    //################## GPIO-configuration ##############################
    // Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
    // So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
    // Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
    // GPIOs 16+17 are not available for D32 pro as they're used to internal purposes (PSRAM).
#endif
