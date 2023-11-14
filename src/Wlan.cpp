#include <Arduino.h>
#include "settings.h"

#include "Wlan.h"

#include "AudioPlayer.h"
#include "Log.h"
#include "MemX.h"
#include "RotaryEncoder.h"
#include "System.h"
#include "Web.h"
#include "esp_sntp.h"
#include "main.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#define WIFI_STATE_INIT			0u
#define WIFI_STATE_CONNECT_LAST 1u
#define WIFI_STATE_SCAN_CONN	2u
#define WIFI_STATE_CONN_SUCCESS 3u
#define WIFI_STATE_CONNECTED	4u
#define WIFI_STATE_DISCONNECTED 5u
#define WIFI_STATE_CONN_FAILED	6u
#define WIFI_STATE_AP			7u
#define WIFI_STATE_END			8u

uint8_t wifiState = WIFI_STATE_INIT;

#define RECONNECT_INTERVAL 600000

// AP-WiFi
IPAddress apIP(192, 168, 4, 1); // Access-point's static IP
IPAddress apNetmask(255, 255, 255, 0); // Access-point's netmask

bool wifiEnabled; // Current status if wifi is enabled

void accessPointStart(const char *SSID, const char *password, IPAddress ip, IPAddress netmask);
bool getWifiEnableStatusFromNVS(void);
void writeWifiStatusToNVS(bool wifiStatus);

void handleWifiStateInit();

// state for connection attempt
static uint8_t scanIndex = 0;
static uint8_t connectionAttemptCounter = 0;
static unsigned long connectStartTimestamp = 0;
static uint32_t connectionFailedTimestamp = 0;

// state for persistent settings
static const char *nvsWiFiNetworksKey = "SAVED_WIFIS";
static constexpr size_t maxSavedNetworks = 10;
static size_t numKnownNetworks = 0;
static WiFiSettings knownNetworks[maxSavedNetworks];
static String hostname;

// state for AP
DNSServer *dnsServer;
constexpr uint8_t DNS_PORT = 53;

void Wlan_Init(void) {
	wifiEnabled = getWifiEnableStatusFromNVS();

	hostname = gPrefsSettings.getString("Hostname");
	// Get (optional) hostname-configuration from NVS
	if (hostname) {
		Log_Printf(LOGLEVEL_INFO, restoredHostnameFromNvs, hostname.c_str());
	} else {
		Log_Println(wifiHostnameNotSet, LOGLEVEL_INFO);
	}

	// load array of up to maxSavedNetworks from NVS
	numKnownNetworks = gPrefsSettings.getBytes(nvsWiFiNetworksKey, knownNetworks, maxSavedNetworks * sizeof(WiFiSettings)) / sizeof(WiFiSettings);
	for (int i = 0; i < numKnownNetworks; i++) {
		knownNetworks[i].ssid[32] = '\0';
		knownNetworks[i].password[64] = '\0';
		Log_Printf(LOGLEVEL_NOTICE, wifiNetworkLoaded, i, knownNetworks[i].ssid);
	}

	// ******************* MIGRATION *******************
	// migration from single-wifi setup. Delete some time in the future
	if (gPrefsSettings.isKey("SSID") && gPrefsSettings.isKey("Password")) {
		String strSSID = gPrefsSettings.getString("SSID", "");
		String strPassword = gPrefsSettings.getString("Password", "");
		Log_Println("migrating from old wifi NVS settings!", LOGLEVEL_NOTICE);
		gPrefsSettings.putString("LAST_SSID", strSSID);

		struct WiFiSettings networkSettings;

		strncpy(networkSettings.ssid, strSSID.c_str(), 32);
		networkSettings.ssid[32] = '\0';
		strncpy(networkSettings.password, strPassword.c_str(), 64);
		networkSettings.password[64] = '\0';
		networkSettings.use_static_ip = false;

#ifdef STATIC_IP_ENABLE
		networkSettings.static_addr = (uint32_t) IPAddress(LOCAL_IP);
		networkSettings.static_gateway = (uint32_t) IPAddress(GATEWAY_IP);
		networkSettings.static_subnet = (uint32_t) IPAddress(SUBNET_IP);
		networkSettings.static_dns1 = (uint32_t) IPAddress(DNS_IP);
		networkSettings.use_static_ip = true;
#endif

		Wlan_AddNetworkSettings(networkSettings);
		// clean up old values from nvs
		gPrefsSettings.remove("SSID");
		gPrefsSettings.remove("Password");
	}

	// ******************* MIGRATION *******************

	if (OPMODE_NORMAL != System_GetOperationMode()) {
		wifiState = WIFI_STATE_END;
		return;
	}

	// The use of dynamic allocation is recommended to save memory and reduce resources usage.
	// However, the dynamic performs slightly slower than the static allocation.
	// Use static allocation if you want to have more performance and if your application is multi-tasking.
	// Arduino 2.0.x only, comment to use dynamic buffers.

	// for Arduino 2.0.9 this does not seem to bring any advantage just more memory use, so leave it outcommented
	// WiFi.useStaticBuffers(true);

	wifiState = WIFI_STATE_INIT;
	handleWifiStateInit();
}

