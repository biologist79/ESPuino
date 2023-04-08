#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include "settings.h"
#include "AudioPlayer.h"
#include "RotaryEncoder.h"
#include "Log.h"
#include "System.h"
#include "Web.h"
#include "MemX.h"
#include "main.h"
#include "Wlan.h"

#define WIFI_STATE_INIT 		0u
#define WIFI_STATE_CONNECT_LAST 1u
#define WIFI_STATE_SCAN_CONN	2u
#define WIFI_STATE_CONN_SUCCESS 3u
#define WIFI_STATE_CONNECTED	4u
#define WIFI_STATE_DISCONNECTED	5u
#define WIFI_STATE_CONN_FAILED	6u
#define WIFI_STATE_AP	 		7u
#define WIFI_STATE_END	 		8u

uint8_t wifiState = WIFI_STATE_INIT;

#define RECONNECT_INTERVAL 600000

// AP-WiFi
IPAddress apIP(192, 168, 4, 1);        // Access-point's static IP
IPAddress apNetmask(255, 255, 255, 0); // Access-point's netmask

bool wifiEnabled; // Current status if wifi is enabled

void accessPointStart(const char *SSID, const char *password, IPAddress ip, IPAddress netmask);
bool getWifiEnableStatusFromNVS(void);
void writeWifiStatusToNVS(bool wifiStatus);

uint8_t numSavedNetworks = 0;

void handleWifiStateInit();

// state for connection attempt
uint8_t scanIndex = 0;
uint8_t connectionAttemptCounter = 0;
unsigned long connectStartTimestamp = 0;
uint32_t connectionFailedTimestamp = 0;
String knownNetworks[10];
String hostname;

// state for AP
DNSServer *dnsServer;
constexpr uint8_t DNS_PORT = 53;

