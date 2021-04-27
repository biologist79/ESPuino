#include <Arduino.h>
#include "settings.h"
#include "Bluetooth.h"
#include "System.h"

#ifdef BLUETOOTH_ENABLE
    #include "esp_bt.h"
    #include "BluetoothA2DPSink.h"
#endif

#ifdef BLUETOOTH_ENABLE
    BluetoothA2DPSink *a2dp_sink;
#endif

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
            a2dp_sink->start((char *)FPSTR(nameBluetoothDevice));
        } else {
            esp_bt_mem_release(ESP_BT_MODE_BTDM);
        }
    #endif
}

void Bluetooth_Cyclic(void) {
    #ifdef BLUETOOTH_ENABLE
        esp_a2d_audio_state_t state = a2dp_sink->get_audio_state();
        // Reset Sleep Timer when audio is playing
        if (state == ESP_A2D_AUDIO_STATE_STARTED) {
            System_UpdateActivityTimer();
        }
    #endif
}
