
#include "CardMan.h"
#include "Log.h"
#include "base64url.h"
#include <HTTPClient.h>
#include "SdCard.h"
#include "AudioPlayer.h"

#define CHUNK_SIZE      1024

 
cardmanData cardData;
MFRC522::MIFARE_Key cardman_key; //create a MIFARE_Key struct named 'key', which will hold the card information

void CardMan_Init() {
        for (byte i = 0; i < 6; i++) {
                cardman_key.keyByte[i] = 0xFF; //keyByte is defined in the "MIFARE_Key" 'struct' definition in the .h file of the library
        }
}



int CardMan_readBlock(MFRC522 *frc, int blockNumber, byte arrayAddress[]) 
{
    Log_Printf(LOGLEVEL_INFO, "CardMan_readBlock: %d", blockNumber);

  int largestModulo4Number=blockNumber/4*4;
  int trailerBlock=largestModulo4Number+3;//determine trailer block for the sector

  /*****************************************authentication of the desired block for access***********************************************************/
 Log_Println("PCD_Authenticate() ...", LOGLEVEL_INFO);

  MFRC522::StatusCode status = frc->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &cardman_key, &(frc->uid));
  //byte PCD_Authenticate(byte command, byte blockAddr, MIFARE_Key *key, Uid *uid);
  //this method is used to authenticate a certain block for writing or reading
  //command: See enumerations above -> PICC_CMD_MF_AUTH_KEY_A = 0x60 (=1100000),    // this command performs authentication with Key A
  //blockAddr is the number of the block from 0 to 15.
  //MIFARE_Key *key is a pointer to the MIFARE_Key struct defined above, this struct needs to be defined for each block. New cards have all A/B= FF FF FF FF FF FF
  //Uid *uid is a pointer to the UID struct that contains the user ID of the card.
  if (status != MFRC522::STATUS_OK) {
     	Log_Printf(LOGLEVEL_ERROR, "PCD_Authenticate() failed (read):  %s", (const char*)frc->GetStatusCodeName(status));
        frc->PICC_DumpToSerial(&(frc->uid));
        return 3;//return "3" as error message
  }
  //it appears the authentication needs to be made before every block read/write within a specific sector.
  //If a different sector is being authenticated access to the previous one is lost.

 Log_Println("MIFARE_Read() ...", LOGLEVEL_INFO);

  /*****************************************reading a block***********************************************************/
        
  byte buffersize = 18;//we need to define a variable with the read buffer size, since the MIFARE_Read method below needs a pointer to the variable that contains the size... 
  status = frc->MIFARE_Read(blockNumber, arrayAddress, &buffersize);//&buffersize is a pointer to the buffersize variable; MIFARE_Read requires a pointer instead of just a number
  if (status != MFRC522::STATUS_OK) {
    	Log_Printf(LOGLEVEL_ERROR, "MIFARE_read() failed: %s", (const char*)frc->GetStatusCodeName(status));
        return 4;//return "4" as error message
  }

  Log_Printf(LOGLEVEL_DEBUG, "block was read: %d", blockNumber);
  return 0;
}

int CardMan_filldata(MFRC522 *frc, cardmanData *data) {
    Log_Println("CardMan_filldata()", LOGLEVEL_INFO);


    byte blockbuffer[18];
    if(CardMan_readBlock(frc, 4, blockbuffer) != 0) return 1;
    data->cardType = blockbuffer[8];
    if(data->cardType == 0) return 0;

    data->playMode = blockbuffer[9];
    data->trackNr  = blockbuffer[10];

    if(data->cardType == 17 || data->cardType == 18) {
        if(CardMan_readBlock(frc, 5, blockbuffer) != 0) return 1;
        memcpy(&data->uuid, &blockbuffer, 16);
        base64url_encode((unsigned char *)data->uuid, 16, data->uuid64);  
        Log_Printf(LOGLEVEL_INFO, "Cardman UUID: %s", data->uuid64);
    }
    if(data->cardType != 173) {
        int pos = 0;
        for (int s = 2; s <= 6; s++) {
            for (int b = 0; b <= 2; b++) {           
                if(pos>=192) return 2;
                if(CardMan_readBlock(frc, (s * 4) + b , blockbuffer) != 0) return 1;
                memcpy(&data->uri[pos], &blockbuffer, 16);          
                if(blockbuffer[0] == 0) return 0;
                if(blockbuffer[15] == 0) return 0;
                pos+=16;
            }
        }
        Log_Printf(LOGLEVEL_INFO, "Cardman URI: %s", data->uri);
    }

    return 0; 
}

TaskHandle_t dlTaskHandle = NULL;

