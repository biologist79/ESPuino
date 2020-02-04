# Tonuino based on ESP32 with I2S-output

## Disclaimer
This is a fork of the popular [Tonuino-project](https://github.com/xfjx/TonUINO) which means, that it only shares the basic concept of controlling music-play by RFID-tags and buttons. Said this I want to make clear, that the code-basis is completely different. So there might be features, that a supported by my fork whereas others are missing or implemented different.

## What's different (basically)?
The original project makes use of microcontrollers (uC) like Arduino nano (which is the Microchip AVR-platform behind the scenes). Music-decoding is done in hardware using [DFPlayer mini](https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299) which also has a uSD-card-slot and an integrated amp as well. Control of this unit is done by a serial-interconnect with the uC using an api provided.

The core of my implementation is based on the popular [ESP32 by Espressif](https://www.espressif.com/en/products/hardware/esp32/overview). Having WiFi-support out of the box makes it possible to provide features like an integrated FTP-server (to feed the player with music), smarthome-integration by using MQTT and webradio. However, my aim was to port the project on a modular base which means, that music-decoding takes place in software with a dedicated uSD-card and music-output with I2S. I did all my tests on [Adafruit's MAX98357A](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts). Hopefully, not only in theory, other DACs can be used as well.
