#include <Arduino.h>
#include <AudioPlayer.h>
#include "settings.h"
#include "Bluetooth.h"
#include "Log.h"
#include "RotaryEncoder.h"
#include "System.h"

#ifdef BLUETOOTH_ENABLE
	#include "esp_bt.h"
	#include "BluetoothA2DPSink.h"
	#include "BluetoothA2DPSource.h"
#endif

#ifdef BLUETOOTH_ENABLE
	#define BLUETOOTHPLAYER_VOLUME_MAX 21u
	#define BLUETOOTHPLAYER_VOLUME_MIN 0u

	BluetoothA2DPSink *a2dp_sink;
	BluetoothA2DPSource *a2dp_source;
	RingbufHandle_t audioSourceRingBuffer;
#endif


#ifdef BLUETOOTH_ENABLE
	// for esp_a2d_connection_state_t see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_a2dp.html#_CPPv426esp_a2d_connection_state_t
	void connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
		if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
			snprintf(Log_Buffer, Log_BufferLength, "Bluetooth sink => connection state: %s", a2dp_sink->to_str(state));
		} else {
			snprintf(Log_Buffer, Log_BufferLength, "Bluetooth source => connection state: %s", a2dp_source->to_str(state));
		}
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
	}
#endif


#ifdef BLUETOOTH_ENABLE
// for esp_a2d_audio_state_t see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_a2dp.html#_CPPv421esp_a2d_audio_state_t
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
	if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
        	snprintf(Log_Buffer, Log_BufferLength, "Bluetooth sink => audio state: %s", a2dp_sink->to_str(state));
    	} else {
        	snprintf(Log_Buffer, Log_BufferLength, "Bluetooth source => audio state: %s", a2dp_source->to_str(state));
    	}
    	Log_Println(Log_Buffer, LOGLEVEL_INFO);
}
#endif

#ifdef BLUETOOTH_ENABLE
	// handle Bluetooth AVRC metadata
	// https://docs.espressif.com/projects/esp-idf/en/release-v3.2/api-reference/bluetooth/esp_avrc.html
	void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
		if (strlen((char *)text) == 0)
			return;
		switch (id) {
			case ESP_AVRC_MD_ATTR_TITLE:
				// title
				snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => AVRC Title: %s", text);
				break;
			case ESP_AVRC_MD_ATTR_ARTIST:
				// artists
				snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => AVRC Artist: %s", text);
				break;
			case ESP_AVRC_MD_ATTR_ALBUM:
				// album
				snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => AVRC Album: %s", text);
				break;
			case ESP_AVRC_MD_ATTR_TRACK_NUM:
				// current track number
				snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => AVRC Track-No: %s", text);
				break;
			case ESP_AVRC_MD_ATTR_NUM_TRACKS:
				// number of tracks in playlist
				snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => AVRC Number of tracks: %s", text);
				break;
			case ESP_AVRC_MD_ATTR_GENRE:
				// genre
				snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => AVRC Genre: %s", text);
				break;
			default:
				// unknown/unsupported metadata
				snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => AVRC metadata rsp: attribute id 0x%x, %s", id, text);
				break;
		}
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
	}
#endif

#ifdef BLUETOOTH_ENABLE
	// feed the A2DP source with audio data
	int32_t get_data_channels(Frame *frame, int32_t channel_len) {
		if (channel_len < 0 || frame == NULL)
			return 0;
		// Receive data from ring buffer
		size_t len{};
		#if ESP_ARDUINO_VERSION_MAJOR >= 2
			vRingbufferGetInfo(audioSourceRingBuffer, nullptr, nullptr, nullptr, nullptr, &len);
		#else
			vRingbufferGetInfo(audioSourceRingBuffer, nullptr, nullptr, nullptr, &len);
		#endif
		if (len < (channel_len * 4)) {
			// Serial.println("Bluetooth source => not enough data");
			return 0;
		};
		size_t sampleSize = 0;
		uint8_t* sampleBuff;
		sampleBuff = (uint8_t *)xRingbufferReceiveUpTo(audioSourceRingBuffer, &sampleSize, (portTickType)portMAX_DELAY, channel_len * 4);
		if (sampleBuff != NULL) {
			// fill the channel data
			for (int sample = 0; sample < (channel_len); ++sample) {
				frame[sample].channel1 = (sampleBuff[sample * 4 + 3] << 8) | sampleBuff[sample * 4 + 2];
				frame[sample].channel2 = (sampleBuff[sample * 4 + 1] << 8) | sampleBuff[sample * 4];
			};
			vRingbufferReturnItem(audioSourceRingBuffer, (void *)sampleBuff);
		};
		return channel_len;
	};
#endif

#ifdef BLUETOOTH_ENABLE
	// callback which is notified on update Receiver RSSI
	void rssi(esp_bt_gap_cb_param_t::read_rssi_delta_param  &rssiParam){
	snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => RSSI value: %d", rssiParam.rssi_delta);
	Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
	}
#endif


void Bluetooth_VolumeChanged(int _newVolume) {
	#ifdef BLUETOOTH_ENABLE
		snprintf(Log_Buffer, Log_BufferLength, "%s %d !", (char *) FPSTR("Bluetooth => volume changed: "), _newVolume);
		Log_Println(Log_Buffer, LOGLEVEL_INFO);
		uint8_t _volume;
		if (_newVolume < BLUETOOTHPLAYER_VOLUME_MIN) {
			return;
		} else if (_newVolume > BLUETOOTHPLAYER_VOLUME_MAX) {
			return;
		} else {
			// map bluetooth volume (0..127) to ESPuino volume (0..21) to
			_volume = map(_newVolume, 0, 0x7F, BLUETOOTHPLAYER_VOLUME_MIN, BLUETOOTHPLAYER_VOLUME_MAX);
			AudioPlayer_SetCurrentVolume(_volume);
			// update rotary encoder
			RotaryEncoder_Readjust();
		}
	#endif
}