void Wlan_Init(void) {
	wifiEnabled = getWifiEnableStatusFromNVS();
	numSavedNetworks = gPrefsSettings.getUChar("NUM_NETWORKS", 0U);

	hostname = gPrefsSettings.getString("Hostname", "-1");
	// Get (optional) hostname-configuration from NVS
	if (hostname.compareTo("-1")) {
		Log_Printf(LOGLEVEL_INFO, restoredHostnameFromNvs, hostname.c_str());
	} else {
		Log_Println((char *) FPSTR(wifiHostnameNotSet), LOGLEVEL_INFO);
	}

	// ******************* MIGRATION *******************
	// migration from single-wifi setup. Delete some time in the future
	String strSSID = gPrefsSettings.getString("SSID", "-1");
	String strPassword = gPrefsSettings.getString("Password", "-1");
	if (!strSSID.equals("-1") && !strPassword.equals("-1")) {
		Log_Println("migrating from old wifi NVS settings!", LOGLEVEL_NOTICE);
		gPrefsSettings.putString("LAST_SSID", strSSID);
		Wlan_AddNetworkSettings(strSSID, strPassword);
	}
	// clean up old values from nvs
	if (!strSSID.equals("-1") || !strPassword.equals("-1")) {
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
	WiFi.useStaticBuffers(true);

	wifiState = WIFI_STATE_INIT;
	handleWifiStateInit();
}

void connectToKnownNetwork(String ssid) {
	String pwd = Wlan_GetPwd(ssid);

	// set hostname on connect, because when resetting wifi config elsewhere it could be reset
	if (hostname.compareTo("-1")) {
		WiFi.setHostname(hostname.c_str());
	}

	Log_Printf(LOGLEVEL_NOTICE, wifiConnectionInProgress, ssid.c_str());

	WiFi.begin(ssid.c_str(), pwd.c_str());
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
	numSavedNetworks = Wlan_GetSSIDs(knownNetworks);
	connectionFailedTimestamp = 0;
	wifiState = WIFI_STATE_CONNECT_LAST;
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
	String lastSSID = gPrefsSettings.getString("LAST_SSID", "-1");

	if(lastSSID.equals("-1") || connectionAttemptCounter > 1) {
		// you can tweak passive/active mode and time per channel
		// routers send a beacon msg every 100ms and passive mode with 120ms works well and is fastest here
		WiFi.scanNetworks(true, false, true, 120);

		connectionAttemptCounter = 0;
		connectStartTimestamp = 0;
		wifiState = WIFI_STATE_SCAN_CONN;
		return;
	}
	
	connectStartTimestamp = millis();
	connectToKnownNetwork(lastSSID);
	connectionAttemptCounter++;
}

void handleWifiStateScanConnect() {
	// wait for scan results and handle them

	if (WiFi.status() == WL_CONNECTED) {
		wifiState = WIFI_STATE_CONN_SUCCESS;
		return;
	}

	int n = WiFi.scanComplete();

	switch (n) {
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
		for (int i = 0; i < n; ++i) {
			Log_Printf(LOGLEVEL_NOTICE, wifiScanResult, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
		}
	} else {		
		if (millis() - connectStartTimestamp < 5000) {
			return;
		}
	}
	
	// TODO: now that we manage multiple networks we might want to configure a static ip for each of them
	// Add configuration of static IP (if requested)
	#ifdef STATIC_IP_ENABLE
		Log_Println(tryStaticIpConfig, LOGLEVEL_NOTICE);
		if (!WiFi.config(IPAddress(LOCAL_IP), IPAddress(GATEWAY_IP), IPAddress(SUBNET_IP), IPAddress(DNS_IP))) {
			Log_Println(staticIPConfigFailed, LOGLEVEL_ERROR);
		}
	#endif

	WiFi.disconnect(true, true);
	WiFi.mode(WIFI_STA);
	// fortunately, scan results are already sorted by best signal
	for (int i = scanIndex; i < n; i++) {
		// try to connect to wifi network with index i
		String issid = WiFi.SSID(i);
		// check if ssid name matches any saved ssid
		for (int j = 0; j < numSavedNetworks; j++) {
			if (issid.equals(knownNetworks[j])) {
				String ssid = WiFi.SSID(i);

				connectToKnownNetwork(ssid);
				connectStartTimestamp = millis();

				// prepare for next iteration
				if (connectionAttemptCounter > 0) {
					scanIndex = i+1;
					connectionAttemptCounter = 0;
				} else {
					scanIndex = i;
					connectionAttemptCounter++;
				}

				return;
			}
		}
	}

	wifiState = WIFI_STATE_CONN_FAILED;
}

bool initialStart = true;
// executed once after successfully connecting
void handleWifiStateConnectionSuccess() {
	initialStart = false;
	IPAddress myIP = WiFi.localIP();

	Log_Printf(LOGLEVEL_NOTICE, wifiCurrentIp, myIP.toString().c_str());

	String mySSID = Wlan_GetCurrentSSID();
	if (!gPrefsSettings.getString("LAST_SSID").equals(mySSID)) {
		Log_Printf(LOGLEVEL_INFO, wifiSetLastSSID, mySSID.c_str());
		gPrefsSettings.putString("LAST_SSID", mySSID);
	}

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
	delete dnsServer;
	dnsServer = nullptr;
	
	#ifdef PLAY_LAST_RFID_AFTER_REBOOT
		if (gPlayLastRfIdWhenWiFiConnected && gTriedToConnectToHost ) {
			gPlayLastRfIdWhenWiFiConnected=false;
			recoverLastRfidPlayedFromNvs(true);
		}
	#endif

	wifiState = WIFI_STATE_CONNECTED;
}

uint32_t lastPrintRssiTimestamp = 0;
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
		Log_Printf(LOGLEVEL_DEBUG, "RSSI: %d dBm", Wlan_GetRssi());
	}
}

