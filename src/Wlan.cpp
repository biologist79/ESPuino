#include <Arduino.h>
#include "settings.h"

#include "Wlan.h"

#include "AudioPlayer.h"
#include "Log.h"
#include "MemX.h"
#include "Mqtt.h"
#include "RotaryEncoder.h"
#include "System.h"
#include "Web.h"
#include "esp_sntp.h"
#include "main.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <list>
#include <nvs.h>

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
const IPAddress apIP(192, 168, 4, 1); // Access-point's static IP
const IPAddress apNetmask(255, 255, 255, 0); // Access-point's netmask

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
static constexpr const char *nvsWiFiNamespace = "wifi-settings";
static constexpr const char *nvsWiFiKey = "wifi-";
static constexpr const char *nvsWiFiKeyFormat = "wifi-%02u";
static Preferences prefsWifiSettings;
static bool prefsWifiSettingsAvailable = false;

struct NvsEntry {
	String key;
	WiFiSettings value;

	NvsEntry(const char *key, WiFiSettings value)
		: key(key)
		, value(value) { }
};

// state for AP
DNSServer *dnsServer;
constexpr uint8_t DNS_PORT = 53;

const String getHostname() {
	return System_SettingsPrefsAvailable() ? gPrefsSettings.getString("Hostname") : String();
}

static constexpr size_t maxWiFiSettingsBlobLength = (1 + WiFiSettings::maxSsidLength + 1)
	+ (1 + WiFiSettings::maxPasswordLength + 1)
	+ (1 + WiFiSettings::StaticIp::numFields * sizeof(uint32_t));

static WiFiSettings makeInvalidWiFiSettings() {
	WiFiSettings settings;
	settings.hasDeserializeError = true;
	return settings;
}

/**
 * @brief Load WiFiSettings from NVS blob
 * @param key The key to load
 * @return WiFiSettings The deserialized settings from the key, invalid if the NVS blob is malformed
 */
static WiFiSettings loadWiFiSettingsFromNvs(const char *key) {
	const size_t blobLen = prefsWifiSettings.getBytesLength(key);
	if (blobLen == 0 || blobLen > maxWiFiSettingsBlobLength) {
		return makeInvalidWiFiSettings();
	}

	std::vector<uint8_t> buffer;
	buffer.resize(blobLen);
	const size_t bytesRead = prefsWifiSettings.getBytes(key, buffer.data(), buffer.size());
	if (bytesRead != buffer.size()) {
		return makeInvalidWiFiSettings();
	}

	return WiFiSettings(buffer);
}
[[maybe_unused]] static WiFiSettings loadWiFiSettingsFromNvs(const String &key) {
	return loadWiFiSettingsFromNvs(key.c_str());
}

/**
 * @brief Write serialized WiFiSettings to an NVS blob
 * @param key The key used to store the data
 * @param s The data to be stored
 * @return true When the data was written successfully
 * @return false When the WiFISettings was inValid or the NVS write failed
 */
static bool storeWiFiSettingsToNvs(const char *key, const WiFiSettings &s) {
	auto buffer = s.serialize();
	if (!buffer) {
		// Invalid WiFiSettings instance
		return false;
	}
	return prefsWifiSettings.putBytes(key, buffer->data(), buffer->size()) == buffer->size();
}
[[maybe_unused]] static bool storeWiFiSettingsToNvs(const String &key, const WiFiSettings &s) {
	return storeWiFiSettingsToNvs(key.c_str(), s);
}

static bool isWiFiSettingsNvsKey(const char *key) {
	return strncmp(key, nvsWiFiKey, strlen(nvsWiFiKey)) == 0;
}

static void iterateNvsEntryKeys(std::function<bool(const char *)> handler) {
#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3))
	nvs_iterator_t it = nullptr;
	esp_err_t res = nvs_entry_find("nvs", nvsWiFiNamespace, NVS_TYPE_BLOB, &it);
	while (res == ESP_OK) {
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);
		// some basic sanity check
		if (isWiFiSettingsNvsKey(info.key) && !handler(info.key)) {
			// handler requested an abort
			nvs_release_iterator(it);
			return;
		}
		res = nvs_entry_next(&it);
	}
