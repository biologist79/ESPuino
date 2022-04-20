#include <Arduino.h>
#include "settings.h"
#include "i2c.h"

SemaphoreHandle_t xSemaphore_I2C = NULL;
void (*execFunction)(void);

// initialise TwoWire for AC101 
#if (HAL == 2)
    extern TwoWire i2cBusOne = TwoWire(0);
    extern AC101 ac(&i2cBusOne);
#endif

// initialise TwoWire for other Devices

#ifdef I2C_2_ENABLE
    extern TwoWire i2cBusTwo = TwoWire(1);
#endif

void i2c_Init() {

    #if (HAL == 2)
        i2c_clear_lines(IIC_DATA, IIC_CLK);
        i2cBusOne.begin(IIC_DATA, IIC_CLK, 100000L);

        ac.begin();
        pinMode(22, OUTPUT);
        digitalWrite(22, HIGH);

        pinMode(GPIO_PA_EN, OUTPUT);
        digitalWrite(GPIO_PA_EN, HIGH);

    #endif

    #if defined(RFID_READER_TYPE_MFRC522_I2C) || defined(PORT_EXPANDER_ENABLE) || defined (PORT_TOUCHMPR121_ENABLE)
        i2c_clear_lines(ext_IIC_DATA, ext_IIC_CLK);
        i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK, 100000L);

        // Create Mutex for i2c_access to avoid data collision
        xSemaphore_I2C = xSemaphoreCreateMutex();
    #endif
}

void i2c_clear_lines(int PIN_SDA, int PIN_SCL) {
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

void i2c_tsafe_execute(void (*execFunction)(void), int waitTicks = 15) {
  /* See if semaphore is initialized */
  if(xSemaphore_I2C != NULL) {
        /* See if we can obtain the semaphore.  If the semaphore is not
        available wait defined number of ticks to see if it becomes free. */
        if( xSemaphoreTake( xSemaphore_I2C, ( TickType_t ) waitTicks ) == pdTRUE )
        {
            /* We were able to obtain the semaphore and can now access the
            shared resource. */
            execFunction();
            /* We have finished accessing the shared resource.  Release the
            semaphore. */
            xSemaphoreGive( xSemaphore_I2C );
        }
        else
        {
            /* We could not obtain the semaphore and can therefore not access
            the shared resource safely. Throw Error */
            Serial.print("Semaphore could not be taken within ");
            Serial.print(waitTicks);
            Serial.print(" Ticks!");
        }
    }
}

void i2c_scanExtBus() {
    byte error, address;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    i2cBusTwo.beginTransmission(address);
    error = i2cBusTwo.endTransmission();
    if (error == 0) {
      Serial.print("external I2C-Bus: device found at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
      nDevices++;
    }
    else if (error==4) {
      Serial.print("Unknow error at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  }
  else {
    Serial.println("done\n");
  }
}
