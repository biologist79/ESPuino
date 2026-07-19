#pragma once

typedef enum class WebsocketCode {
	Ok = 0,
	Error,
	Dropout,
	CurrentRfid,
	Pong,
	TrackInfo,
	CoverImg,
	Volume,
	Settings,
	Ssid,
	TrackProgress,
	OperationMode,
	NotAllowedInCurrentMode,
	BluetoothScanInProgress,
	BluetoothScanComplete,
	// Request was a read-only data fetch (ssids/settings/trackinfo/coverimg/volume/...) that
	// already sent its own specific response - the caller should not also forward Ok/an ack
	// for it, unlike a genuine settings-save or control action.
	Silent
} WebsocketCodeType;

void Web_Cyclic(void);
void Web_Exit(void);
void Web_SendWebsocketData(uint32_t client, WebsocketCodeType code);