void connectToKnownNetwork(WiFiSettings settings, byte *bssid = nullptr) {
	// set hostname on connect, because when resetting wifi config elsewhere it could be reset
	if (hostname.compareTo("-1")) {
		WiFi.setHostname(hostname.c_str());
	}

	if (settings.use_static_ip) {
		Log_Println(tryStaticIpConfig, LOGLEVEL_NOTICE);
		if (!WiFi.config(
				IPAddress(settings.static_addr),
				IPAddress(settings.static_gateway),
				IPAddress(settings.static_subnet),
				IPAddress(settings.static_dns1),
				IPAddress(settings.static_dns2))) {
			Log_Println(staticIPConfigFailed, LOGLEVEL_ERROR);
		}
	}

	Log_Printf(LOGLEVEL_NOTICE, wifiConnectionInProgress, settings.ssid);

	WiFi.begin(settings.ssid, settings.password, 0, bssid);
}

void handleWifiStateInit() {
	if (!wifiEnabled) {
		wifiState = WIFI_STATE_END;
		return;
	}

	WiFi.mode(WIFI_STA);

	scanIndex = 0;
	connectionAttemptCounter = 0;
	connectStartTimestamp = 0;
	connectionFailedTimestamp = 0;
	bool scanWiFiOnStart = gPrefsSettings.getBool("ScanWiFiOnStart", false);
	if (scanWiFiOnStart) {
		// perform a scan to find the strongest network with same ssid (e.g. for mesh/repeater networks)
		WiFi.scanNetworks(true, false, true, 120);
		wifiState = WIFI_STATE_SCAN_CONN;
	} else {
		// quick connect without additional scan
		wifiState = WIFI_STATE_CONNECT_LAST;
	}
}

void handleWifiStateConnectLast() {
	if (WiFi.status() == WL_CONNECTED) {
		wifiState = WIFI_STATE_CONN_SUCCESS;
		return;
	}

	if (connectStartTimestamp > 0 && millis() - connectStartTimestamp < 3000) {
		return;
	}

	WiFi.disconnect(true, true);
	WiFi.mode(WIFI_STA);

	// for speed, try to connect to last ssid first
	String lastSSID = gPrefsSettings.getString("LAST_SSID");

	std::optional<WiFiSettings> lastSettings = std::nullopt;

	if (lastSSID) {
		for (int i = 0; i < numKnownNetworks; i++) {
			if (strncmp(knownNetworks[i].ssid, lastSSID.c_str(), 32) == 0) {
				lastSettings = knownNetworks[i];
				break;
			}
		}
	}

	if (!lastSettings || connectionAttemptCounter > 1) {
		// you can tweak passive/active mode and time per channel
		// routers send a beacon msg every 100ms and passive mode with 120ms works well and is fastest here
		WiFi.scanNetworks(true, false, true, 120);

		connectionAttemptCounter = 0;
		connectStartTimestamp = 0;
		wifiState = WIFI_STATE_SCAN_CONN;
		return;
	}

	connectStartTimestamp = millis();
	connectToKnownNetwork(lastSettings.value());
	connectionAttemptCounter++;
}

