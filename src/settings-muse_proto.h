#ifndef __ESPUINO_SETTINGS_MUSEPROTO_H__
#define __ESPUINO_SETTINGS_MUSEPROTO_H__
    #include "Arduino.h"

    //######################### INFOS ####################################
    /*
     * Make sure to read instructions:
     * https://forum.espuino.de/t/anleitung-raspiaudio-muse-proto-anfaenger-espuino/1598
     *
     * Support for this device is limited, and only available by the community in the
     * forum. (See thread above)
     *
     * This is a device-specific settings file for the RASPIAUDIO Muse Proto
     * offered by https://raspiaudio.com/produit/muse-proto and available at
     * multiple sources. It uses
     *
     * * On-Board Neopixel (1 Pixel)
     * * PN532 NFC Reader, using I2C, GPIOs 12 (CLK) and 32 (DATA)
     * * KY-040 Rotary Encoder with on-board pull ups on GPIOs 36, 39 (Printed VP/VN)
     * * Three buttons without pull ups on GPIOs 18, 19, 0
     * * Batteries measured using the on-board battery controller
     * * Using the on-board 3W output for externals speakers
     *
    */

    //################## GPIO-configuration ##############################
    // Please note: GPIOs 34, 35, 36, 39 are input-only and don't have pullup-resistors.
    // So if connecting a button to these, make sure to add a 10k-pullup-resistor for each button.
    // Further infos: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
#endif
