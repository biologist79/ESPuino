#include <Arduino.h>
#include "settings.h"

#include "Bluetooth.h"

#include "Common.h"
#include "Log.h"
#include "RotaryEncoder.h"
#include "System.h"
#include "Web.h"
#include "esp_bt_main.h"

#include <AudioPlayer.h>
#include <algorithm>

#ifdef BLUETOOTH_ENABLE
	#include "BluetoothA2DPCommon.h"
	#include "BluetoothA2DPSink.h"
	#include "BluetoothA2DPSource.h"
	#include "esp_bt.h"
	#include "esp_bt_defs.h"
	#include "esp_gap_bt_api.h"
	#include "esp_heap_caps.h"
	#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3))
		#include "ESP_I2S.h"
I2SClass i2s;
	#endif
#endif

#ifdef BLUETOOTH_ENABLE
	#define BLUETOOTHPLAYER_VOLUME_MAX 21u
	#define BLUETOOTHPLAYER_VOLUME_MIN 0u

// Timeout matches ESP-IDF inquiry duration: 10 slots * 1.28s = 12.8s
static constexpr uint32_t SCAN_TIMEOUT_MS = 13000u;
static constexpr uint32_t SCAN_TIMEOUT_GUARD_MS = 2000u;

BluetoothA2DPSink *a2dp_sink;
BluetoothA2DPSource *a2dp_source;
std::vector<ScannedBluetoothDevice> scannedDevices;
static volatile bool scanInProgress = false;
static uint32_t scanStartTimestamp = 0;
static esp_bd_addr_t pendingConnectAddress = {0};
static volatile bool pendingConnect = false;
static portMUX_TYPE pendingConnectMux = portMUX_INITIALIZER_UNLOCKED;
RingbufHandle_t audioSourceRingBuffer;
static volatile bool bluetoothSourceConnected = false;
String btDeviceName;
static portMUX_TYPE scannedDevicesMux = portMUX_INITIALIZER_UNLOCKED;
// Suppresses auto-connect via scan_bluetooth_device_callback while a manual
// connect is pending (i.e. user picked a device from a scan result).
static volatile bool manualConnectPending = false;

// Track whether we registered our interceptor so we always restore correctly.
static volatile bool interceptorRegistered = false;

// Peer address cached at the moment connection_state_changed fires CONNECTED.
static esp_bd_addr_t cachedPeerAddress = {0};
static portMUX_TYPE cachedPeerAddressMux = portMUX_INITIALIZER_UNLOCKED;

// Returns true if addr is non-zero (i.e. a valid BT address).
static inline bool isValidBdAddr(const esp_bd_addr_t addr) {
	for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
		if (addr[i] != 0) {
			return true;
		}
	}
	return false;
}

// ── connect-retry state ────────────────────────────────────────────────────
static constexpr uint8_t CONNECT_MAX_RETRIES = 3u;
static constexpr uint32_t CONNECT_RETRY_DELAY_MS = 1500u;
static uint8_t connectRetryCount = 0;
static uint32_t connectRetryTimestamp = 0;
static bool connectRetryPending = false;
static esp_bd_addr_t connectRetryAddress = {0};
#endif

#ifdef BLUETOOTH_ENABLE
const char *getType() {
	if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
		return "sink";
	} else {
		return "source";
	}
}
#endif

#ifdef BLUETOOTH_ENABLE
static void Bluetooth_Source_FlushRingbuffer(void) {
	if (!audioSourceRingBuffer) {
		return;
	}
	size_t bytesRead = 0;
	uint8_t *item = nullptr;
	while ((item = static_cast<uint8_t *>(xRingbufferReceiveUpTo(audioSourceRingBuffer, &bytesRead, 0, SIZE_MAX))) != nullptr) {
		vRingbufferReturnItem(audioSourceRingBuffer, item);
	}
}
#endif

#ifdef BLUETOOTH_ENABLE
// Forward declarations
static void Bluetooth_StopScan();
static void Bluetooth_RestoreLibraryCallback();
#endif

#ifdef BLUETOOTH_ENABLE
extern "C" void ccall_app_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