void handleWifiStateScanConnect() {
	// wait for scan results and handle them

	if (WiFi.status() == WL_CONNECTED) {
		WiFi.scanDelete();
		wifiState = WIFI_STATE_CONN_SUCCESS;
		return;
	}

	int wifiScanCompleteResult = WiFi.scanComplete();

	switch (wifiScanCompleteResult) {
		case -1:
			// scan not fin
			return;
		case -2:
			// scan not triggered
			wifiState = WIFI_STATE_CONN_FAILED;
			return;
		case 0:
			wifiState = WIFI_STATE_CONN_FAILED;
			return;
	}

	if (connectStartTimestamp == 0) {
		for (int i = 0; i < wifiScanCompleteResult; ++i) {
			Log_Printf(LOGLEVEL_NOTICE, wifiScanResult, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), WiFi.BSSIDstr(i).c_str());
		}
	} else {
		if (millis() - connectStartTimestamp < 5000) {
			return;
		}
	}

	WiFi.disconnect(true, true);
	WiFi.mode(WIFI_STA);
	// fortunately, scan results are already sorted by best signal
	for (int i = scanIndex; i < wifiScanCompleteResult; i++) {
		// try to connect to wifi network with index i
		String issid = WiFi.SSID(i);
		byte *bssid = WiFi.BSSID(i);
		// check if ssid name matches any saved ssid
		for (int j = 0; j < numKnownNetworks; j++) {
			if (strncmp(issid.c_str(), knownNetworks[j].ssid, 32) == 0) {
				connectToKnownNetwork(knownNetworks[j], bssid);

				connectStartTimestamp = millis();

				// prepare for next iteration
				if (connectionAttemptCounter > 0) {
					scanIndex = i + 1;
					connectionAttemptCounter = 0;
				} else {
					scanIndex = i;
					connectionAttemptCounter++;
				}

				return;
			}
		}
	}

	WiFi.scanDelete();
	wifiState = WIFI_STATE_CONN_FAILED;
}

// Callback function (get's called when time adjusts via NTP)
void ntpTimeAvailable(struct timeval *t) {
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) {
		Log_Println(ntpFailed, LOGLEVEL_NOTICE);
		return;
	}
	static char timeStringBuff[255];
	snprintf(timeStringBuff, sizeof(timeStringBuff), ntpGotTime, timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
	Log_Println(timeStringBuff, LOGLEVEL_NOTICE);
	// set ESPuino's very first start date
	if (!gPrefsSettings.isKey("firstStart")) {
		gPrefsSettings.putULong("firstStart", t->tv_sec);
	}
}

static bool initialStart = true;
// executed once after successfully connecting
void handleWifiStateConnectionSuccess() {
	initialStart = false;
	IPAddress myIP = WiFi.localIP();
	String mySSID = Wlan_GetCurrentSSID();

	Log_Printf(LOGLEVEL_NOTICE, wifiConnectionSuccess, mySSID.c_str(), WiFi.RSSI(), WiFi.channel(), WiFi.BSSIDstr().c_str());
	Log_Printf(LOGLEVEL_NOTICE, wifiCurrentIp, myIP.toString().c_str());

	if (!gPrefsSettings.getString("LAST_SSID").equals(mySSID)) {
		Log_Printf(LOGLEVEL_INFO, wifiSetLastSSID, mySSID.c_str());
		gPrefsSettings.putString("LAST_SSID", mySSID);
	}

	// get current time and date
	Log_Println(syncingViaNtp, LOGLEVEL_NOTICE);
	// Updates system time immediately upon receiving a response from the SNTP server
	sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
	// set notification call-back function
	sntp_set_time_sync_notification_cb(ntpTimeAvailable);
	// start NTP request with timezone
	configTzTime(timeZone, "de.pool.ntp.org", "0.pool.ntp.org", "ptbtime1.ptb.de");
#ifdef MDNS_ENABLE
	// zero conf, make device available as <hostname>.local
	if (MDNS.begin(hostname.c_str())) {
		MDNS.addService("http", "tcp", 80);
		Log_Printf(LOGLEVEL_NOTICE, mDNSStarted, hostname.c_str());
	} else {
		Log_Printf(LOGLEVEL_ERROR, mDNSFailed, hostname.c_str());
	}
#endif
	delete dnsServer;
	dnsServer = nullptr;

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
	if (gPlayLastRfIdWhenWiFiConnected && gTriedToConnectToHost) {
		gPlayLastRfIdWhenWiFiConnected = false;
		recoverLastRfidPlayedFromNvs(true);
	}
#endif

	wifiState = WIFI_STATE_CONNECTED;
}