// good candidate for a user setting
uint32_t wifiReconnectTimeout = 600000;
uint32_t wifiAPStartedTimestamp = 0;
void handleWifiStateConnectionFailed() {
	if (connectionFailedTimestamp == 0) {
		Log_Println((char *) FPSTR(cantConnectToWifi), LOGLEVEL_INFO);
		connectionFailedTimestamp = millis();
	}

	if (initialStart) {
		initialStart = false;
		accessPointStart((char *) FPSTR(accessPointNetworkSSID), (char *) FPSTR(accessPointNetworkPassword), apIP, apNetmask);
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

// good candidate for a user setting
uint32_t closeWifiAPTimeout = 300000;
void handleWifiStateAP() {
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

bool Wlan_SetHostname(String newHostname) {
	// hostname should just be applied after reboot
	gPrefsSettings.putString("Hostname", newHostname);
	return true;
}

char nvs_key[9];
char* getSSIDKey(uint8_t index) { 
	snprintf(nvs_key, 9, "SSID_%d", index);
	return nvs_key;
}
char* getPWDKey(uint8_t index) { 
	snprintf(nvs_key, 9, "WPWD_%d", index);
	return nvs_key;
}

bool Wlan_AddNetworkSettings(String ssid, String pwd) {
	uint8_t len = Wlan_NumSavedNetworks();

	for (uint8_t i=0; i<len; i++) {
		String issid = gPrefsSettings.getString(getSSIDKey(i));

		if (ssid.equals(issid)) {
			Log_Printf(LOGLEVEL_NOTICE, wifiUpdateNetwork, ssid.c_str());
			gPrefsSettings.putString(getPWDKey(i), pwd);
			return true;
		}
	}

	if (len > 9) {
		Log_Println((char *) FPSTR(wifiAddTooManyNetworks), LOGLEVEL_ERROR);
		return false;
	}

	Log_Printf(LOGLEVEL_NOTICE, wifiAddNetwork, ssid.c_str());

	gPrefsSettings.putString(getSSIDKey(len), ssid);
	gPrefsSettings.putString(getPWDKey(len), pwd);

	numSavedNetworks++;
	gPrefsSettings.putUChar("NUM_NETWORKS", numSavedNetworks);

	return true;
}


uint8_t Wlan_NumSavedNetworks() {
	return numSavedNetworks;
}

// write ssids from nvs into the passed array. 
uint8_t Wlan_GetSSIDs(String* ssids) {	

	uint8_t len = Wlan_NumSavedNetworks();
	if (len > 10) {
		Log_Printf(LOGLEVEL_ERROR, wifiTooManyNetworks, len, 10);
		len = 10;
	}
	
	uint8_t i;
	for (i=0; i<len; i++) {
		String ssid = gPrefsSettings.getString(getSSIDKey(i));
		Log_Printf(LOGLEVEL_NOTICE, wifiNetworkLoaded, i, ssid.c_str());

		ssids[i] = ssid;
	}
	return i;
}

String Wlan_GetPwd(String ssid) {
	uint8_t len = Wlan_NumSavedNetworks();

	for (uint8_t i=0; i<len; i++) {
		String issid = gPrefsSettings.getString(getSSIDKey(i));

		if(ssid.equals(issid)) {
			return gPrefsSettings.getString(getPWDKey(i));
		}
	}
	return String();
}

String Wlan_GetCurrentSSID() {
	return WiFi.SSID();
}

String Wlan_GetHostname() {
	return gPrefsSettings.getString("Hostname", "");
}

bool Wlan_DeleteNetwork(String ssid) {
	uint8_t len = numSavedNetworks;
	numSavedNetworks--;
	gPrefsSettings.putUChar("NUM_NETWORKS", numSavedNetworks);
	char* key;

	Log_Printf(LOGLEVEL_NOTICE, wifiDeleteNetwork, ssid.c_str());

	for (uint8_t i=0; i<len-1; i++) {

		String issid = gPrefsSettings.getString(getSSIDKey(i));

		if(ssid.equals(issid)) {
			key = getSSIDKey(len-1);
			String lastSSID = gPrefsSettings.getString(key);
			gPrefsSettings.remove(key);
			key = getPWDKey(len-1);
			String lastPWD = gPrefsSettings.getString(key);
			gPrefsSettings.remove(key);	

			gPrefsSettings.putString(getSSIDKey(i), lastSSID);
			gPrefsSettings.putString(getPWDKey(i), lastPWD);
			return true;
		}
	}
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

	Log_Println((char *) FPSTR(apReady), LOGLEVEL_NOTICE);
	Log_Printf(LOGLEVEL_NOTICE, "IP-Adresse: %s", apIP.toString().c_str());

	if (!dnsServer)
		dnsServer = new DNSServer();

	dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer->start(DNS_PORT, "*", ip);

	Web_Init();
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

	if(wifiEnabled) {
		Log_Println((char *) FPSTR(wifiEnabledMsg), LOGLEVEL_NOTICE);
	} else {
		Log_Println((char *) FPSTR(wifiDisabledMsg), LOGLEVEL_NOTICE);
		if (gPlayProperties.isWebstream) {
			AudioPlayer_TrackControlToQueueSender(STOP);
		}
	}

	// go to init state again to handle new 'wifiEnabled'
	wifiState = WIFI_STATE_INIT;
}

bool Wlan_IsConnected(void) {
	return (WiFi.status() == WL_CONNECTED);
}