static void gap_callback_interceptor(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
	if (event == ESP_BT_GAP_DISC_RES_EVT) {
		// Accept discovery results regardless of scanInProgress so that results
		// arriving in the brief window between natural stop and flag-clear are
		// not silently dropped.
		char ssid[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {0};
		esp_bd_addr_t bda;
		memcpy(bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
		int rssi = -129;

		for (int i = 0; i < param->disc_res.num_prop; i++) {
			esp_bt_gap_dev_prop_t *p = param->disc_res.prop + i;
			if (p->type == ESP_BT_GAP_DEV_PROP_BDNAME && p->len > 0) {
				size_t len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN : p->len;
				memcpy(ssid, p->val, len);
				ssid[len] = '\0';
			} else if (p->type == ESP_BT_GAP_DEV_PROP_EIR) {
				uint8_t name_len = 0;
				uint8_t *name_ptr = esp_bt_gap_resolve_eir_data((uint8_t *) p->val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &name_len);
				if (!name_ptr) {
					name_ptr = esp_bt_gap_resolve_eir_data((uint8_t *) p->val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &name_len);
				}
				if (name_ptr && name_len > 0) {
					size_t len = (name_len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN : name_len;
					memcpy(ssid, name_ptr, len);
					ssid[len] = '\0';
				}
			} else if (p->type == ESP_BT_GAP_DEV_PROP_RSSI) {
				rssi = *(int8_t *) (p->val);
			}
		}

		portENTER_CRITICAL_ISR(&scannedDevicesMux);
		ScannedBluetoothDevice *existing = nullptr;
		for (auto &d : scannedDevices) {
			if (memcmp(d.address, bda, ESP_BD_ADDR_LEN) == 0) {
				existing = &d;
				break;
			}
		}

		if (existing) {
			if (strlen(ssid) > 0 && strcmp(existing->name, "Unknown") == 0) {
				strncpy(existing->name, ssid, ESP_BT_GAP_MAX_BDNAME_LEN);
				existing->name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
			}
			existing->rssi = rssi;
		} else {
			ScannedBluetoothDevice device;
			const char *name = (strlen(ssid) > 0) ? ssid : "Unknown";
			strncpy(device.name, name, ESP_BT_GAP_MAX_BDNAME_LEN);
			device.name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
			memcpy(device.address, bda, ESP_BD_ADDR_LEN);
			device.rssi = rssi;
			scannedDevices.push_back(device);

			// Actively request the remote name for devices that arrived without one
			if (strlen(ssid) == 0) {
				esp_bt_gap_read_remote_name(bda);
			}
		}
		portEXIT_CRITICAL_ISR(&scannedDevicesMux);

	} else if (event == ESP_BT_GAP_READ_REMOTE_NAME_EVT && param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
		portENTER_CRITICAL_ISR(&scannedDevicesMux);
		for (auto &d : scannedDevices) {
			if (memcmp(d.address, param->read_rmt_name.bda, ESP_BD_ADDR_LEN) == 0) {
				if (strcmp(d.name, "Unknown") == 0) {
					strncpy(d.name, (char *) param->read_rmt_name.rmt_name, ESP_BT_GAP_MAX_BDNAME_LEN);
					d.name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
				}
				break;
			}
		}
		portEXIT_CRITICAL_ISR(&scannedDevicesMux);

	} else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
		if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
			// Inquiry stopped naturally — clear our flag and restore the library
			// callback BEFORE calling ccall_app_gap_callback so the library sees
			// itself as the registered handler again.
			scanInProgress = false;
			Bluetooth_RestoreLibraryCallback();
			Log_Println("Bluetooth => Device discovery stopped (natural).", LOGLEVEL_NOTICE);
			Web_SendWebsocketData(0, WebsocketCodeType::BluetoothScanComplete);
		} else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
			Log_Println("Bluetooth => Device discovery started.", LOGLEVEL_NOTICE);
		}
	}

	// Always forward to the library's own handler to keep its internal state consistent.
	// Note: after Bluetooth_RestoreLibraryCallback() the library IS the registered
	// callback again, but we still call it directly here for this final event.
	ccall_app_gap_callback(event, param);
}

// Restores the library callback and clears our interceptor-registered flag.
// Safe to call multiple times (idempotent).
static void Bluetooth_RestoreLibraryCallback() {
	if (interceptorRegistered) {
		interceptorRegistered = false;
		esp_bt_gap_register_callback(ccall_app_gap_callback);
	}
}
#endif