#else
	nvs_iterator_t it = nvs_entry_find("nvs", nvsWiFiNamespace, NVS_TYPE_BLOB);
	while (it) {
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);

		// check if we have a wifi setting
		if (isWiFiSettingsNvsKey(info.key) && !handler(info.key)) {
			// handler requested an abort
			nvs_release_iterator(it);
			return;
		}
		it = nvs_entry_next(it);
	}
#endif
}

/**
 * @brief Iterate and execute callback function over all valid WiFiSettings binary entries
 * @param handler The function to be called, it receives the key and the WiFiSettings object loaded from NVS
 */
static void iterateNvsEntries(std::function<bool(const char *, const WiFiSettings &)> handler) {
	iterateNvsEntryKeys([&handler](const char *key) {
		WiFiSettings settings = loadWiFiSettingsFromNvs(key);
		if (!settings.isValid()) {
			Log_Printf(LOGLEVEL_ERROR, "Ignoring invalid WiFi NVS entry: %s", key);
			return true;
		}

		return handler(key, settings);
	});
}

static void deleteInvalidWiFiNvsEntries() {
	std::vector<String> invalidKeys;
	iterateNvsEntryKeys([&invalidKeys](const char *key) {
		if (!loadWiFiSettingsFromNvs(key).isValid()) {
			invalidKeys.push_back(key);
		}
		return true;
	});

	for (const String &key : invalidKeys) {
		Log_Printf(LOGLEVEL_ERROR, "Deleting invalid WiFi NVS entry: %s", key.c_str());
		prefsWifiSettings.remove(key.c_str());
	}
}

/**
 * @brief Search wifi entries and return key & WifiSettings object if found
 * @param ssid The SSID
 * @return std::optional<NvsEntry> A struct with the key and the WifiSettings
 * @return std::nullopt if SSID was not found
 */
static std::optional<NvsEntry> getNvsWifiSettings(const char *ssid) {
	std::optional<NvsEntry> ret = std::nullopt;

	iterateNvsEntries([&ssid, &ret](const char *key, const WiFiSettings &setting) {
		if (setting.isValid() && setting.ssid.equals(ssid)) {
			// we found our key, so break the loop
			ret = NvsEntry(key, setting);
			return false;
		}
		// go on, give us another one...
		return true;
	});
	return ret;
}
static std::optional<NvsEntry> getNvsWifiSettings(const String ssid) {
	return getNvsWifiSettings(ssid.c_str());
}

/// @brief Migrate version 1 WiFi (single network) entries to current version
static void migrateFromVersion1() {
	if (!System_SettingsPrefsAvailable()) {
		return;
	}

	if (gPrefsSettings.isKey("SSID")) {
		const String strSSID = gPrefsSettings.getString("SSID");
		const String strPassword = gPrefsSettings.getString("Password");
		Log_Println("migrating from old wifi NVS settings!", LOGLEVEL_NOTICE);
		gPrefsSettings.putString("LAST_SSID", strSSID);

		WiFiSettings entry;
		entry.ssid = std::move(strSSID);
		entry.password = std::move(strPassword);

#ifdef STATIC_IP_ENABLE
		entry.staticIp = WiFiSettings::StaticIp(IPAddress(LOCAL_IP), IPAddress(SUBNET_IP), IPAddress(GATEWAY_IP), IPAddress(DNS_IP));
#endif

		Wlan_AddNetworkSettings(entry);
		// clean up old values from nvs
		gPrefsSettings.remove("SSID");
		gPrefsSettings.remove("Password");
	}
}

