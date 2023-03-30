#ifndef __ESPUINO_SETTINGS_LOLIN32_H__
#define __ESPUINO_SETTINGS_LOLIN32_H__
#include "Arduino.h"

//######################### INFOS ####################################
/* This is a develboard-specific config-file for *Wemos Lolin32*. Specific doesn't mean it's only working with this board.
   Lolin32 is the predecessor of Lolin D32.
   PCB: https://github.com/biologist79/ESPuino/tree/master/PCBs/Wemos%20Lolin32
   PCB: https://forum.espuino.de/t/lolin32-mit-sd-sd-mmc-und-pn5180-als-rfid-leser/77
   Infos: https://arduino-projekte.info/wemos-lolin32/
   Caveats: None
   Status:
    tested with 2x SPI: RC522 & SD (by biologist79)
    tested with 1x SPI: PN5180, SD (MMC) (by tueddy and biologist79) => don't enable CONFIG_SINGLE_SPI
*/

//################## GPIO-configuration ##############################
// Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
// So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
// Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
#endif