bool DownloadFile(HTTPClient* http,  const char* uri,  const char* path,  const char* temp) {
        Log_Println("DownloadFile", LOGLEVEL_INFO);

    bool ret = true;
    Serial.print("http uri: ");
    Serial.print(uri);
    Serial.print(" -> ");
    Serial.println(path);
    if(!gFSystem.exists(path)) {
        http->begin(uri);
        int httpCode = http->GET();
        Serial.print("http result: ");
        Serial.println(httpCode);
        if(httpCode == 200) {
            File file = gFSystem.open(temp, FILE_WRITE);

            // ********************* DOWNLOAD PROCESS *********************
            uint8_t data[CHUNK_SIZE];

            const size_t TOTAL_SIZE = http->getSize();
            Serial.print("TOTAL SIZE : ");
            Serial.println(TOTAL_SIZE);
            size_t downloadRemaining = TOTAL_SIZE;
            Serial.println("Download START");
            WiFiClient* stream = http->getStreamPtr();
            auto start_ = millis();
            size_t data_size;
            while ( downloadRemaining > 0 && http->connected() ) {
                data_size = stream->available();
                if (data_size > 0) {
                    auto read_count = stream->read(data, ((data_size > CHUNK_SIZE) ? CHUNK_SIZE : data_size));                    
                    // If one chunk of data has been accumulated, write to SD card
                    if (read_count > 0) 
                    {
                        downloadRemaining -= read_count;
                        file.write(data, read_count);                   
                    }
                }
                vTaskDelay(1);
            }

            size_t time_ = (millis() - start_) / 1000;
            file.close();
            Serial.println("Download END");
            gFSystem.rename(temp, path);
            String speed_ = String(TOTAL_SIZE / time_);
            Serial.println("Speed: " + speed_ + " bytes/sec");
            ret = true;
        } else {
            Serial.print("HTTP Failed, Status: ");
            Serial.println(httpCode);
            ret = false;
        }
        http->end();
    }
    return ret;
    
}

void DownloadUri(void * parameter) {
    HTTPClient http;

    char uri[255];
    char buid[26];
    char path[100];
    char temp[50];
    snprintf(buid, sizeof(buid), "/%s", cardData.uuid64);               
    if(!gFSystem.exists(buid)) gFSystem.mkdir(buid);
    snprintf(temp, sizeof(temp), "%s/download.tmp", buid);

    if(cardData.cardType == 18) {
        snprintf(path, sizeof(path), "%s.m3c", buid);
        snprintf(uri, sizeof(uri), "%s", cardData.uri);
        bool ret = false;
        uint8_t track = 0;
        ret = DownloadFile(&http, uri, path, temp);

        if(ret) {
            File cat = gFSystem.open(path, FILE_READ);
            size_t n;
            while (ret && (n = cat.readBytesUntil('\n', uri, sizeof(uri)-1)) > 0) {
                uri[n] = 0;
                if(uri[0] == '#') continue;               
                Serial.println(uri);
                track++;
                snprintf(path, sizeof(path), "%s/%03u.mp3", buid, track);
                if(n>10 && !gFSystem.exists(path)) {
                   ret = DownloadFile(&http, uri, path, temp);          
                }
            }
        }
        snprintf(path, sizeof(path), "%s/", buid);
        AudioPlayer_TrackQueueDispatcher(path,0,cardData.playMode,0);
    } else {
        snprintf(path, sizeof(path), "%s/%03u.mp3", buid, cardData.trackNr);
        DownloadFile(&http, cardData.uri, path, temp);
        AudioPlayer_TrackQueueDispatcher(path,0,cardData.playMode,0);
    }

    dlTaskHandle = NULL;
    vTaskDelete(NULL);
}

String CardMan_Handle(cardmanData *data) {
    Log_Println("CardMan_Handle()", LOGLEVEL_INFO);

    if (cardData.cardType > 0) {              
        char sb[255];
        //// #<file/folder>#<startPlayPositionInBytes>#<playmode>#<trackNumberToStartWith>
        if(cardData.cardType == 1 ) {
            snprintf(sb, sizeof(sb), "#%s#0#%u#0", cardData.uri, cardData.playMode);
        }
        else if(cardData.cardType == 173 ) {
            snprintf(sb, sizeof(sb), "#0#0#%u#0", cardData.playMode);
        }
        else if(cardData.cardType == 17 ) {
            char ph[50];
            snprintf(ph, sizeof(ph), "/%s/%03u.mp3", cardData.uuid64, cardData.trackNr);
            if(!gFSystem.exists(ph)) {
                if(dlTaskHandle == NULL) xTaskCreate(DownloadUri, "DownloadUri", 6000, NULL, 1, &dlTaskHandle);
                return "-1";
            } 
            else {
                snprintf(sb, sizeof(sb), "#/%s/%03u.mp3#0#%u#0", cardData.uuid64, cardData.trackNr, cardData.playMode);
            }
        }
        else if(cardData.cardType == 18 ) {
            if(cardData.playMode<5 || cardData.playMode>9 || cardData.playMode==8) cardData.playMode = 6;
            if(dlTaskHandle == NULL) xTaskCreate(DownloadUri, "DownloadUri", 6000, NULL, 1, &dlTaskHandle);
            snprintf(sb, sizeof(sb), "#/%s#0#%u#0", cardData.uuid64, cardData.playMode);
        }
        return String(sb);
    }
    return "-1";
}