#ifdef BLUETOOTH_ENABLE
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
	Log_Printf(LOGLEVEL_INFO, "Bluetooth %s => connection state: %s (Free heap: %u Bytes)", getType(), ((BluetoothA2DPCommon *) ptr)->to_str(state), ESP.getFreeHeap());
	if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
		gPlayProperties.isWebstream = false;
		gPlayProperties.pausePlay = false;
		gPlayProperties.playlistFinished = true;
	} else if (System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) {
		const bool connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
		const bool wasConnected = bluetoothSourceConnected;

		if (!connected && wasConnected) {
			Bluetooth_Source_FlushRingbuffer();
		}
		bluetoothSourceConnected = connected;

		if (connected) {
			AudioPlayer_SetupVolumeAndAmps();
			// Cache the peer address NOW while the library state is guaranteed valid.
			// get_last_peer_address() can return all-zeros if called later (e.g. from
			// Bluetooth_StartScan or Bluetooth_GetScannedDevices) at a moment when the
			// library's internal peer-address field hasn't settled yet.
			portENTER_CRITICAL(&cachedPeerAddressMux);
			memcpy(cachedPeerAddress, a2dp_source->get_last_peer_address(), ESP_BD_ADDR_LEN);
			portEXIT_CRITICAL(&cachedPeerAddressMux);
			Log_Printf(LOGLEVEL_INFO, "Bluetooth => connected, cached peer: %02X:%02X:%02X:%02X:%02X:%02X",
				cachedPeerAddress[0], cachedPeerAddress[1], cachedPeerAddress[2],
				cachedPeerAddress[3], cachedPeerAddress[4], cachedPeerAddress[5]);
			// Connection succeeded — clear retry and manual-connect state
			manualConnectPending = false;
			connectRetryPending = false;
			connectRetryCount = 0;
			Bluetooth_StopScan();
		} else {
			// Clear the cached peer address on disconnect so stale data is never used.
			portENTER_CRITICAL(&cachedPeerAddressMux);
			memset(cachedPeerAddress, 0, ESP_BD_ADDR_LEN);
			portEXIT_CRITICAL(&cachedPeerAddressMux);
			// If this was a manual connect attempt that failed, schedule a retry.
			// Do NOT call AudioPlayer_SetupVolumeAndAmps() here: the library's own
			// auto-reconnect (set_auto_reconnect(true)) fires Connecting→Disconnected
			// repeatedly every ~5 s while searching for the device, so calling
			// SetupVolumeAndAmps() on each transient disconnect spams the log and
			// needlessly toggles the speaker/amp on every cycle.
			// Only restore the amp once we have truly given up all retries, or when
			// the device was previously connected and has now genuinely dropped.
			if (manualConnectPending && connectRetryCount < CONNECT_MAX_RETRIES) {
				connectRetryPending = true;
				connectRetryTimestamp = millis();
				connectRetryCount++;
				Log_Printf(LOGLEVEL_INFO, "Bluetooth => connection failed, retry %u/%u in %u ms",
					connectRetryCount, CONNECT_MAX_RETRIES, CONNECT_RETRY_DELAY_MS);
			} else if (manualConnectPending && connectRetryCount >= CONNECT_MAX_RETRIES) {
				Log_Println("Bluetooth => all connection retries exhausted, giving up", LOGLEVEL_NOTICE);
				manualConnectPending = false;
				connectRetryPending = false;
				connectRetryCount = 0;
				// Retries exhausted — now it is appropriate to restore the amp
				AudioPlayer_SetupVolumeAndAmps();
			} else if (wasConnected) {
				// Device was genuinely connected and has now dropped — restore amp
				AudioPlayer_SetupVolumeAndAmps();
			}
			// else: library auto-reconnect cycle (wasConnected==false, no manual
			// connect pending) — do nothing, avoid spam
		}
	}
}
#endif

#ifdef BLUETOOTH_ENABLE
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
	Log_Printf(LOGLEVEL_INFO, "Bluetooth %s => audio state: %s (Free heap: %u Bytes)", getType(), ((BluetoothA2DPCommon *) ptr)->to_str(state), ESP.getFreeHeap());
	if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
		gPlayProperties.pausePlay = (state != ESP_A2D_AUDIO_STATE_STARTED);
		gPlayProperties.playlistFinished = false;
		gPlayProperties.isWebstream = true;
	}
}
#endif

#ifdef BLUETOOTH_ENABLE
void button_handler(uint8_t id, bool isReleased) {
	if (isReleased) {
		switch (id) {
			case 70:
			case 68:
				Log_Printf(LOGLEVEL_DEBUG, "Bluetooth button id %u (pause/resume) is released.", id);
				AudioPlayer_SetTrackControl(PAUSEPLAY);
				break;
			case 75:
				Log_Printf(LOGLEVEL_DEBUG, "Bluetooth button id %u (next track) is released.", id);
				AudioPlayer_SetTrackControl(NEXTTRACK);
				break;
			case 76:
				Log_Printf(LOGLEVEL_DEBUG, "Bluetooth button id %u (previous track) is released.", id);
				AudioPlayer_SetTrackControl(PREVIOUSTRACK);
				break;
			default:
				Log_Printf(LOGLEVEL_DEBUG, "Unknown bluetooth button id %u is released.", id);
				break;
		}
	}
}
#endif

#ifdef BLUETOOTH_ENABLE
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
	if (strlen((char *) text) == 0) {
		return;
	}
	switch (id) {
		case ESP_AVRC_MD_ATTR_TITLE:
			Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => AVRC Title: %s", text);
			break;
		case ESP_AVRC_MD_ATTR_ARTIST:
			Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => AVRC Artist: %s", text);
			break;
		case ESP_AVRC_MD_ATTR_ALBUM:
			Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => AVRC Album: %s", text);
			break;
		case ESP_AVRC_MD_ATTR_TRACK_NUM:
			Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => AVRC Track-No: %s", text);
			break;
		case ESP_AVRC_MD_ATTR_NUM_TRACKS:
			Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => AVRC Number of tracks: %s", text);
			break;
		case ESP_AVRC_MD_ATTR_GENRE:
			Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => AVRC Genre: %s", text);
			break;
		default:
			Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => AVRC metadata rsp: attribute id 0x%x, %s", id, text);
			break;
	}
}
#endif