/// @brief Migrate Version 2 (single entry with max 10 binary entries) to current version
static void migrateFromVersion2() {
	if (!System_SettingsPrefsAvailable()) {
		return;
	}

	constexpr const char *nvsKey = "SAVED_WIFIS";
	struct OldWiFiSettings {
		char ssid[33] {0};
		char password[65] {0};
		bool use_static_ip {false};
		uint32_t static_addr {0};
		uint32_t static_gateway {0};
		uint32_t static_subnet {0};
		uint32_t static_dns1 {0};
		uint32_t static_dns2 {0};

		OldWiFiSettings() = default;
	};

	if (gPrefsSettings.isKey(nvsKey)) {
		constexpr size_t maxLegacyWiFiSettingsEntries = 10;
		const size_t blobLen = gPrefsSettings.getBytesLength(nvsKey);
		if (blobLen == 0 || blobLen % sizeof(OldWiFiSettings) != 0 || blobLen > maxLegacyWiFiSettingsEntries * sizeof(OldWiFiSettings)) {
			Log_Println("Invalid legacy WiFi NVS settings; deleting", LOGLEVEL_ERROR);
			gPrefsSettings.remove(nvsKey);
			return;
		}

		const size_t numNetworks = blobLen / sizeof(OldWiFiSettings);
		std::vector<OldWiFiSettings> settings(numNetworks);
		if (gPrefsSettings.getBytes(nvsKey, settings.data(), blobLen) != blobLen) {
			Log_Println("Could not read legacy WiFi NVS settings; deleting", LOGLEVEL_ERROR);
			gPrefsSettings.remove(nvsKey);
			return;
		}

		Log_Println("migrating from old wifi NVS settings!", LOGLEVEL_NOTICE);
		for (size_t i = 0; i < numNetworks; i++) {
			OldWiFiSettings &s = settings[i];
			// convert the old wifi settings to the new structure
			WiFiSettings entry;
			entry.ssid = s.ssid;
			entry.password = s.password;

			if (settings[i].use_static_ip) {
				// fill out the static structure
				entry.staticIp = WiFiSettings::StaticIp(s.static_addr, s.static_subnet, s.static_gateway, s.static_dns1, s.static_dns2);
			}

			// validate the data and write the entry
			if (entry.isValid()) {
				Wlan_AddNetworkSettings(entry);
			}
		}

		// clean up old nvs entries
		gPrefsSettings.remove(nvsKey);
	}
}

