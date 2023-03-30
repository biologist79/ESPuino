#include "Arduino.h"

//######################### INFOS ####################################
/* This is a develboard-specific config-file for Lilygo T7 v1.7
   PCB: t.b.a
   Infos: https://github.com/LilyGO/TTGO-T8-ESP32
   Caveats: None
   Status:
    tested by biologist with SD_MMC + PN5180
*/

//################## GPIO-configuration ##############################
// Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
// So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
// Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