#ifdef BLUETOOTH_ENABLE
int32_t get_data_channels(Frame *frame, int32_t channel_len) {
	if (channel_len < 0 || frame == NULL) {
		return 0;
	}
	if (audioSourceRingBuffer == NULL) {
		return 0;
	}
	size_t len {};
	vRingbufferGetInfo(audioSourceRingBuffer, nullptr, nullptr, nullptr, nullptr, &len);
	if (len < (channel_len * 4)) {
		return 0;
	};
	size_t sampleSize = 0;
	uint8_t *sampleBuff;
	sampleBuff = (uint8_t *) xRingbufferReceiveUpTo(audioSourceRingBuffer, &sampleSize, (TickType_t) portMAX_DELAY, channel_len * 4);
	if (sampleBuff != NULL) {
		for (int sample = 0; sample < (channel_len); ++sample) {
			frame[sample].channel1 = (sampleBuff[sample * 4 + 3] << 8) | sampleBuff[sample * 4 + 2];
			frame[sample].channel2 = (sampleBuff[sample * 4 + 1] << 8) | sampleBuff[sample * 4];
		};
		vRingbufferReturnItem(audioSourceRingBuffer, (void *) sampleBuff);
	};
	vTaskDelay(portTICK_PERIOD_MS * 1);
	return channel_len;
};
#endif

#ifdef BLUETOOTH_ENABLE
void rssi(esp_bt_gap_cb_param_t::read_rssi_delta_param &rssiParam) {
	Log_Printf(LOGLEVEL_DEBUG, "Bluetooth => RSSI value: %d", rssiParam.rssi_delta);
}
#endif

#ifdef BLUETOOTH_ENABLE
// Returns true to auto-connect, false to keep scanning.
// Suppressed while a manual scan or manual connect is in progress.
bool scan_bluetooth_device_callback(const char *ssid, esp_bd_addr_t address, int rssi) {
	if (scanInProgress || manualConnectPending) {
		return false;
	}

	Log_Printf(LOGLEVEL_INFO, "Bluetooth source => Device found: %s (%02X:%02X:%02X:%02X:%02X:%02X)",
		ssid, address[0], address[1], address[2], address[3], address[4], address[5]);

	if (btDeviceName == "") {
		return true;
	} else {
		esp_bd_addr_t addr;
		if (sscanf(btDeviceName.c_str(), "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX", &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]) == 6) {
			if (memcmp(address, addr, ESP_BD_ADDR_LEN) == 0) {
				return true;
			}
		}
		return (ssid != nullptr && startsWith(ssid, btDeviceName.c_str()));
	}
}
#endif

void Bluetooth_VolumeChanged(int _newVolume) {
#ifdef BLUETOOTH_ENABLE
	if ((_newVolume < 0) || (_newVolume > 0x7F)) {
		return;
	}
	uint8_t _volume;
	_volume = map(_newVolume, 0, 0x7F, BLUETOOTHPLAYER_VOLUME_MIN, BLUETOOTHPLAYER_VOLUME_MAX);
	if (AudioPlayer_GetCurrentVolume() != _volume) {
		Log_Printf(LOGLEVEL_INFO, "Bluetooth => volume changed:  %d !", _volume);
		AudioPlayer_SetVolume(_volume, true);
	}
#endif
}