void Wlan_Init(void) {
	prefsWifiSettingsAvailable = prefsWifiSettings.begin(nvsWiFiNamespace);
	if (!prefsWifiSettingsAvailable) {
		Log_Printf(LOGLEVEL_ERROR, "NVS namespace open failed: %s", nvsWiFiNamespace);
	}

	wifiEnabled = getWifiEnableStatusFromNVS();

	// Get (optional) hostname-configuration from NVS
	const String hostname = getHostname();
	if (hostname) {
		Log_Printf(LOGLEVEL_INFO, restoredHostnameFromNvs, hostname.c_str());
	} else {
		Log_Println(wifiHostnameNotSet, LOGLEVEL_INFO);
	}

	// ******************* MIGRATION *******************
	if (prefsWifiSettingsAvailable) {
		migrateFromVersion1();
		migrateFromVersion2();

		// Self-heal NVS: malformed WiFi blobs cannot be used and must not be allowed to break boot.
		deleteInvalidWiFiNvsEntries();
	} else {
		Log_Println("WiFi settings NVS namespace is not available; skipping saved WiFi entries", LOGLEVEL_ERROR);
	}

	// dump all network settings
	if (prefsWifiSettingsAvailable) {
		iterateNvsEntries([](const char *, const WiFiSettings &s) {
			char buffer[128]; // maximum buffer needed when we have static IP
			const char *ipMode = "dynamic IP";

			if (s.staticIp.isValid()) {
				snprintf(buffer, sizeof(buffer), "ip: %s, subnet: %s, gateway: %s, dns1: %s, dns2: %s", s.staticIp.addr.toString().c_str(),
					s.staticIp.subnet.toString().c_str(), s.staticIp.gateway.toString().c_str(), s.staticIp.dns1.toString().c_str(),
					s.staticIp.dns2.toString().c_str());
				ipMode = buffer;
			}
			Log_Printf(LOGLEVEL_DEBUG, "SSID: %s, Password: %s, %s", s.ssid.c_str(), (s.password.length()) ? "yes" : "no", ipMode);

			if (System_SettingsPrefsAvailable() && gPrefsSettings.isKey("LAST_SSID") == false) {
				gPrefsSettings.putString("LAST_SSID", s.ssid);
				Log_Println("Warn: using saved SSID as LAST_SSID", LOGLEVEL_NOTICE);
			}

			return true;
		});
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

void connectToKnownNetwork(const WiFiSettings &settings, const uint8_t *bssid = nullptr) {
	// set hostname on connect, because when resetting wifi config elsewhere it could be reset
	const String hostname = getHostname();
	if (hostname) {
		WiFi.setHostname(hostname.c_str());
	}

	if (settings.staticIp.isValid()) {
		Log_Println(tryStaticIpConfig, LOGLEVEL_NOTICE);
		if (!WiFi.config(settings.staticIp.addr, settings.staticIp.gateway, settings.staticIp.subnet, settings.staticIp.dns1, settings.staticIp.dns2)) {
			Log_Println(staticIPConfigFailed, LOGLEVEL_ERROR);
		}
	}

	Log_Printf(LOGLEVEL_NOTICE, wifiConnectionInProgress, settings.ssid.c_str());

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
	const String lastSSID = gPrefsSettings.getString("LAST_SSID");

	std::optional<WiFiSettings> lastSettings = std::nullopt;

	if (lastSSID) {
		auto entry = getNvsWifiSettings(lastSSID);
		if (entry) {
			lastSettings = entry->value;
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
		uint8_t *bssid = WiFi.BSSID(i);
		// check if ssid name matches any saved ssid
		const auto entry = getNvsWifiSettings(issid);
		if (entry) {
			// we found the entry
			connectToKnownNetwork(entry->value, bssid);

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
	char *timeStringBuff = (char *) x_malloc(255);
	snprintf(timeStringBuff, sizeof(char) * 255, ntpGotTime, timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
	Log_Println(timeStringBuff, LOGLEVEL_NOTICE);
	free(timeStringBuff);
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
	const String hostname = getHostname();
	if (hostname) {
		if (MDNS.begin(hostname.c_str())) {
			MDNS.addService("http", "tcp", 80);
			Log_Printf(LOGLEVEL_NOTICE, mDNSStarted, hostname.c_str());
		} else {
			Log_Printf(LOGLEVEL_ERROR, mDNSFailed, hostname.c_str());
		}
	}
#endif
	delete dnsServer;
	dnsServer = nullptr;

	bool playLastRfidAfterReboot;
#ifdef PLAY_LAST_RFID_AFTER_REBOOT
	playLastRfidAfterReboot = gPrefsSettings.getBool("playLastOnBoot", true);
#else
	playLastRfidAfterReboot = gPrefsSettings.getBool("playLastOnBoot", false);
#endif

	if (playLastRfidAfterReboot && gPlayLastRfIdWhenWiFiConnected && gTriedToConnectToHost) {
		gPlayLastRfIdWhenWiFiConnected = false;
		recoverLastRfidPlayedFromNvs(true);
	}

	wifiState = WIFI_STATE_CONNECTED;
	Mqtt_OnWifiConnected();
}

unsigned long lastRssiTimestamp;

void handleWifiStateConnected() {
	static int8_t lastRssiValue = 0;

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

	if ((millis() - lastRssiTimestamp) > 60000u) {
		// print RSSI every 60 seconds
		lastRssiTimestamp = millis();
		// show RSSI value only if it has changed by > 3 dBm
		if (abs(lastRssiValue - Wlan_GetRssi()) > 3) {
			Log_Printf(LOGLEVEL_DEBUG, "RSSI: %d dBm", Wlan_GetRssi());
			lastRssiValue = Wlan_GetRssi();
		}
#ifdef MQTT_ENABLE
		publishMqtt(topicWiFiRssi, static_cast<int32_t>(Wlan_GetRssi()), false);
#endif
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
	constexpr uint32_t closeWifiAPTimeout = 300000;

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
	// correct lenght check
	const size_t len = newHostname.length();
	if (len < 2 || len > 32) {
		return false;
	}

	// rules:
	//   - first character:  alphanumerical
	//   - last character:   alphanumerical
	//   - other characters: alphanumerical or '-'
	//
	// These rules are mainly for mDNS purposes, a "pretty" hostname could have far fewer restrictions

	// check first and last character
	if (!isAlphaNumeric(newHostname[0]) || !isAlphaNumeric(newHostname[len - 1])) {
		return false;
	}

	// check all characters in the string
	for (const auto &c : newHostname) {
		if (!isAlphaNumeric(c) && c != '-') {
			return false;
		}
	}

	// all checks passed
	return true;
}

bool Wlan_SetHostname(String newHostname) {
	if (!Wlan_ValidateHostname(newHostname)) {
		return false;
	}

	if (!System_SettingsPrefsAvailable()) {
		return false;
	}

	// check if the correct length was written to nvs
	return gPrefsSettings.putString("Hostname", newHostname) == newHostname.length();
}

bool Wlan_AddNetworkSettings(const WiFiSettings &settings) {
	if (!prefsWifiSettingsAvailable) {
		Log_Println("WiFi settings NVS namespace is not available; will not write WiFi settings", LOGLEVEL_ERROR);
		return false;
	}

	if (!settings.isValid()) {
		Log_Println("Invalid WiFi settings, will not write to NVS", LOGLEVEL_ERROR);
		return false;
	}

	// find the entry if the network already exists
	auto nvsSetting = getNvsWifiSettings(settings.ssid);
	if (nvsSetting) {
		// we are updating an existing entry
		Log_Printf(LOGLEVEL_NOTICE, wifiUpdateNetwork, settings.ssid.c_str());
		return storeWiFiSettingsToNvs(nvsSetting->key, settings);
	}

	// this is a new entry, find the first unused key
	for (uint8_t i = 0; i < std::numeric_limits<uint8_t>::max(); i++) {
		char key[NVS_KEY_NAME_MAX_SIZE];
		snprintf(key, NVS_KEY_NAME_MAX_SIZE, nvsWiFiKeyFormat, i);
		if (!prefsWifiSettings.isKey(key) || !loadWiFiSettingsFromNvs(key).isValid()) {
			// we found a slot, use it
			if (prefsWifiSettings.isKey(key)) {
				prefsWifiSettings.remove(key);
			}
			Log_Printf(LOGLEVEL_NOTICE, wifiAddNetwork, settings.ssid.c_str());
			return storeWiFiSettingsToNvs(key, settings);
		}
	}

	// if we reach here, we did not find a free slot
	Log_Println(wifiAddTooManyNetworks, LOGLEVEL_ERROR);
	return false;
}

uint8_t Wlan_NumSavedNetworks() {
	if (!prefsWifiSettingsAvailable) {
		return 0;
	}

	// this will be suprisingly expensive
	uint8_t numEntries = 0;
	iterateNvsEntries([&numEntries](const char *, const WiFiSettings &) {
		numEntries++;
		return true;
	});
	return numEntries;
}

void Wlan_GetSavedNetworks(std::function<void(const WiFiSettings &)> handler) {
	if (!prefsWifiSettingsAvailable) {
		return;
	}

	iterateNvsEntries([&handler](const char *, const WiFiSettings &s) {
		handler(s);
		return true;
	});
}

const String Wlan_GetCurrentSSID() {
	return WiFi.SSID();
}

const String Wlan_GetHostname() {
	return System_SettingsPrefsAvailable() ? gPrefsSettings.getString("Hostname", "ESPuino") : String("ESPuino");
}

const String Wlan_GetMacAddress() {
	return WiFi.macAddress();
}

bool Wlan_DeleteNetwork(String ssid) {
	Log_Printf(LOGLEVEL_NOTICE, wifiDeleteNetwork, ssid.c_str());

	if (!prefsWifiSettingsAvailable) {
		return false;
	}

	const auto entry = getNvsWifiSettings(ssid);
	if (entry) {
		// we found the SSID, remove key
		prefsWifiSettings.remove(entry->key.c_str());
	}

	return entry.has_value();
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
	Log_Printf(LOGLEVEL_NOTICE, "IP-Adresse: %s", ip.toString().c_str());

	if (!dnsServer) {
		dnsServer = new DNSServer();
	}

	dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer->start(DNS_PORT, "*", ip);
}

// Reads stored WiFi-status from NVS
bool getWifiEnableStatusFromNVS(void) {
	if (!System_SettingsPrefsAvailable()) {
		return true;
	}

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

	if (System_SettingsPrefsAvailable()) {
		gPrefsSettings.putUInt("enableWifi", wifiEnabled ? 1 : 0);
	}

	if (wifiEnabled) {
		Log_Println(wifiEnabledMsg, LOGLEVEL_NOTICE);
	} else {
		Log_Println(wifiDisabledMsg, LOGLEVEL_NOTICE);
		if (gPlayProperties.isWebstream) {
			AudioPlayer_SetTrackControl(STOP);
		}
	}

	// go to init state again to handle new 'wifiEnabled'
	wifiState = WIFI_STATE_INIT;
}

bool Wlan_IsConnected(void) {
	return (wifiState == WIFI_STATE_CONNECTED);
}

std::optional<std::vector<uint8_t>> WiFiSettings::serialize() const {
	if (!isValid()) {
		// we refuse to serialize a corrupted data set
		Log_Println("Data corrupted, will not serialize", LOGLEVEL_ERROR);
		return std::nullopt;
	}

	// calculate the length of the entry
	const size_t binaryLen = serializeStringLen(ssid) + serializeStringLen(password) + serializeStaticIpLen(staticIp);
	std::vector<uint8_t> buffer;
	buffer.resize(binaryLen);

	// the write iterator
	auto it = buffer.begin();

	// write the ssid
	serializeString(it, ssid);

	// write the password
	serializeString(it, password);

	// write the static IPs
	serializeStaticIp(it, staticIp);

	return buffer;
}

void WiFiSettings::deserialize(const std::vector<uint8_t> &buffer) {
	hasDeserializeError = false;
	ssid = "";
	password = "";
	staticIp = StaticIp();

	// prepare the read iterator
	auto it = buffer.cbegin();
	const auto end = buffer.cend();

	const bool ok = deserializeString(it, end, ssid, maxSsidLength)
		&& deserializeString(it, end, password, maxPasswordLength)
		&& deserializeStaticIp(it, end, staticIp)
		&& it == end
		&& !ssid.isEmpty();

	if (!ok) {
		ssid = "";
		password = "";
		staticIp = StaticIp();
		hasDeserializeError = true;
	}
}

void WiFiSettings::serializeString(std::vector<uint8_t>::iterator &it, const String &s) const {
	*it = s.length();
	it++;
	it = std::copy(s.begin(), s.end(), it);
	*it = '\0';
	it++;
};

bool WiFiSettings::deserializeString(std::vector<uint8_t>::const_iterator &it, std::vector<uint8_t>::const_iterator end, String &s, size_t maxLen) {
	s = "";
	if (it == end) {
		return false;
	}

	const size_t len = *it;
	it++;
	if (len > maxLen || static_cast<size_t>(std::distance(it, end)) < len + 1) {
		return false;
	}

	if (*(it + len) != '\0') {
		return false;
	}

	s.reserve(len + 1);
	for (size_t i = 0; i < len; i++) {
		const char c = static_cast<char>(*(it + i));
		if (c == '\0') {
			return false;
		}
		s += c;
	}
	it += len + 1;
	return true;
};

void WiFiSettings::serializeStaticIp(std::vector<uint8_t>::iterator &it, const StaticIp &ip) const {
	*it = ip.isValid();
	it++;
	if (ip.isValid()) {
		uint32_t blob[StaticIp::numFields];
		ip.serialize(blob);
		memcpy(&(*it), blob, sizeof(blob));
		it += StaticIp::numFields * sizeof(uint32_t);
	}
};

bool WiFiSettings::deserializeStaticIp(std::vector<uint8_t>::const_iterator &it, std::vector<uint8_t>::const_iterator end, StaticIp &ip) {
	ip = StaticIp();
	if (it == end) {
		return false;
	}

	const uint8_t hasStaticIp = *it;
	it++;
	if (hasStaticIp > 1) {
		return false;
	}

	if (!hasStaticIp) {
		return true;
	}

	constexpr size_t staticIpBytes = StaticIp::numFields * sizeof(uint32_t);
	if (static_cast<size_t>(std::distance(it, end)) < staticIpBytes) {
		return false;
	}

	uint32_t blob[StaticIp::numFields];
	memcpy(blob, &(*it), sizeof(blob));
	ip.deserialize(blob);
	it += staticIpBytes;
	return ip.isValid();
};
