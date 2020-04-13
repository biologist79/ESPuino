# OpenHAB-integration for Tonuino

## Disclaimer
[OpenHAB](https://www.openhab.org/) is a pretty complex software for home automation. I Just extracted that parts of my local config, that's necessary for Tonuino. Hopefully I didn't forget anything :-) Said this I want to rule out that this document's aim isn't to provide a fully-featured howto. For further informations please have a look at the project's [documentation](https://www.openhab.org/docs/).

## What's necessary
In order to make use of my config-files you need to have a running openHAB-installation. Mine is running on the Raspberry Pi. In general there are two ways to achieve this:
* [Raspbian with manual openHAB-installation](https://www.openhab.org/docs/installation/rasppi.html)
* [openHABian](https://www.openhab.org/docs/installation/openhabian.html)
After completing the installation, [PaperUI](http://<ip>:8080/paperui/index.html) should be browsable. In order to get Tonuino running you need at least the MQTT-binding (Add-ons -> Bindings) and the Map-transformation (Add-ons -> transformations).
Beside of openHAB you need a MQTT-broker. You can use a public one but if there's already a Raspberry Pi running with openHAB, it probably makes sense to install [Mosquitto](https://mosquitto.org/) as MQTT-broker in parallel.

## MQTT
After MQTT-broker is set up have look at mqttConnections.things in order to configure the MQTTT-connection between openHAB and the broker. In doubt restart openHAB as changes sometimes don't get recognized immediately without restart. If nothing happens have a look at the logfiles /var/log/openhab2/openhab.log or /var/log/openhab2/events.log. In general, to debug MQTT-stuff, [MQTT fx](https://mqttfx.jensd.de/) is a good tool to refer.

## Important
In openHAB it's your choice to make use of configuration via Paper UI or textfiles. Sounds basically good but in fact it's not, because mixing up both can be really crappy and painstaking to debug. Make sure to only use one way.