#ifdef BLUETOOTH_ENABLE
void Bluetooth_StartScan() {
	if (System_GetOperationMode() != OPMODE_BLUETOOTH_SOURCE || !a2dp_source) {
		Log_Println("Bluetooth_StartScan: not in source mode or source not initialized", LOGLEVEL_ERROR);
		return;
	}
	if (scanInProgress) {
		Log_Println("Bluetooth_StartScan: scan already in progress, ignoring", LOGLEVEL_INFO);
		return;
	}

	// Capture connected device so it stays at the top of the list during scan.
	// Use cachedPeerAddress (set in connection_state_changed) rather than
	// get_last_peer_address() which can return all-zeros at this point.
	ScannedBluetoothDevice connectedDev;
	bool wasConnected = bluetoothSourceConnected;
	if (wasConnected) {
		portENTER_CRITICAL(&cachedPeerAddressMux);
		memcpy(connectedDev.address, cachedPeerAddress, ESP_BD_ADDR_LEN);
		portEXIT_CRITICAL(&cachedPeerAddressMux);
		if (!isValidBdAddr(connectedDev.address)) {
			Log_Println("Bluetooth_StartScan => WARNING: connected but cachedPeerAddress is all-zeros, skipping pre-population", LOGLEVEL_NOTICE);
			wasConnected = false; // treat as not connected to avoid inserting a zero-address entry
		} else {
			strncpy(connectedDev.name, btDeviceName.length() > 0 ? btDeviceName.c_str() : "Connected Device", ESP_BT_GAP_MAX_BDNAME_LEN);
			connectedDev.name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
			connectedDev.rssi = 0;
			Log_Printf(LOGLEVEL_INFO, "Bluetooth_StartScan => pre-populating connected device: %s (%02X:%02X:%02X:%02X:%02X:%02X)",
				connectedDev.name,
				connectedDev.address[0], connectedDev.address[1], connectedDev.address[2],
				connectedDev.address[3], connectedDev.address[4], connectedDev.address[5]);
		}
	}

	portENTER_CRITICAL(&scannedDevicesMux);
	scannedDevices.clear();
	if (wasConnected) {
		scannedDevices.push_back(connectedDev);
	}
	portEXIT_CRITICAL(&scannedDevicesMux);

	interceptorRegistered = true;
	esp_bt_gap_register_callback(gap_callback_interceptor);

	scanInProgress = true;
	scanStartTimestamp = millis();

	Log_Println("Bluetooth => Starting device discovery...", LOGLEVEL_NOTICE);
	// Duration: 10 * 1.28 s = 12.8 s
	esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
	if (err != ESP_OK) {
		Log_Printf(LOGLEVEL_ERROR, "Bluetooth => esp_bt_gap_start_discovery failed: %s (connected=%d, heapFree=%u)",
			esp_err_to_name(err), (int) bluetoothSourceConnected, ESP.getFreeHeap());
		scanInProgress = false;
		Bluetooth_RestoreLibraryCallback();
		// Still notify the UI so it doesn't hang waiting for a completion that will never arrive.
		// The connected device was already pre-populated above (if applicable), so the
		// list is as complete as it can be.
		Web_SendWebsocketData(0, WebsocketCodeType::BluetoothScanComplete);
	}
}

static void Bluetooth_StopScan() {
	if (!scanInProgress) {
		return;
	}
	// Clear flag BEFORE cancel so a re-entrant DISC_STATE_CHANGED_EVT triggered
	// by cancel does not recurse into this function via Cyclic().
	scanInProgress = false;

	// Cancel the ongoing inquiry; the resulting DISC_STATE_CHANGED_EVT will be
	// forwarded to the library by gap_callback_interceptor (which is still
	// registered at this point) and the library callback will be restored there.
	esp_bt_gap_cancel_discovery();

	// Restore callback here as a safety net in case cancel fires no callback
	// (e.g. inquiry already ended between our flag-check and cancel call).
	Bluetooth_RestoreLibraryCallback();

	Log_Println("Bluetooth => Device discovery stopped (forced).", LOGLEVEL_NOTICE);
	Web_SendWebsocketData(0, WebsocketCodeType::BluetoothScanComplete);
}

void Bluetooth_ConnectToAddress(esp_bd_addr_t address) {
	if (System_GetOperationMode() != OPMODE_BLUETOOTH_SOURCE || !a2dp_source) {
		Log_Println("Bluetooth_ConnectToAddress: not in source mode or source not initialized", LOGLEVEL_ERROR);
		return;
	}

	// Update stored device name from scan results if available
	portENTER_CRITICAL(&scannedDevicesMux);
	for (const auto &d : scannedDevices) {
		if (memcmp(d.address, address, ESP_BD_ADDR_LEN) == 0) {
			if (strcmp(d.name, "Unknown") != 0 && strcmp(d.name, "Connected Device") != 0) {
				btDeviceName = d.name;
				gPrefsSettings.putString("btDeviceName", btDeviceName);
				Log_Printf(LOGLEVEL_INFO, "Bluetooth => set preferred device name: %s", btDeviceName.c_str());
			}
			break;
		}
	}
	portEXIT_CRITICAL(&scannedDevicesMux);

	// Reset retry state for this fresh manual connect request
	connectRetryCount = 0;
	connectRetryPending = false;

	portENTER_CRITICAL(&pendingConnectMux);
	memcpy(pendingConnectAddress, address, ESP_BD_ADDR_LEN);
	manualConnectPending = true;
	pendingConnect = true;
	portEXIT_CRITICAL(&pendingConnectMux);
}

