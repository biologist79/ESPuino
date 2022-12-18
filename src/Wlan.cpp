#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "settings.h"
#include "AudioPlayer.h"
#include "RotaryEncoder.h"
#include "Log.h"
#include "System.h"
#include "Web.h"
#include "MemX.h"

// HELPER //
unsigned long wifiCheckLastTimestamp = 0;
bool wifiEnabled; // Current status if wifi is enabled
uint32_t wifiStatusToggledTimestamp = 0;
bool wifiNeedsRestart = false;
uint8_t wifiConnectIteration = 0;
bool wifiConnectionTryInProgress = false;
bool wifiInit = true;
uint32_t lastPrintRssiTimestamp = 0;

// AP-WiFi
IPAddress apIP(192, 168, 4, 1);        // Access-point's static IP
IPAddress apNetmask(255, 255, 255, 0); // Access-point's netmask
bool accessPointStarted = false;

void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask);
bool getWifiEnableStatusFromNVS(void);
void writeWifiStatusToNVS(bool wifiStatus);
bool Wlan_IsConnected(void);
int8_t Wlan_GetRssi(void);

// WiFi-credentials + hostname
String hostname;
char *_ssid;
char *_pwd;

void Wlan_Init(void) {
	wifiEnabled = getWifiEnableStatusFromNVS();
}

