#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "settings.h"
#include "AudioPlayer.h"
#include "RotaryEncoder.h"
#include "Log.h"
#include "System.h"
#include "Web.h"

// Don't change anything here unless you know what you're doing
// HELPER //
// WiFi
unsigned long wifiCheckLastTimestamp = 0;
bool wifiEnabled; // Current status if wifi is enabled
uint32_t wifiStatusToggledTimestamp = 0;
bool wifiNeedsRestart = false;

// AP-WiFi
IPAddress apIP(192, 168, 4, 1);        // Access-point's static IP
IPAddress apNetmask(255, 255, 255, 0); // Access-point's netmask
bool accessPointStarted = false;

void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask);
bool getWifiEnableStatusFromNVS(void);
void writeWifiStatusToNVS(bool wifiStatus);
bool Wlan_IsConnected(void);
int8_t Wlan_GetRssi(void);

uint32_t lastPrintRssiTimestamp = 0;

void Wlan_Init(void) {
    wifiEnabled = getWifiEnableStatusFromNVS();
}

void Wlan_Cyclic(void) {
    // If wifi would not be activated, return instantly
    if (!wifiEnabled) {
        return;
    }

    if (!wifiCheckLastTimestamp || wifiNeedsRestart) {
        // Get credentials from NVS
        String strSSID = gPrefsSettings.getString("SSID", "-1");
        if (!strSSID.compareTo("-1")) {
            Log_Println((char *) FPSTR(ssidNotFoundInNvs), LOGLEVEL_ERROR);
        }
        String strPassword = gPrefsSettings.getString("Password", "-1");
        if (!strPassword.compareTo("-1")) {
            Log_Println((char *) FPSTR(wifiPwdNotFoundInNvs), LOGLEVEL_ERROR);
        }
        const char *_ssid = strSSID.c_str();
        const char *_pwd = strPassword.c_str();

        // Get (optional) hostname-configration from NVS
        String hostname = gPrefsSettings.getString("Hostname", "-1");
        if (hostname.compareTo("-1")) {
            //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
            WiFi.setHostname(hostname.c_str());
            snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(restoredHostnameFromNvs), hostname.c_str());
            Log_Println(Log_Buffer, LOGLEVEL_INFO);
        } else {
            Log_Println((char *) FPSTR(wifiHostnameNotSet), LOGLEVEL_INFO);
        }

        // Add configration of static IP (if requested)
        #ifdef STATIC_IP_ENABLE
            snprintf(Log_Buffer, Log_BufferLength, "%s", (char *) FPSTR(tryStaticIpConfig));
            Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
            if (!WiFi.config(IPAddress(LOCAL_IP), IPAddress(GATEWAY_IP), IPAddress(SUBNET_IP), IPAddress(DNS_IP))) {
                snprintf(Log_Buffer, Log_BufferLength, "%s", (char *) FPSTR(staticIPConfigFailed));
                Log_Println(Log_Buffer, LOGLEVEL_ERROR);
            }
        #endif

        // Try to join local WiFi. If not successful, an access-point is opened
        WiFi.begin(_ssid, _pwd);

        uint8_t tryCount = 0;
        while (WiFi.status() != WL_CONNECTED && tryCount <= 12) {
            delay(500);
            Serial.print(F("."));
            tryCount++;
            wifiCheckLastTimestamp = millis();
            if (tryCount >= 4 && WiFi.status() == WL_CONNECT_FAILED) {
                WiFi.begin(_ssid, _pwd); // ESP32-workaround (otherwise WiFi-connection sometimes fails)
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
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
            } else {
			// Starts AP if WiFi-connect wasn't successful
            accessPointStart((char *) FPSTR(accessPointNetworkSSID), apIP, apNetmask);
        }

        #ifdef MDNS_ENABLE
            // zero conf, make device available as <hostname>.local
            if (MDNS.begin(hostname.c_str())) {
                MDNS.addService("http", "tcp", 80);
            }
        #endif

        wifiNeedsRestart = false;
    }

    if (Wlan_IsConnected()) {
        if (millis() - lastPrintRssiTimestamp >= 60000) {
            lastPrintRssiTimestamp = millis();
            snprintf(Log_Buffer, Log_BufferLength, "RSSI: %d dBm", Wlan_GetRssi());
            Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
        }
    }
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