std::vector<ScannedBluetoothDevice> Bluetooth_GetScannedDevices() {
	// Returns a BY-VALUE snapshot so the caller never races with concurrent
	// modifications from the BT task.  The copy is made inside the spinlock so
	// it is always consistent.  Callers must NOT hold scannedDevicesMux when
	// calling this function.

	if (System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE && bluetoothSourceConnected) {
		esp_bd_addr_t currentAddr;
		portENTER_CRITICAL(&cachedPeerAddressMux);
		memcpy(currentAddr, cachedPeerAddress, ESP_BD_ADDR_LEN);
		portEXIT_CRITICAL(&cachedPeerAddressMux);

		if (!isValidBdAddr(currentAddr)) {
			Log_Println("Bluetooth_GetScannedDevices => WARNING: bluetoothSourceConnected=true but cachedPeerAddress is all-zeros", LOGLEVEL_NOTICE);
		} else {
			// Inject / promote the connected device under the spinlock so the
			// snapshot below always includes it at position 0.
			portENTER_CRITICAL(&scannedDevicesMux);
			auto it = std::find_if(scannedDevices.begin(), scannedDevices.end(), [&](const ScannedBluetoothDevice &d) {
				return memcmp(d.address, currentAddr, ESP_BD_ADDR_LEN) == 0;
			});

			if (it == scannedDevices.end()) {
				ScannedBluetoothDevice dev;
				memcpy(dev.address, currentAddr, ESP_BD_ADDR_LEN);
				strncpy(dev.name, btDeviceName.length() > 0 ? btDeviceName.c_str() : "Connected Device", ESP_BT_GAP_MAX_BDNAME_LEN);
				dev.name[ESP_BT_GAP_MAX_BDNAME_LEN] = '\0';
				dev.rssi = 0;
				scannedDevices.insert(scannedDevices.begin(), dev);
				Log_Printf(LOGLEVEL_INFO, "Bluetooth_GetScannedDevices => injected connected device at top: %s (%02X:%02X:%02X:%02X:%02X:%02X)",
					dev.name, currentAddr[0], currentAddr[1], currentAddr[2],
					currentAddr[3], currentAddr[4], currentAddr[5]);
			} else if (it != scannedDevices.begin()) {
				ScannedBluetoothDevice dev = *it;
				scannedDevices.erase(it);
				scannedDevices.insert(scannedDevices.begin(), dev);
				Log_Printf(LOGLEVEL_INFO, "Bluetooth_GetScannedDevices => moved connected device to top: %s", dev.name);
			}
			portEXIT_CRITICAL(&scannedDevicesMux);
		}
	}

	// Take a protected snapshot — the spinlock is held only for the duration of
	// the copy (3 POD structs ≈ microseconds), then released before returning.
	portENTER_CRITICAL(&scannedDevicesMux);
	std::vector<ScannedBluetoothDevice> snapshot(scannedDevices);
	portEXIT_CRITICAL(&scannedDevicesMux);

	Log_Printf(LOGLEVEL_INFO, "Bluetooth_GetScannedDevices => returning %u device(s)", snapshot.size());
	return snapshot;
}
#endif

void Bluetooth_Init(void) {
#ifdef BLUETOOTH_ENABLE
	bluetoothSourceConnected = false;
	if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
		a2dp_sink = new BluetoothA2DPSink();
	#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3))
		i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
		if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
			Log_Println("Failed to initialize I2S!", LOGLEVEL_ERROR);
			while (1)
				;
		}
		a2dp_sink->set_output(i2s);
	#else
		i2s_pin_config_t pin_config = {
		#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
			.mck_io_num = 0,
		#endif
			.bck_io_num = I2S_BCLK,
			.ws_io_num = I2S_LRC,
			.data_out_num = I2S_DOUT,
			.data_in_num = I2S_PIN_NO_CHANGE,
		};
		a2dp_sink->set_pin_config(pin_config);
	#endif
		a2dp_sink->set_rssi_callback(rssi);
		a2dp_sink->activate_pin_code(false);
		if (gPrefsSettings.getBool("playMono", false)) {
			a2dp_sink->set_mono_downmix(true);
		}
		a2dp_sink->set_auto_reconnect(true);
		a2dp_sink->set_rssi_active(true);
		a2dp_sink->start(nameBluetoothSinkDevice);
		Log_Printf(LOGLEVEL_INFO, "Bluetooth sink started, Device: %s", nameBluetoothSinkDevice);
		a2dp_sink->set_on_connection_state_changed(connection_state_changed, a2dp_sink);
		a2dp_sink->set_on_audio_state_changed(audio_state_changed, a2dp_sink);
		a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
		a2dp_sink->set_on_volumechange(Bluetooth_VolumeChanged);
	} else if (System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) {
		audioSourceRingBuffer = nullptr;

		a2dp_source = new BluetoothA2DPSource();
		if (!a2dp_source) {
			Log_Println("Failed to create BluetoothA2DPSource object!", LOGLEVEL_ERROR);
			return;
		}

		a2dp_source->set_auto_reconnect(true);
		a2dp_source->set_ssp_enabled(true);

		String btPinCode = gPrefsSettings.getString("btPinCode", "");
		if (btPinCode != "") {
			a2dp_source->set_pin_code(btPinCode.c_str(), ESP_BT_PIN_TYPE_VARIABLE);
		}
		a2dp_source->set_ssid_callback(scan_bluetooth_device_callback);
		a2dp_source->set_avrc_passthru_command_callback(button_handler);
		a2dp_source->start(get_data_channels);
		btDeviceName = "";
		if (gPrefsSettings.isKey("btDeviceName")) {
			btDeviceName = gPrefsSettings.getString("btDeviceName", "");
		}
		Log_Printf(LOGLEVEL_INFO, "Bluetooth source started, connect to device: '%s'", (btDeviceName == "") ? "connect to first device found" : btDeviceName.c_str());
		a2dp_source->set_on_connection_state_changed(connection_state_changed, a2dp_source);
		a2dp_source->set_on_audio_state_changed(audio_state_changed, a2dp_source);
		a2dp_source->set_volume(127);
	} else {
		esp_bt_mem_release(ESP_BT_MODE_BTDM);
	}
