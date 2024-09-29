#pragma once

#ifdef MQTT_ENABLE
	#define MQTT_SOCKET_TIMEOUT 1 // https://github.com/knolleary/pubsubclient/issues/403
	#include <PubSubClient.h>
#endif
// MQTT-configuration
// Please note: all lengths will be published n-1 as maxlength to GUI
constexpr uint8_t mqttClientIdLength = 16u;
constexpr uint8_t mqttServerLength = 32u;
constexpr uint8_t mqttUserLength = 16u;
constexpr uint8_t mqttPasswordLength = 16u;

extern String gMqttUser;
extern String gMqttPassword;
extern uint16_t gMqttPort;

void Mqtt_Init(void);
void Mqtt_Cyclic(void);
void Mqtt_Exit(void);
bool Mqtt_IsEnabled(void);

bool publishMqtt(const char *topic, const char *payload, bool retained);
bool publishMqtt(const char *topic, int32_t payload, bool retained);
#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 3))
bool publishMqtt(const char *topic, unsigned long payload, bool retained);
#endif
bool publishMqtt(const char *topic, uint32_t payload, bool retained);
