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
	OperationMode
} WebsocketCodeType;

typedef enum class ToastStatus {
  Silent = 0,
  Success,
  Error
} ToastStatusType;

void Web_Cyclic(void);
void Web_SendWebsocketData(uint32_t client, WebsocketCodeType code);
