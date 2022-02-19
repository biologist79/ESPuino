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
#endif

#ifdef BLUETOOTH_ENABLE
    #define BLUETOOTHPLAYER_VOLUME_MAX 21u
    #define BLUETOOTHPLAYER_VOLUME_MIN 0u

    BluetoothA2DPSink *a2dp_sink;
#endif

#ifdef BLUETOOTH_ENABLE
// for esp_a2d_connection_state_t see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_a2dp.html#_CPPv426esp_a2d_connection_state_t
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
    snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => connection: %s", a2dp_sink->to_str(state));
    Log_Println(Log_Buffer, LOGLEVEL_INFO);
}
#endif

#ifdef BLUETOOTH_ENABLE
// for esp_a2d_audio_state_t see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_a2dp.html#_CPPv421esp_a2d_audio_state_t
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr){
    snprintf(Log_Buffer, Log_BufferLength, "Bluetooth => audio: %s", a2dp_sink->to_str(state));
    Log_Println(Log_Buffer, LOGLEVEL_INFO);
}
#endif

#ifdef BLUETOOTH_ENABLE
// handle Bluetooth AVRC metadata
// https://docs.espressif.com/projects/esp-idf/en/release-v3.2/api-reference/bluetooth/esp_avrc.html
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
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
        if (System_GetOperationMode() == OPMODE_BLUETOOTH) {
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
            // start bluetooth sink
            a2dp_sink->start((char *)FPSTR(nameBluetoothDevice)); 
            snprintf(Log_Buffer, Log_BufferLength, "Bluetooth sink started, Device: %s", (char *)FPSTR(nameBluetoothDevice));
            Log_Println(Log_Buffer, LOGLEVEL_INFO);
            // connect events after startup
            a2dp_sink->set_on_connection_state_changed(connection_state_changed);
            a2dp_sink->set_on_audio_state_changed(audio_state_changed);
            a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
            a2dp_sink->set_on_volumechange(Bluetooth_VolumeChanged);
        } else {
            esp_bt_mem_release(ESP_BT_MODE_BTDM);
        }
    #endif
} 

void Bluetooth_Cyclic(void) {
    #ifdef BLUETOOTH_ENABLE
        if (a2dp_sink) {
            esp_a2d_audio_state_t state = a2dp_sink->get_audio_state();
            // Reset Sleep Timer when audio is playing
            if (state == ESP_A2D_AUDIO_STATE_STARTED) {
                System_UpdateActivityTimer();
            }
        }
    #endif
}

void Bluetooth_PlayPauseTrack(void) {
    #ifdef BLUETOOTH_ENABLE
        if (a2dp_sink) {
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
        if (a2dp_sink) {
            a2dp_sink->next();
        } 
    #endif
}

void Bluetooth_PreviousTrack(void) {
    #ifdef BLUETOOTH_ENABLE
        if (a2dp_sink) {
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
