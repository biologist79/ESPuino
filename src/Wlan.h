#pragma once

void Wlan_Init(void);
void Wlan_Cyclic(void);
bool Wlan_IsConnected(void);
boolean Wlan_ToggleEnable(void);
String Wlan_GetIpAddress(void);
int8_t Wlan_GetRssi(void);
bool Wlan_ConnectionTryInProgress(void);
