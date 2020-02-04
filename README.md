# Tonuino based on ESP32 with I2S-output

## Disclaimer
This is a fork of the popular [Tonuino-project](https://github.com/xfjx/TonUINO) which means, that it only shares the basic concept of controlling music-play by RFID-tags and buttons. Said this I want to make clear, that the code-basis is completely different. So there might be features, that a supported by my fork whereas others are missing or implemented different. For sure both share, that's it's non-profit, DIY and developed on [Arduino](https://www.arduino.cc/).

## What's different (basically)?
The original project makes use of microcontrollers (uC) like Arduino nano (which is the Microchip AVR-platform behind the scenes). Music-decoding is done in hardware using [DFPlayer mini](https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299) which also has a uSD-card-slot and an integrated amp as well. Control of this unit is done by a serial-interconnect with the uC using an api provided.

The core of my implementation is based on the popular [ESP32 by Espressif](https://www.espressif.com/en/products/hardware/esp32/overview). Having WiFi-support out of the box makes it possible to provide features like an integrated FTP-server (to feed the player with music), smarthome-integration by using MQTT and webradio. However, my aim was to port the project on a modular base which means, that music-decoding takes place in software with a dedicated uSD-card and music-output with I2S. I did all my tests on [Adafruit's MAX98357A](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts). Hopefully, not only in theory, other DACs can be used as well.

## Basic concept/handling
The basic idea of Tonuino (and my fork as well) is to provide a way, to use the Arduino-platform for a music-control-concept that is derived from the popular Toniebox. This basically means that RFID-tags are used to direct a music-player. Even for kids is concept is simple: place an RFID-object (card, character) on top of a box and the music starts to play. Place another object on it and the player plays anything else.

## Hardware-setup
So it's about time to have a look at the hardware I used. I'm using an ESP32 on a development-board that more or less looks like [this](https://docs.zerynth.com/latest/official/board.zerynth.nodemcu_esp32/docs/index.html). If ordered in China (Aliexpress, eBay e.g.) it's pretty cheap (around 4€) but even in Europe it's only around 8€.
* [Adafruit's MAX98357A](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts)
* [uSD-card-reader](https://www.amazon.de/AZDelivery-Reader-Speicher-Memory-Arduino/dp/B077MB17JB)
* [RFID-reader](https://www.amazon.de/AZDelivery-Reader-Arduino-Raspberry-gratis/dp/B074S8MRQ7)
* [RFID-tags](https://www.amazon.de/AZDelivery-Keycard-56MHz-Schlüsselkarte-Karte/dp/B07TVJPTM7)
* [Neopixel-ring](https://www.ebay.de/itm/16Bit-RGB-LED-Ring-WS2812-5V-ahnl-Neopixel-fur-Arduino-Raspberry-Pi/173881828935)
* [Rotary Encoder](https://www.amazon.de/gp/product/B07T3672VK)
* [Buttons](https://de.aliexpress.com/item/32697109472.html)

Most of them can be ordered cheaper directly in China. It's just a give an short impression of the hardware.

## Getting Started
I recommend Microsoft's [Visual Studio Code](https://code.visualstudio.com/) alongside with [Platformio Plugin](https://platformio.org/install/ide?install=vscode.) My project in Github contains platformio.ini, so libararies used should be fetched automatically. Please note: If you use another ESP32-develboard (Lolin32) you have to change "env:" in platformio.ini to the corresponding value. Documentation can be found [here](https://docs.platformio.org/en/latest/projectconf.html). After that it might be necessary to adjust the names of the GPIO-pins in the upper #define-section of my code.

## Wiring
| ESP32 (GPIO)  | Hardware              | Pin    | Comment  |
| ------------- |:---------------------:| ------:| --------:|
| 15            | SD-reader             | CS     |                                                              |
| 13            | SD-reader             | MOSI   |                                                              |
| 16            | SD-reader             | MISO   |                                                              |
| 14            | SD-reader             | SCK    |                                                              |
| 5 V           | SD-reader             | VCC    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | SD-reader             | GND    |                                                              |
| 17            | RFID-reader           | 3.3V   | Connect directly to GPIO 17 for power-saving when uC is off  |
| GND           | RFID-reader           | GND    |                                                              |
| 22            | RFID-reader           | RST    |                                                              |
| 21            | RFID-reader           | CS     |                                                              |
| 23            | RFID-reader           | MOSI   |                                                              |
| 19            | RFID-reader           | MISO   |                                                              |
| 18            | RFID-reader           | SCK    |                                                              |
| 5 V           | MAX98357              | VIN    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | MAX98357              | GND    |                                                              |
| 25            | MAX98357              | DIN    |                                                              |
| 27            | MAX98357              | BCLK   |                                                              |
| 26            | MAX98357              | LRC    |                                                              |
| 34            | Rotary encoder        | CLR    |                                                              |
| 35            | Rotary encoder        | DT     |                                                              |
| 32            | Rotary encoder        | BUTTON |                                                              |
| 3.3 V         | Rotary encoder        | +      |                                                              |
| GND           | Rotary encoder        | GND    |                                                              |
| 4             | Button (next)         |        |                                                              |
| GND           | Button (next)         |        |                                                              |
| 35            | Button (previous)     |        |                                                              |
| GND           | Button (previous)     |        |                                                              |
| 32            | Button (pause/play)   |        |                                                              |
| GND           | Button (pause/play)   |        |                                                              |
| 5 V           | Neopixel              | 5 V    | Connect to p-channel MOSFET for power-saving when uC is off  |
| GND           | Neopixel              | GND    |                                                              |
| 12            | Neopixel              | DI     |                                                              |
| 17            | BC337 (via R5)        | Base   |                                                              |

Optionally, GPIO 17 can be used to drive an NPN-transistor (BC337-40) that pulls a p-channel MOSFET (IRF9520) to GND in order to switch off current. Transistor-current is described [here](https://dl6gl.de/schalten-mit-transistoren): Just have a look at Abb. 4. R1: 10k, R2: omitted(!), R4: 10k, R5: 4,7k

## Interacting with Tonuino-ESP32
Todo...