void Bluetooth_Init(void) {
	#ifdef BLUETOOTH_ENABLE
        	if (System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) {
            		// bluetooth in sink mode (player acts as a BT-Speaker)
			a2dp_sink = new BluetoothA2DPSink();
			i2s_pin_config_t pin_config = {
				.bck_io_num = I2S_BCLK,
				.ws_io_num = I2S_LRC,
				.data_out_num = I2S_DOUT,
				.data_in_num = I2S_PIN_NO_CHANGE};
			a2dp_sink->set_pin_config(pin_config);
			a2dp_sink->activate_pin_code(false);
			#ifdef PLAY_MONO_SPEAKER
				a2dp_sink->set_mono_downmix(true);
			#endif
			a2dp_sink->set_auto_reconnect(true);
			a2dp_sink->set_rssi_active(true);
			a2dp_sink->set_rssi_callback(rssi);
			// start bluetooth sink
			a2dp_sink->start((char *)FPSTR(nameBluetoothSinkDevice));
			snprintf(Log_Buffer, Log_BufferLength, "Bluetooth sink started, Device: %s", (char *)FPSTR(nameBluetoothSinkDevice));
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
			// connect events after startup
			a2dp_sink->set_on_connection_state_changed(connection_state_changed);
			a2dp_sink->set_on_audio_state_changed(audio_state_changed);
			a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
			a2dp_sink->set_on_volumechange(Bluetooth_VolumeChanged);
        	} else if (System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) {
			// create audio source ringbuffer on demand
			audioSourceRingBuffer = xRingbufferCreate(8192, RINGBUF_TYPE_BYTEBUF);
			if (audioSourceRingBuffer == NULL)
				Log_Println("cannot create audioSourceRingBuffer!", LOGLEVEL_ERROR);
			//  setup BT source
			a2dp_source = new BluetoothA2DPSource();

			//a2dp_source->set_auto_reconnect(false);   // auto reconnect
			//a2dp_source->set_task_core(1);            // task core
			//a2dp_source->set_nvs_init(true);          // erase/initialize NVS
			//a2dp_source->set_ssp_enabled(true);       // enable secure simple pairing

			// start bluetooth source
			String btDeviceName = gPrefsSettings.getString("btDeviceName", nameBluetoothSourceDevice);
			a2dp_source->start(btDeviceName.c_str(), get_data_channels);
			snprintf(Log_Buffer, Log_BufferLength, "Bluetooth source started, connect to device: '%s'", (char *)FPSTR(btDeviceName.c_str()));
			Log_Println(Log_Buffer, LOGLEVEL_INFO);
			// connect events after startup
			a2dp_source->set_on_connection_state_changed(connection_state_changed);
			a2dp_source->set_on_audio_state_changed(audio_state_changed);
			// max headphone volume (0..255): volume is controlled by audio class
			a2dp_source->set_volume(127);
		} else {
			esp_bt_mem_release(ESP_BT_MODE_BTDM);
		}
	#endif
}

void Bluetooth_Cyclic(void) {
	#ifdef BLUETOOTH_ENABLE
		if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SINK) && (a2dp_sink)) {
			esp_a2d_audio_state_t state = a2dp_sink->get_audio_state();
			// Reset Sleep Timer when audio is playing
			if (state == ESP_A2D_AUDIO_STATE_STARTED) {
				System_UpdateActivityTimer();
			}
		}
		if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && (a2dp_source)) {
			esp_a2d_audio_state_t state = a2dp_source->get_audio_state();
			// Reset Sleep Timer when audio is playing
			if (state == ESP_A2D_AUDIO_STATE_STARTED) {
				System_UpdateActivityTimer();
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

// set volume from ESPuino to phone needs at least Arduino ESP version 2.0.0
void Bluetooth_SetVolume(const int32_t _newVolume, bool reAdjustRotary) {
	#ifdef BLUETOOTH_ENABLE
		if (!a2dp_sink)
			return;
		uint8_t _volume;
		if (_newVolume < BLUETOOTHPLAYER_VOLUME_MIN) {
			return;
		} else if (_newVolume > BLUETOOTHPLAYER_VOLUME_MAX) {
			return;
		} else {
			// map ESPuino min/max volume (0..21) to bluetooth volume (0..127)
			_volume = map(_newVolume, BLUETOOTHPLAYER_VOLUME_MIN, BLUETOOTHPLAYER_VOLUME_MAX, 0, 0x7F);
			a2dp_sink->set_volume(_volume);
			if (reAdjustRotary) {
				RotaryEncoder_Readjust();
			}
		}
	#endif
}

bool Bluetooth_Source_SendAudioData(uint32_t* sample) {
	#ifdef BLUETOOTH_ENABLE
		// send audio data to ringbuffer
		if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && (a2dp_source) && a2dp_source->is_connected()) {
			return (pdTRUE == xRingbufferSend(audioSourceRingBuffer, sample, sizeof(uint32_t), (portTickType)portMAX_DELAY));
		} else {
			return false;
		}
	#else
		return false;
	#endif
}

bool Bluetooth_Source_Connected() {
	#ifdef BLUETOOTH_ENABLE
		// send audio data to ringbuffer
		if ((System_GetOperationMode() == OPMODE_BLUETOOTH_SOURCE) && (a2dp_source) && a2dp_source->is_connected()) {
			return true;
		} else {
			return false;
		}
	#else
		return false;
	#endif
}
