# Tonuino-PCB based on Wemos' Lolin32

## Introduction
After I've been asked many times to provide a PCB, I finally did so :-) It makes use of Wemos' Lolin32 which is the predecessor of Lolin D32. D32's advantage over Lolin32 is especially, that a voltage-divider for measuring battery's voltage is already integrated (fixed-wired to GPIO 35). However, as I wasn't aware of that when buying Lolin32 and because of now, that multiple Lolin32 are here on my desk, my reasonable intention was to use them. So things would have been a bit easier with D32 but in the end it works the same way with Lolin32. As per rev2 of the PCB, I added a 10 uF-capacitor to smoothen the battery-measurement. Ii's optional but be advised values measured are a bit more volatile without capacitor-stabilization.

## Features
* Fits Wemos Lolin32 (not Lolin D32, Lolin D32 pro or Lolin 32 lite!)
* Outer diameter: 56 x 93mm
* JST-PH 2.0-connectors for buttons, rotary encoder, Neopixel, RFID, reset and battery (not 2.54mm pitch!)
* 2.54mm-connectors for MAX98357a and uSD-card-reader
* Mosfet-circuit that switches off MAX98357a, Neopixel, headphone-pcb and uSD-card-reader automatically when deepsleep is active
* All peripherals are solely driven at 3.3V! Keep this especially in mind when choosing uSD-reader. If in doubts use one without voltage-regulator (link below).
* If [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is used, MAX98357a is automatically muted when there's a headphone plugged in and vice versa.
* If `HEADPHONE_ADJUST_ENABLE` is enabled and a headphone is plugged in, an alternative maximum volume is activated. I added this feature because [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) makes use of an amp that (probably) "allows" children to damage ears. This maximum volume can be set and re-adjusted via webgui.
* rev2: Reset-pinheader added. Can be used to connect e.g. a [micro switch](https://www.ebay.de/itm/10x-Mini-Taster-Drucktaster-klein-Mikroschalter-6x6x5mm-Arduino-Raspberry-Pi/333273061003) to it, so you can reset Tonuino even in battery-mode from the outside of the enclosure. Micro switch can be placed somewhat hidden at the enclosure.

## Prerequisites
* If no [headphone-pcb](https://github.com/biologist79/Tonuino-ESP32-I2S/tree/master/PCBs/Headphone%20with%20PCM5102a%20and%20TDA1308) is connected, make sure `HEADPHONE_ADJUST_ENABLE` is not active.
* I used 390/130 kOhms-resistors as voltage-divider. However, make sure to use a multimeter to determine their exact values in order to achieve a better battery-measurement. They can be configured in `settings.h` as `rdiv1` and `rdiv2`. Hint: for Lolin D32's battery-measurement 100k+100k were used. However, I decided to change the ratio from 50/50% to 25/75% to have a "better" signal. 100/100 might be work as well; didn't test it.

## Things to mention
* RFID: In order to avoid buying a 6pin-JST-PH-connector I used 2x3pin instead. This is because I already had ten of them (see link below).
* In contrast to Lolin D32, Lolin32 doesn't feature an integrated voltage-divider. That's why on the lower left there's a JST-PH2.0-connector to connect the LiPo-battery. Make sure to connect (+) to the left und GND to the right. From there you need to solder two short wires (5cm or so) onto the pcb with a JST-PH2.0-connector attached on the other side. This one needs to be plugged into Lolin32 (see pictures-folder). Please note: Lolin's JST-PH2.0-connector needs (+) left side and GND right side. Don't be confused if black/red-colouring of the JST-wires used seems "weird" because is reversed (black => (+); red => GND).
* Better don't solder Lolin32 directly to the PCB. I recommend to make use of female connectors instead (link below).
* When ordering a LiPo-battery, make sure to use [one](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm) with deep discharge protection! This is really really really important!!!

## Hardware-setup
The heart of my project is an ESP32 on a [Wemos Lolin32 development-board](https://www.ebay.de/itm/4MB-Flash-WEMOS-Lolin32-V1-0-0-WIFI-Bluetooth-Card-Based-ESP-32-ESP-WROOM-32/162716855489). Make sure to install the drivers for the USB/Serial-chip (CP2102 e.g.).
* [MAX98357A (like Adafruit's)](https://de.aliexpress.com/item/32999952454.html)
* [uSD-card-reader 3.3V only](https://www.ebay.de/itm/Micro-SPI-Kartenleser-Card-Reader-2GB-SD-8GB-SDHC-Card-3-3V-ESP8266-Arduino-NEU/333796577968)
* [RFID-reader](https://www.amazon.de/AZDelivery-Reader-Arduino-Raspberry-gratis/dp/B074S8MRQ7)
* [RFID-tags](https://www.amazon.de/AZDelivery-Keycard-56MHz-Schlüsselkarte-Karte/dp/B07TVJPTM7)
* [Neopixel-ring](https://de.aliexpress.com/item/32673883645.html)
* [Rotary Encoder](https://de.aliexpress.com/item/33041814942.html)
* [Buttons](https://de.aliexpress.com/item/32896285438.html)
* [Speaker](https://www.visaton.de/de/produkte/chassiszubehoer/breitband-systeme/fr-7-4-ohm)
* uSD-card: doesn't have to be a super-fast one; uC is limiting the throughput. Tested 32GB without any problems.
* [JSP PH-2.0-connectors](https://de.aliexpress.com/item/32968344273.html)
* [Female connector](https://de.aliexpress.com/item/32724478308.html)
* [(optional) IDC-connector female 6pin for headphone-pcb](https://de.aliexpress.com/item/33029492417.html)
* [(optional) IDC-connector male 6pin for headphone-pcb](https://de.aliexpress.com/item/1005001400147026.html)
* [(optional) Mikro switch](https://www.ebay.de/itm/10x-Mini-Taster-Drucktaster-klein-Mikroschalter-6x6x5mm-Arduino-Raspberry-Pi/333273061003)
* [LiPo-battery (2500 mAh) with connector JST PH 2.0mm](https://www.eremit.de/p/eremit-3-7v-2500mah-lipo-104050-jst-ph-2-0mm)

## Parts
* 1x IRF530NPbF (N-channel MOSFET)
* 1x NDP6020P (P-channel MOSFET)
* 1x 1k resistor
* 1x 10k resistor
* 2x 100k resistor
* 1x 130k resistor (can be replaced by 100k)
* 1x 390k resistor (can be replaced by 100k)
* 5x JST-PH2.0-connector (2 Pins)
* 3x JST-PH2.0-connector (3 Pins)
* 1x JST-PH2.0-connector (5 Pins)
* Female connector as socket for Lolin32, uSD-reader and MAX98357a
* (optional for headphone-PCB) 1x IDC-connecor female (6pin)
* (optional for headphone-PCB) 1x IDC-connecor male (6pin)
* rev2: (optional) 1x 10 uF capacitor (2.0mm-pitch)

## Where to order?
I ordered my PCBs at [jlcpcb](https://jlcpcb.com/). You have to order at least 5 pcs, which is only at 2$ + shipping. It took two weeks to arrive. If you want to have a look at the PCBs first (without having KiCad installed), visit [Gerberlook](https://www.gerblook.org/) and upload `gerber.zip` from the Gerberfiles-folder.

## Do I need to install KiCad?
Unless you don't want to change anything: no! All you need to provide are the gerberfiles (`gerber.zip`) to your manufactur (e.g. [jlcpcb](https://jlcpcb.com/)). However, all Kicad-files used are provided as well.