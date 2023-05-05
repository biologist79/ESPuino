#pragma once

// be very careful changing this struct, as it is used for NVS storage and will corrupt existing entries
struct WiFiSettings {
	char ssid[33];
	char password[65];
	bool use_static_ip;
	IPAddress static_addr;
	IPAddress static_gateway;
	IPAddress static_subnet;
	IPAddress static_dns1;
	IPAddress static_dns2;
};

void Wlan_Init(void);
void Wlan_Cyclic(void);
bool Wlan_AddNetworkSettings(WiFiSettings);
uint8_t Wlan_NumSavedNetworks();
size_t Wlan_GetSSIDs(String*, size_t);
const String Wlan_GetCurrentSSID();
const String Wlan_GetHostname();
bool Wlan_DeleteNetwork(String);
bool Wlan_SetHostname(String);
bool Wlan_IsConnected(void);
void Wlan_ToggleEnable(void);
String Wlan_GetIpAddress(void);
int8_t Wlan_GetRssi(void);
bool Wlan_ConnectionTryInProgress(void);