static uint32_t lastPrintRssiTimestamp = 0;
static int8_t lastRssiValue = 0;
void handleWifiStateConnected() {
	switch (WiFi.status()) {
		case WL_CONNECTED:
			break;
		case WL_NO_SSID_AVAIL:
			// is set if reconnect failed and network is not found
			wifiState = WIFI_STATE_DISCONNECTED;
			return;
		case WL_DISCONNECTED:
			// is set if reconnect failed for other reason
			wifiState = WIFI_STATE_DISCONNECTED;
			return;
		default:
			break;
	}

	if (millis() - lastPrintRssiTimestamp >= 60000) {
		lastPrintRssiTimestamp = millis();
		// show RSSI value only if it has changed by > 3 dBm
		if (abs(lastRssiValue - Wlan_GetRssi()) > 3) {
			Log_Printf(LOGLEVEL_DEBUG, "RSSI: %d dBm", Wlan_GetRssi());
			lastRssiValue = Wlan_GetRssi();
		}
	}
}

static uint32_t wifiAPStartedTimestamp = 0;
void handleWifiStateConnectionFailed() {
	// good candidate for a user setting
	static constexpr uint32_t wifiReconnectTimeout = 600000;

	if (connectionFailedTimestamp == 0) {
		Log_Println(cantConnectToWifi, LOGLEVEL_INFO);
		connectionFailedTimestamp = millis();
	}

	if (initialStart) {
		initialStart = false;
		accessPointStart(accessPointNetworkSSID, accessPointNetworkPassword, apIP, apNetmask);
		wifiAPStartedTimestamp = millis();
		wifiState = WIFI_STATE_AP;
		return;
	}

	// every 600s, try connecting again
	if (millis() - connectionFailedTimestamp > wifiReconnectTimeout) {
		wifiState = WIFI_STATE_INIT;
		return;
	}
}

void handleWifiStateAP() {
	// good candidate for a user setting
	static constexpr uint32_t closeWifiAPTimeout = 300000;

	// close the AP after the desired time has passed; set to 0 to keep on forever
	if (closeWifiAPTimeout != 0 && millis() - wifiAPStartedTimestamp > closeWifiAPTimeout) {
		WiFi.mode(WIFI_OFF);
		wifiState = WIFI_STATE_DISCONNECTED;
		return;
	}

	dnsServer->processNextRequest();
}

void Wlan_Cyclic(void) {
	switch (wifiState) {
		case WIFI_STATE_INIT:
			handleWifiStateInit();
			return;
		case WIFI_STATE_CONNECT_LAST:
			handleWifiStateConnectLast();
			return;
		case WIFI_STATE_SCAN_CONN:
			handleWifiStateScanConnect();
			return;
		case WIFI_STATE_CONN_SUCCESS:
			handleWifiStateConnectionSuccess();
			return;
		case WIFI_STATE_CONNECTED:
			handleWifiStateConnected();
			return;
		case WIFI_STATE_DISCONNECTED:
			wifiState = WIFI_STATE_INIT;
			return;
		case WIFI_STATE_CONN_FAILED:
			handleWifiStateConnectionFailed();
			return;
		case WIFI_STATE_AP:
			handleWifiStateAP();
			return;
		case WIFI_STATE_END:
			WiFi.disconnect(true, true);
			WiFi.mode(WIFI_OFF);
			return;
	}
}

bool Wlan_ValidateHostname(String newHostname) {
	size_t len = newHostname.length();
	const char *hostname = newHostname.c_str();

	// validation: first char alphanumerical, then alphanumerical or '-', last char alphanumerical
	// These rules are mainly for mDNS purposes, a "pretty" hostname could have far fewer restrictions
	bool validated = true;
	if (len < 2 || len > 32) {
		validated = false;
	}

	if (!isAlphaNumeric(hostname[0]) || !isAlphaNumeric(hostname[len - 1])) {
		validated = false;
	}

	for (int i = 0; i < len; i++) {
		if (!isAlphaNumeric(hostname[i]) && hostname[i] != '-') {
			validated = false;
			break;
		}
	}

	return validated;
}

bool Wlan_SetHostname(String newHostname) {
	// hostname should just be applied after reboot
	gPrefsSettings.putString("Hostname", newHostname);
	// check if hostname is written
	return (gPrefsSettings.getString("Hostname", "-1") == newHostname);
}