#endif
}

void Bluetooth_Exit(void) {
#ifdef BLUETOOTH_ENABLE
	if (a2dp_sink || a2dp_source) {
		if (a2dp_sink) {
			Log_Println("shutdown Bluetooth sink..", LOGLEVEL_NOTICE);
			a2dp_sink = nullptr; // abandon, do NOT delete or call end()
		}
		if (a2dp_source) {
			Log_Println("shutdown Bluetooth source..", LOGLEVEL_NOTICE);
			a2dp_source = nullptr; // abandon, do NOT delete or call end()
		}
		bluetoothSourceConnected = false;
	}
#endif
}

void Bluetooth_Cyclic(void) {
#ifdef BLUETOOTH_ENABLE
	// ── Handle pending manual connect (deferred from Bluetooth_ConnectToAddress) ──
	bool doConnect = false;
	esp_bd_addr_t connectAddress;

	portENTER_CRITICAL(&pendingConnectMux);
	if (pendingConnect) {
		doConnect = true;
		memcpy(connectAddress, pendingConnectAddress, ESP_BD_ADDR_LEN);
		memset(pendingConnectAddress, 0, ESP_BD_ADDR_LEN);
		pendingConnect = false;
	}
	portEXIT_CRITICAL(&pendingConnectMux);

	if (doConnect) {
		// Stop any ongoing scan first so inquiry doesn't compete with paging
		if (scanInProgress) {
			Bluetooth_StopScan();
		}
		if (a2dp_source->is_connected()) {
			esp_bd_addr_t currentAddr;
			portENTER_CRITICAL(&cachedPeerAddressMux);
			memcpy(currentAddr, cachedPeerAddress, ESP_BD_ADDR_LEN);
			portEXIT_CRITICAL(&cachedPeerAddressMux);
			if (memcmp(connectAddress, currentAddr, ESP_BD_ADDR_LEN) != 0) {
				Log_Println("Bluetooth => disconnecting current device before new connection", LOGLEVEL_INFO);
				a2dp_source->disconnect();
				// ── FIX: don't block the main loop with vTaskDelay here ───────
				// Store address for retry and let the disconnect callback (which
				// sets bluetoothSourceConnected=false) naturally trigger the retry
				// path via connectRetryPending below.
				memcpy(connectRetryAddress, connectAddress, ESP_BD_ADDR_LEN);
				connectRetryPending = true;
				connectRetryTimestamp = millis() + 400; // short settle time after disconnect
				connectRetryCount = 0;
				doConnect = false; // skip the immediate connect_to below
			}
		}
		if (doConnect) {
			memcpy(connectRetryAddress, connectAddress, ESP_BD_ADDR_LEN);
			connectRetryCount = 0;
			Log_Printf(LOGLEVEL_INFO, "Bluetooth => connecting to %02X:%02X:%02X:%02X:%02X:%02X",
				connectAddress[0], connectAddress[1], connectAddress[2],
				connectAddress[3], connectAddress[4], connectAddress[5]);
			a2dp_source->connect_to(connectAddress);
		}
	}

	// ── Handle connection retries ─────────────────────────────────────────────
	if (connectRetryPending && !bluetoothSourceConnected) {
		if (millis() - connectRetryTimestamp >= CONNECT_RETRY_DELAY_MS) {
			connectRetryPending = false;
			if (connectRetryCount < CONNECT_MAX_RETRIES) {
				connectRetryCount++;
				connectRetryTimestamp = millis();
				Log_Printf(LOGLEVEL_INFO, "Bluetooth => retry connect %u/%u to %02X:%02X:%02X:%02X:%02X:%02X",
					connectRetryCount, CONNECT_MAX_RETRIES,
					connectRetryAddress[0], connectRetryAddress[1], connectRetryAddress[2],
					connectRetryAddress[3], connectRetryAddress[4], connectRetryAddress[5]);
				a2dp_source->connect_to(connectRetryAddress);
			} else {
				Log_Println("Bluetooth => all retries exhausted", LOGLEVEL_NOTICE);
				manualConnectPending = false;
				connectRetryCount = 0;
			}
		}
	}

	// ── Sink: activity timer ──────────────────────────────────────────────────
	if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) && (a2dp_sink)) {
		if (a2dp_sink->get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
			System_UpdateActivityTimer();
		}
	}

	// ── Source: activity timer + scan timeout guard ───────────────────────────
	if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && (a2dp_source)) {
		if (a2dp_source->get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
			System_UpdateActivityTimer();
		}
		if (scanInProgress) {
			uint32_t elapsed = millis() - scanStartTimestamp;
			if (elapsed > SCAN_TIMEOUT_MS + SCAN_TIMEOUT_GUARD_MS) {
				Log_Println("Bluetooth => scan timeout guard fired, forcing stop", LOGLEVEL_NOTICE);
				Bluetooth_StopScan();
			}
		}
	}
