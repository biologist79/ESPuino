#include <Arduino.h>
#include "settings.h"
#include "i2c.h"

#if (HAL == 2)
    TwoWire i2cBusOne = TwoWire(0);
#endif

#if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE) || defined (PORT_TOUCHMPR121_ENABLE)
    TwoWire i2cBusTwo = TwoWire(1);
#endif

void I2C_Init() {

    #if (HAL == 2)
        i2cBusOne.begin(IIC_DATA, IIC_CLK, 40000);
//        loggerNl(serialDebug, (char *) FPSTR(i2cB1Active), LOGLEVEL_DEBUG);
    #endif

    #if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE) || defined (PORT_TOUCHMPR121_ENABLE)
        I2C_clear_lines(ext_IIC_DATA, ext_IIC_CLK);
        i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK, 40000);
        delay(50);
//        loggerNl(serialDebug, (char *) FPSTR(i2cB2Active), LOGLEVEL_DEBUG);
    #endif
}

void I2C_clear_lines(int PIN_SDA, int PIN_SCL) {
    boolean stuck_transaction = false;
    uint8_t stuck_transaction_retry_count = 0;
    const uint8_t stuck_transaction_retry_MAX = 10;

    ::pinMode( PIN_SDA, INPUT_PULLUP );
    ::pinMode( PIN_SCL, INPUT_PULLUP );

    do{
        if(( ::digitalRead( PIN_SDA ) == LOW ) && ( ::digitalRead( PIN_SCL ) == HIGH )){
        stuck_transaction = true;
        ::pinMode( PIN_SCL, OUTPUT );
        ::digitalWrite( PIN_SCL, LOW );
        delay( 1 ); // this is way longer than required (would be 1.25us at 400kHz) but err on side of caution
        ::pinMode( PIN_SCL, INPUT_PULLUP );
        stuck_transaction_retry_count++;
        } else {
        stuck_transaction = false;
        }
    } while ( stuck_transaction && ( stuck_transaction_retry_count < stuck_transaction_retry_MAX ));

    // TODO: add new error code that can be handled externally
    if( stuck_transaction_retry_count > 0){
        if( stuck_transaction ){
        } else {
        }
    }
}