void Wlan_Cyclic(void) {
	// If wifi would not be activated, return instantly
	if (!wifiEnabled) {
		return;
	}

	if (wifiInit || wifiNeedsRestart) {
		wifiConnectionTryInProgress = true;
		bool wifiAccessIncomplete = false;
		// Get credentials from NVS
		String strSSID = gPrefsSettings.getString("SSID", "-1");
		if (!strSSID.compareTo("-1")) {
			Log_Println((char *) FPSTR(ssidNotFoundInNvs), LOGLEVEL_ERROR);
			wifiAccessIncomplete = true;
		}
		String strPassword = gPrefsSettings.getString("Password", "-1");
		if (!strPassword.compareTo("-1")) {
			Log_Println((char *) FPSTR(wifiPwdNotFoundInNvs), LOGLEVEL_ERROR);
			wifiAccessIncomplete = true;
		}

		if (wifiAccessIncomplete) {
			accessPointStart((char *) FPSTR(accessPointNetworkSSID), apIP, apNetmask);
			wifiInit = false;
			wifiConnectionTryInProgress = false;
			return;
		}
		_ssid = x_strdup(strSSID.c_str());
		_pwd = x_strdup(strPassword.c_str());

		// The use of dynamic allocation is recommended to save memory and reduce resources usage.
		// However, the dynamic performs slightly slower than the static allocation.
		// Use static allocation if you want to have more performance and if your application is multi-tasking.
		// Arduino 2.0.x only, outcomment to use static buffers.
		//WiFi.useStaticBuffers(true);

		// set to station mode
		WiFi.mode(WIFI_STA);
		// Get (optional) hostname-configuration from NVS
		hostname = gPrefsSettings.getString("Hostname", "-1");
		if (hostname.compareTo("-1")) {
			//WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
			WiFi.setHostname(hostname.c_str());
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredHostnameFromNvs), hostname.c_str());
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
		} else {
			Log_Println((char *) FPSTR(wifiHostnameNotSet), LOGLEVEL_INFO);
		}

		// Add configuration of static IP (if requested)
		#ifdef STATIC_IP_ENABLE
			snprintf(Log_Buffer, Log_BufferLength, "%s", (char *) FPSTR(tryStaticIpConfig));
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			if (!WiFi.config(IPAddress(LOCAL_IP), IPAddress(GATEWAY_IP), IPAddress(SUBNET_IP), IPAddress(DNS_IP))) {
				snprintf(Log_Buffer, Log_BufferLength, "%s", (char *) FPSTR(staticIPConfigFailed));
				Log_Println(Log_Buffer, LOGLEVEL_ERROR);
			}
		#endif

		// Try to join local WiFi. If not successful, an access-point is opened.
		WiFi.begin(_ssid, _pwd);
		#if (LANGUAGE == DE)
			snprintf(Log_Buffer, Log_BufferLength, "Versuche mit WLAN '%s' zu verbinden...", _ssid);
		#else
			snprintf(Log_Buffer, Log_BufferLength, "Try to connect to WiFi with SSID '%s'...", _ssid);
		#endif
		Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
		wifiCheckLastTimestamp = millis();
		wifiConnectIteration = 1;   // 1: First try; 2: Retry; 0: inactive
		wifiInit = false;
		wifiNeedsRestart = false;
	}

	if (wifiConnectIteration > 0 && (millis() - wifiCheckLastTimestamp >= 500)) {
		if (WiFi.status() != WL_CONNECTED && wifiConnectIteration == 1 && (millis() - wifiCheckLastTimestamp >= 4500)) {
			snprintf(Log_Buffer, Log_BufferLength, "%s", (char *) FPSTR(cantConnectToWifi));
			Log_Println(Log_Buffer, LOGLEVEL_ERROR);
			WiFi.disconnect(true, true);
			WiFi.begin(_ssid, _pwd); // ESP32-workaround (otherwise WiFi-connection fails every 2nd time)
			wifiConnectIteration = 2;
			return;
		}

		if (WiFi.status() != WL_CONNECTED && wifiConnectIteration == 2 && (millis() - wifiCheckLastTimestamp >= 9000)) {
			wifiConnectIteration = 0;
			accessPointStart((char *) FPSTR(accessPointNetworkSSID), apIP, apNetmask);
			wifiConnectionTryInProgress = false;
			free(_ssid);
			free(_pwd);
			return;
		}

		if (WiFi.status() == WL_CONNECTED) {
			wifiConnectionTryInProgress = false;
			wifiConnectIteration = 0;
			IPAddress myIP = WiFi.localIP();
			#if (LANGUAGE == DE)
				snprintf(Log_Buffer, Log_BufferLength, "Aktuelle IP: %d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
			#else
				snprintf(Log_Buffer, Log_BufferLength, "Current IP: %d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
			#endif
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			// get current time and date
			Log_Println((char *) FPSTR(syncingViaNtp), LOGLEVEL_NOTICE);
			// timezone: Berlin
			long gmtOffset_sec = 3600;
			int daylightOffset_sec = 3600;
			configTime(gmtOffset_sec, daylightOffset_sec, "de.pool.ntp.org", "0.pool.ntp.org", "ptbtime1.ptb.de");
			#ifdef MDNS_ENABLE
				// zero conf, make device available as <hostname>.local
				if (MDNS.begin(hostname.c_str())) {
					MDNS.addService("http", "tcp", 80);
				}
			#endif
			free(_ssid);
			free(_pwd);
		}
	}

	if (Wlan_IsConnected()) {
		if (millis() - lastPrintRssiTimestamp >= 60000) {
			lastPrintRssiTimestamp = millis();
			snprintf(Log_Buffer, Log_BufferLength, "RSSI: %d dBm", Wlan_GetRssi());
			Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
		}
	}
}

bool Wlan_ConnectionTryInProgress(void) {
	return wifiConnectionTryInProgress;
}

void Wlan_ToggleEnable(void) {
	writeWifiStatusToNVS(!getWifiEnableStatusFromNVS());
}

String Wlan_GetIpAddress(void) {
	return WiFi.localIP().toString();
}

int8_t Wlan_GetRssi(void) {
	return WiFi.RSSI();
}

// Initialize soft access-point
void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask) {
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(ip, ip, netmask);
	WiFi.softAP(SSID);
	delay(500);

	Log_Println((char *) FPSTR(apReady), LOGLEVEL_NOTICE);
	snprintf(Log_Buffer, Log_BufferLength, "IP-Adresse: %d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

	Web_Init();

	accessPointStarted = true;
}

// Reads stored WiFi-status from NVS
bool getWifiEnableStatusFromNVS(void) {
	uint32_t wifiStatus = gPrefsSettings.getUInt("enableWifi", 99);

	// if not set so far, preseed with 1 (enable)
	if (wifiStatus == 99) {
		gPrefsSettings.putUInt("enableWifi", 1);
		wifiStatus = 1;
	}

	return wifiStatus;
}

// Writes to NVS whether WiFi should be activated
void writeWifiStatusToNVS(bool wifiStatus) {
	if (!wifiStatus) {
		if (gPrefsSettings.putUInt("enableWifi", 0)) { // disable
			Log_Println((char *) FPSTR(wifiDisabledAfterRestart), LOGLEVEL_NOTICE);
			if (gPlayProperties.isWebstream) {
				AudioPlayer_TrackControlToQueueSender(STOP);
			}
			delay(300);
			WiFi.mode(WIFI_OFF);
			wifiEnabled = false;
		}
	} else {
		if (gPrefsSettings.putUInt("enableWifi", 1)) { // enable
			Log_Println((char *) FPSTR(wifiEnabledAfterRestart), LOGLEVEL_NOTICE);
			wifiNeedsRestart = true;
			wifiEnabled = true;
		}
	}
}

bool Wlan_IsConnected(void) {
	return (WiFi.status() == WL_CONNECTED);
}