#endif
}

void Bluetooth_PlayPauseTrack(void) {
#ifdef BLUETOOTH_ENABLE
	if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) && (a2dp_sink)) {
		esp_a2d_audio_state_t state = a2dp_sink->get_audio_state();
		if (state == ESP_A2D_AUDIO_STATE_STARTED) {
			a2dp_sink->play();
		} else {
			a2dp_sink->pause();
		}
	}
#endif
}

void Bluetooth_NextTrack(void) {
#ifdef BLUETOOTH_ENABLE
	if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) && (a2dp_sink)) {
		a2dp_sink->next();
	}
#endif
}

void Bluetooth_PreviousTrack(void) {
#ifdef BLUETOOTH_ENABLE
	if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) && (a2dp_sink)) {
		a2dp_sink->previous();
	}
#endif
}

void Bluetooth_SetVolume(const int32_t _newVolume, bool reAdjustRotary) {
#ifdef BLUETOOTH_ENABLE
	if (!a2dp_sink) {
		return;
	}
	if (_newVolume < int32_t(BLUETOOTHPLAYER_VOLUME_MIN)) {
		return;
	} else if (_newVolume > BLUETOOTHPLAYER_VOLUME_MAX) {
		return;
	} else {
		uint8_t _volume = map(_newVolume, BLUETOOTHPLAYER_VOLUME_MIN, BLUETOOTHPLAYER_VOLUME_MAX, 0, 0x7F);
		a2dp_sink->set_volume(_volume);
		if (reAdjustRotary) {
			RotaryEncoder_Readjust();
		}
	}
#endif
}

bool Bluetooth_Source_SendAudioData(int32_t *outBuff, int16_t validSamples) {
#ifdef BLUETOOTH_ENABLE
	if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && (a2dp_source) && bluetoothSourceConnected && (validSamples > 0)) {
		if (audioSourceRingBuffer == nullptr) {
			if (psramFound()) {
				size_t bufferSize = 16384;
				StaticRingbuffer_t *bufferStruct = (StaticRingbuffer_t *) heap_caps_malloc(sizeof(StaticRingbuffer_t), MALLOC_CAP_SPIRAM);
				uint8_t *bufferStorage = (uint8_t *) heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);

				if (bufferStruct != NULL && bufferStorage != NULL) {
					audioSourceRingBuffer = xRingbufferCreateStatic(bufferSize, RINGBUF_TYPE_BYTEBUF, bufferStorage, bufferStruct); // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
					Log_Printf(LOGLEVEL_INFO, "Bluetooth => audioSourceRingBuffer created in PSRAM on demand (%d bytes, free heap: %u Bytes)", bufferSize, ESP.getFreeHeap());
				} else {
					Log_Println("Failed to allocate PSRAM for audioSourceRingBuffer, using heap", LOGLEVEL_ERROR);
					if (bufferStruct) {
						heap_caps_free(bufferStruct);
					}
					if (bufferStorage) {
						heap_caps_free(bufferStorage);
					}
					audioSourceRingBuffer = xRingbufferCreate(16384, RINGBUF_TYPE_BYTEBUF);
				}
			} else {
				audioSourceRingBuffer = xRingbufferCreate(16384, RINGBUF_TYPE_BYTEBUF);
				Log_Printf(LOGLEVEL_INFO, "Bluetooth => audioSourceRingBuffer created in heap on demand (Free heap: %u Bytes)", ESP.getFreeHeap());
			}

			if (audioSourceRingBuffer == NULL) {
				Log_Println("Failed to create audioSourceRingBuffer!", LOGLEVEL_ERROR);
				return false;
			}
		}

		const TickType_t sendTimeout = pdMS_TO_TICKS(20);
		return (pdTRUE == xRingbufferSend(audioSourceRingBuffer, outBuff, 2 * 2 * validSamples, sendTimeout));
	} else {
		return false;
	}
#else
	return false;
#endif
}

bool Bluetooth_Device_Connected() {
#ifdef BLUETOOTH_ENABLE
	return (((System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) && (a2dp_sink) && a2dp_sink->is_connected()) || ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && (a2dp_source) && bluetoothSourceConnected));
#else
	return false;
#endif
}
