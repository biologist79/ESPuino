#pragma once

void Wlan_Init(void);
void Wlan_Cyclic(void);
bool Wlan_AddNetworkSettings(String, String);
uint8_t Wlan_NumSavedNetworks();
uint8_t Wlan_GetSSIDs(String*);
String Wlan_GetPwd(String);
String Wlan_GetCurrentSSID();
String Wlan_GetHostname();
bool Wlan_DeleteNetwork(String);
bool Wlan_SetHostname(String);
bool Wlan_IsConnected(void);
void Wlan_ToggleEnable(void);
String Wlan_GetIpAddress(void);
int8_t Wlan_GetRssi(void);
bool Wlan_ConnectionTryInProgress(void);
