#ifndef __ESPUINO_SETTINGS_LOLIN_D32_H__
#define __ESPUINO_SETTINGS_LOLIN_D32_H__
    #include "Arduino.h"

    //######################### INFOS ####################################
    /* This is a develboard-specific config-file for *Wemos Lolin D32*. Specific doesn't mean it's only working with this board.
    Lolin D32 is the successor of Lolin32 and the "little brother" of Wemos Lolin D32 pro.
    PCB: https://github.com/biologist79/ESPuino/tree/master/PCBs/Wemos%20Lolin%20D32
    Infos: https://www.wemos.cc/en/latest/d32/d32.html
    Schematics: https://www.wemos.cc/en/latest/_static/files/sch_d32_v1.0.0.pdf
    Caveats: GPIO35 (battery monitoring) can't be changed, it's built in
    Status:
        tested with 2x SPI: RC522 & SD (by biologist79)
    */


    //################## GPIO-configuration ##############################
    // Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
    // So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
    // Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
#endif