bool Wlan_AddNetworkSettings(WiFiSettings settings) {
	settings.ssid[32] = '\0';
	settings.password[64] = '\0';

	for (uint8_t i = 0; i < numKnownNetworks; i++) {
		if (strncmp(settings.ssid, knownNetworks[i].ssid, 32) == 0) {
			Log_Printf(LOGLEVEL_NOTICE, wifiUpdateNetwork, settings.ssid);
			knownNetworks[i] = settings;
			gPrefsSettings.putBytes(nvsWiFiNetworksKey, knownNetworks, numKnownNetworks * sizeof(WiFiSettings));
			return true;
		}
	}

	if (numKnownNetworks >= maxSavedNetworks) {
		Log_Println(wifiAddTooManyNetworks, LOGLEVEL_ERROR);
		return false;
	}

	Log_Printf(LOGLEVEL_NOTICE, wifiAddNetwork, settings.ssid);

	knownNetworks[numKnownNetworks] = settings;
	numKnownNetworks += 1;

	gPrefsSettings.putBytes(nvsWiFiNetworksKey, knownNetworks, numKnownNetworks * sizeof(WiFiSettings));

	return true;
}

uint8_t Wlan_NumSavedNetworks() {
	return numKnownNetworks;
}

void Wlan_GetSavedNetworks(std::function<void(const WiFiSettings &)> handler) {
	for (uint8_t i = 0; i < numKnownNetworks; i++) {
		handler(knownNetworks[i]);
	}
}

const String Wlan_GetCurrentSSID() {
	return WiFi.SSID();
}

const String Wlan_GetHostname() {
	return gPrefsSettings.getString("Hostname", "ESPuino");
}

bool Wlan_DeleteNetwork(String ssid) {
	Log_Printf(LOGLEVEL_NOTICE, wifiDeleteNetwork, ssid.c_str());

	for (uint8_t i = 0; i < numKnownNetworks; i++) {
		if (strncmp(ssid.c_str(), knownNetworks[i].ssid, 32) == 0) {

			// delete and move all following elements to the left
			std::copy(&knownNetworks[i + 1], &knownNetworks[numKnownNetworks], &knownNetworks[i]);
			numKnownNetworks--;

			size_t new_length = numKnownNetworks * sizeof(WiFiSettings);

			if (new_length == 0) {
				gPrefsSettings.remove(nvsWiFiNetworksKey);
			} else {
				gPrefsSettings.putBytes(nvsWiFiNetworksKey, knownNetworks, new_length);
			}

			return true;
		}
	}
	// ssid not found
	return false;
}

bool Wlan_ConnectionTryInProgress(void) {
	return wifiState == WIFI_STATE_SCAN_CONN;
}

String Wlan_GetIpAddress(void) {
	return WiFi.localIP().toString();
}

int8_t Wlan_GetRssi(void) {
	return WiFi.RSSI();
}

// Initialize soft access-point
void accessPointStart(const char *SSID, const char *password, IPAddress ip, IPAddress netmask) {
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(ip, ip, netmask);
	WiFi.softAP(SSID, (password != NULL && password[0] != '\0') ? password : NULL);
	delay(500);

	Log_Println(apReady, LOGLEVEL_NOTICE);
	Log_Printf(LOGLEVEL_NOTICE, "IP-Adresse: %s", apIP.toString().c_str());

	if (!dnsServer) {
		dnsServer = new DNSServer();
	}

	dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer->start(DNS_PORT, "*", ip);
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

void Wlan_ToggleEnable(void) {
	writeWifiStatusToNVS(!wifiEnabled);
}

// Writes to NVS whether WiFi should be activated
void writeWifiStatusToNVS(bool wifiStatus) {
	wifiEnabled = wifiStatus;

	gPrefsSettings.putUInt("enableWifi", wifiEnabled ? 1 : 0);

	if (wifiEnabled) {
		Log_Println(wifiEnabledMsg, LOGLEVEL_NOTICE);
	} else {
		Log_Println(wifiDisabledMsg, LOGLEVEL_NOTICE);
		if (gPlayProperties.isWebstream) {
			AudioPlayer_TrackControlToQueueSender(STOP);
		}
	}

	// go to init state again to handle new 'wifiEnabled'
	wifiState = WIFI_STATE_INIT;
}

bool Wlan_IsConnected(void) {
	return (wifiState == WIFI_STATE_CONNECTED);
}
