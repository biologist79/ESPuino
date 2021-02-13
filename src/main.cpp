// !!! MAKE SURE TO EDIT settings.h !!!

#include "settings.h"                                   // Contains all user-relevant settings (general)

// !!! MAKE SURE TO EDIT PLATFORM SPECIFIC settings-****.h !!!
#if (HAL == 1)
    #include "settings-lolin32.h"                       // Contains all user-relevant settings for Wemos Lolin32
#elif (HAL == 2)
    #include "settings-espa1s.h"                        // Contains all user-relevant settings for ESP32-A1S Audiokit
#elif (HAL == 3)
    #include "settings-lolin_d32.h"                     // Contains all user-relevant settings for Wemos Lolin D32
#elif (HAL == 4)
    #include "settings-lolin_d32_pro.h"                 // Contains all user-relevant settings for Wemos Lolin D32 pro
#endif

#include <ESP32Encoder.h>
#include "Arduino.h"
#include <WiFi.h>
#ifdef MDNS_ENABLE
    #include <ESPmDNS.h>
#endif
#ifdef FTP_ENABLE
    #include "ESP32FtpServer.h"
#endif
#ifdef BLUETOOTH_ENABLE
    #include "esp_bt.h"
    #include "BluetoothA2DPSink.h"
#endif
#include "Audio.h"
#include "SPI.h"
#include "FS.h"
#ifdef SD_MMC_1BIT_MODE
    #include "SD_MMC.h"
#else
    #include "SD.h"
#endif
#include "esp_task_wdt.h"
#ifdef RFID_READER_TYPE_MFRC522_SPI
        #include <MFRC522.h>
#endif
#ifdef RFID_READER_TYPE_MFRC522_I2C
    #include "Wire.h"
    #include <MFRC522_I2C.h>
#endif
#ifdef RFID_READER_TYPE_PN5180
    #include <PN5180.h>
    #include <PN5180ISO14443.h>
    #include <PN5180ISO15693.h>
#endif
#include <Preferences.h>
#ifdef MQTT_ENABLE
    #define MQTT_SOCKET_TIMEOUT 1               // https://github.com/knolleary/pubsubclient/issues/403
    #include <PubSubClient.h>
#endif
#include <WebServer.h>
#ifdef NEOPIXEL_ENABLE
    #include <FastLED.h>
#endif

#if (LANGUAGE == 1)
    #include "logmessages.h"
    #include "HTMLmanagement.h"
    #include "HTMLaccesspoint.h"
#endif
#if (LANGUAGE == 2)
    #include "logmessages_EN.h"
    #include "HTMLmanagement_EN.h"
    #include "HTMLaccesspoint_EN.h"
#endif

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <nvsDump.h>

#include "freertos/ringbuf.h"


// Serial-logging buffer
uint8_t serialLoglength = 200;
char *logBuf = (char*) calloc(serialLoglength, sizeof(char)); // Buffer for all log-messages

// FilePathLength
#define MAX_FILEPATH_LENTGH 256

#ifdef HEADPHONE_ADJUST_ENABLE
    bool headphoneLastDetectionState;
    uint32_t headphoneLastDetectionTimestamp = 0;
#endif

#ifdef BLUETOOTH_ENABLE
    BluetoothA2DPSink *a2dp_sink;
#endif

#ifdef MEASURE_BATTERY_VOLTAGE
    uint16_t maxAnalogValue = 4095;                 // Highest value given by analogRead(); don't change!
    uint32_t lastVoltageCheckTimestamp = 0;
    #ifdef NEOPIXEL_ENABLE
        bool showVoltageWarning = false;
    #endif
#endif

#ifdef PLAY_LAST_RFID_AFTER_REBOOT
    bool recoverLastRfid = true;
#endif

// Operation Mode
#define OPMODE_NORMAL                   0           // Normal mode
#define OPMODE_BLUETOOTH                1           // Bluetooth mode. WiFi is deactivated. Music from SD and webstreams can't be played.

// Track-Control
#define STOP                            1           // Stop play
#define PLAY                            2           // Start play (currently not used)
#define PAUSEPLAY                       3           // Pause/play
#define NEXTTRACK                       4           // Next track of playlist
#define PREVIOUSTRACK                   5           // Previous track of playlist
#define FIRSTTRACK                      6           // First track of playlist
#define LASTTRACK                       7           // Last track of playlist

// Playmodes
#define NO_PLAYLIST                     0           // If no playlist is active
#define SINGLE_TRACK                    1           // Play a single track
#define SINGLE_TRACK_LOOP               2           // Play a single track in infinite-loop
#define AUDIOBOOK                       3           // Single track, can save last play-position
#define AUDIOBOOK_LOOP                  4           // Single track as infinite-loop, can save last play-position
#define ALL_TRACKS_OF_DIR_SORTED        5           // Play all files of a directory (alph. sorted)
#define ALL_TRACKS_OF_DIR_RANDOM        6           // Play all files of a directory (randomized)
#define ALL_TRACKS_OF_DIR_SORTED_LOOP   7           // Play all files of a directory (alph. sorted) in infinite-loop
#define ALL_TRACKS_OF_DIR_RANDOM_LOOP   9           // Play all files of a directory (randomized) in infinite-loop
#define WEBSTREAM                       8           // Play webradio-stream
#define BUSY                            10          // Used if playlist is created

// RFID-modifcation-types
#define LOCK_BUTTONS_MOD                100         // Locks all buttons and rotary encoder
#define SLEEP_TIMER_MOD_15              101         // Puts uC into deepsleep after 15 minutes + LED-DIMM
#define SLEEP_TIMER_MOD_30              102         // Puts uC into deepsleep after 30 minutes + LED-DIMM
#define SLEEP_TIMER_MOD_60              103         // Puts uC into deepsleep after 60 minutes + LED-DIMM
#define SLEEP_TIMER_MOD_120             104         // Puts uC into deepsleep after 120 minutes + LED-DIMM
#define SLEEP_AFTER_END_OF_TRACK        105         // Puts uC into deepsleep after track is finished + LED-DIMM
#define SLEEP_AFTER_END_OF_PLAYLIST     106         // Puts uC into deepsleep after playlist is finished + LED-DIMM
#define SLEEP_AFTER_5_TRACKS            107         // Puts uC into deepsleep after five tracks
#define REPEAT_PLAYLIST                 110         // Changes active playmode to endless-loop (for a playlist)
#define REPEAT_TRACK                    111         // Changes active playmode to endless-loop (for a single track)
#define DIMM_LEDS_NIGHTMODE             120         // Changes LED-brightness
#define TOGGLE_WIFI_STATUS              130         // Toggles WiFi-status
#define TOGGLE_BLUETOOTH_MODE           140         // Toggles Normal/Bluetooth Mode
#define ENABLE_FTP_SERVER               150         // Enables FTP-server

// Repeat-Modes
#define NO_REPEAT                       0           // No repeat
#define TRACK                           1           // Repeat current track (infinite loop)
#define PLAYLIST                        2           // Repeat whole playlist (infinite loop)
#define TRACK_N_PLAYLIST                3           // Repeat both (infinite loop)

typedef struct { // Bit field
    uint8_t playMode:                   4;      // playMode
    char **playlist;                            // playlist
    bool repeatCurrentTrack:            1;      // If current track should be looped
    bool repeatPlaylist:                1;      // If whole playlist should be looped
    uint16_t currentTrackNumber:        9;      // Current tracknumber
    uint16_t numberOfTracks:            9;      // Number of tracks in playlist
    unsigned long startAtFilePos;               // Offset to start play (in bytes)
    uint8_t currentRelPos:              7;      // Current relative playPosition (in %)
    bool sleepAfterCurrentTrack:        1;      // If uC should go to sleep after current track
    bool sleepAfterPlaylist:            1;      // If uC should go to sleep after whole playlist
    bool saveLastPlayPosition:          1;      // If playposition/current track should be saved (for AUDIOBOOK)
    char playRfidTag[13];                       // ID of RFID-tag that started playlist
    bool pausePlay:                     1;      // If pause is active
    bool trackFinished:                 1;      // If current track is finished
    bool playlistFinished:              1;      // If whole playlist is finished
    uint8_t playUntilTrackNumber:       6;      // Number of tracks to play after which uC goes to sleep
} playProps;
playProps playProperties;

typedef struct {
    char nvsKey[13];
    char nvsEntry[275];
} nvs_t;

// Operation Mode
volatile uint8_t operationMode;

// Configuration of initial values (for the first start) goes here....
// There's no need to change them here as they can be configured via webinterface
// Neopixel
uint8_t initialLedBrightness = 16;                      // Initial brightness of Neopixel
uint8_t ledBrightness = initialLedBrightness;
uint8_t nightLedBrightness = 2;                         // Brightness of Neopixel in nightmode

// MQTT
bool enableMqtt = true;
#ifdef MQTT_ENABLE
    uint8_t const stillOnlineInterval = 60;             // Interval 'I'm still alive' is sent via MQTT (in seconds)
    uint32_t mqttLastRetryTimestamp = 0;
#endif

uint8_t const cardIdSize = 4;                           // RFID
// Volume
uint8_t maxVolume = 21;                                 // Current maximum volume that can be adjusted
uint8_t maxVolumeSpeaker = 21;                          // Maximum volume that can be adjusted in speaker-mode (default; can be changed later via GUI)
uint8_t minVolume = 0;                                  // Lowest volume that can be adjusted
uint8_t initVolume = 3;                                 // 0...21 (If not found in NVS, this one will be taken) (default; can be changed later via GUI)
#ifdef HEADPHONE_ADJUST_ENABLE
    uint8_t maxVolumeHeadphone = 11;                    // Maximum volume that can be adjusted in headphone-mode (default; can be changed later via GUI)
#endif
// Sleep
uint8_t maxInactivityTime = 10;                         // Time in minutes, after uC is put to deep sleep because of inactivity (and modified later via GUI)
uint8_t sleepTimer = 30;                                // Sleep timer in minutes that can be optionally used (and modified later via MQTT or RFID)
// FTP
uint8_t ftpUserLength = 10;                             // Length will be published n-1 as maxlength to GUI
uint8_t ftpPasswordLength = 15;                         // Length will be published n-1 as maxlength to GUI
char *ftpUser = strndup((char*) "esp32", ftpUserLength);                // FTP-user (default; can be changed later via GUI)
char *ftpPassword = strndup((char*) "esp32", ftpPasswordLength);        // FTP-password (default; can be changed later via GUI)


// Don't change anything here unless you know what you're doing
// HELPER //
// WiFi
unsigned long wifiCheckLastTimestamp = 0;
bool wifiEnabled;                                       // Current status if wifi is enabled
uint32_t wifiStatusToggledTimestamp = 0;
bool webserverStarted = false;
bool wifiNeedsRestart = false;
// Neopixel
#ifdef NEOPIXEL_ENABLE
    bool showLedError = false;
    bool showLedOk = false;
    bool showPlaylistProgress = false;
    bool showRewind = false;
    bool showLedVoltage = false;
    bool pauseNeopixel = false;             // Used to pause Neopixel-signalisation (while NVS-writes as this leads to exceptions; don't know why)
#endif
// MQTT
#ifdef MQTT_ENABLE
    unsigned long lastOnlineTimestamp = 0;
#endif
// RFID
unsigned long lastRfidCheckTimestamp = 0;
char *currentRfidTagId = NULL;
// Sleep
unsigned long lastTimeActiveTimestamp = 0;              // Timestamp of last user-interaction
unsigned long sleepTimerStartTimestamp = 0;             // Flag if sleep-timer is active
bool gotoSleep = false;                                 // Flag for turning uC immediately into deepsleep
bool sleeping = false;                                  // Flag for turning into deepsleep is in progress
bool lockControls = false;                              // Flag if buttons and rotary encoder is locked
bool bootComplete = false;
// Rotary encoder-helper
int32_t lastEncoderValue;
int32_t currentEncoderValue;
int32_t lastVolume = -1;                                // Don't change -1 as initial-value!
uint8_t currentVolume = initVolume;
////////////

// AP-WiFi
IPAddress apIP(192, 168, 4, 1);                         // Access-point's static IP
IPAddress apNetmask(255, 255, 255, 0);                  // Access-point's netmask
bool accessPointStarted = false;


// MQTT-configuration
// Please note: all lengths will be published n-1 as maxlength to GUI
uint8_t mqttServerLength = 32;
uint8_t mqttUserLength = 16;
uint8_t mqttPasswordLength = 16;

// Please note: all of them are defaults that can be changed later via GUI
char *mqtt_server = strndup((char*) "192.168.2.43", mqttServerLength);      // IP-address of MQTT-server (if not found in NVS this one will be taken)
char *mqttUser = strndup((char*) "mqtt-user", mqttUserLength);              // MQTT-user
char *mqttPassword = strndup((char*) "mqtt-password", mqttPasswordLength);  // MQTT-password*/


char stringDelimiter[] = "#";                               // Character used to encapsulate data in linear NVS-strings (don't change)
char stringOuterDelimiter[] = "^";                          // Character used to encapsulate encapsulated data along with RFID-ID in backup-file


void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}
AsyncWebServer wServer(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");


// Audio/mp3
#ifndef SD_MMC_1BIT_MODE
    SPIClass spiSD(HSPI);
    fs::FS FSystem = (fs::FS)SD;
#else
    fs::FS FSystem = (fs::FS)SD_MMC;
#endif

TaskHandle_t mp3Play;
TaskHandle_t rfid;
TaskHandle_t fileStorageTaskHandle;

#ifdef NEOPIXEL_ENABLE
    TaskHandle_t LED;
#endif

#if (HAL == 2)
    #include "AC101.h"
    static TwoWire i2cBusOne = TwoWire(0);
    static AC101 ac(i2cBusOne);
#endif
#ifdef RFID_READER_TYPE_MFRC522_SPI
    static MFRC522 mfrc522(RFID_CS, RST_PIN);
#endif
#ifdef RFID_READER_TYPE_MFRC522_I2C
    static TwoWire i2cBusTwo = TwoWire(1);
    static MFRC522 mfrc522(MFRC522_ADDR, MFRC522_RST_PIN, i2cBusTwo);
#endif

// FTP
#ifdef FTP_ENABLE
    FtpServer *ftpSrv;      // Heap-alloction takes place later (when needed)
    bool ftpEnableLastStatus = false;
    bool ftpEnableCurrentStatus = false;
#endif

// Info: SSID / password are stored in NVS
WiFiClient wifiClient;
IPAddress myIP;

// MQTT-helper
#ifdef MQTT_ENABLE
    PubSubClient MQTTclient(wifiClient);
#endif

// Rotary encoder-configuration
ESP32Encoder encoder;

// HW-Timer
hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;

// Button-helper
typedef struct {
    bool lastState;
    bool currentState;
    bool isPressed;
    bool isReleased;
    unsigned long lastPressedTimestamp;
    unsigned long lastReleasedTimestamp;
} t_button;
t_button buttons[4];

Preferences prefsRfid;
Preferences prefsSettings;
static const char prefsRfidNamespace[] PROGMEM = "rfidTags";        // Namespace used to save IDs of rfid-tags
static const char prefsSettingsNamespace[] PROGMEM = "settings";    // Namespace used for generic settings

QueueHandle_t volumeQueue;
QueueHandle_t trackQueue;
QueueHandle_t trackControlQueue;
QueueHandle_t rfidCardQueue;

RingbufHandle_t explorerFileUploadRingBuffer;
QueueHandle_t explorerFileUploadStatusQueue;

// Prototypes
void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask);
static int arrSortHelper(const void* a, const void* b);
#ifdef MQTT_ENABLE
    void callback(const char *topic, const byte *payload, uint32_t length);
#endif
void batteryVoltageTester(void);
void buttonHandler();
void deepSleepManager(void);
void doButtonActions(void);
void doRfidCardModifications(const uint32_t mod);
bool dumpNvsToSd(char *_namespace, char *_destFile);
bool endsWith (const char *str, const char *suf);
bool fileValid(const char *_fileItem);
void freeMultiCharArray(char **arr, const uint32_t cnt);
uint8_t getRepeatMode(void);
bool getWifiEnableStatusFromNVS(void);
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void convertAsciiToUtf8(String asciiString, char *utf8String);
void convertUtf8ToAscii(String utf8String, char *asciiString);
void explorerHandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void explorerHandleFileStorageTask(void *parameter);
void explorerHandleListRequest(AsyncWebServerRequest *request);
void explorerHandleDeleteRequest(AsyncWebServerRequest *request);
void explorerHandleCreateRequest(AsyncWebServerRequest *request);
void explorerHandleRenameRequest(AsyncWebServerRequest *request);
void explorerHandleAudioRequest(AsyncWebServerRequest *request);
void headphoneVolumeManager(void);
bool isNumber(const char *str);
void loggerNl(const uint8_t _currentLogLevel, const char *str, const uint8_t _logLevel);
void logger(const uint8_t _currentLogLevel, const char *str, const uint8_t _logLevel);
float measureBatteryVoltage(void);
#ifdef MQTT_ENABLE
    bool publishMqtt(const char *topic, const char *payload, bool retained);
#endif
size_t nvsRfidWriteWrapper (const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks);
void onWebsocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
#ifdef MQTT_ENABLE
    void postHeartbeatViaMqtt(void);
#endif
bool processJsonRequest(char *_serialJson);
void randomizePlaylist (char *str[], const uint32_t count);
#ifdef MQTT_ENABLE
    bool reconnect();
#endif
char ** returnPlaylistFromWebstream(const char *_webUrl);
char ** returnPlaylistFromSD(File _fileOrDirectory);
void rfidScanner(void *parameter);
void sleepHandler(void) ;
void sortPlaylist(const char** arr, int n);
bool startsWith(const char *str, const char *pre);
String templateProcessor(const String& templ);
void trackControlToQueueSender(const uint8_t trackCommand);
void rfidPreferenceLookupHandler (void);
void sendWebsocketData(uint32_t client, uint8_t code);
void setupVolume(void);
void trackQueueDispatcher(const char *_sdFile, const uint32_t _lastPlayPos, const uint32_t _playMode, const uint16_t _trackLastPlayed);
void volumeHandler(const int32_t _minVolume, const int32_t _maxVolume);
void volumeToQueueSender(const int32_t _newVolume);
wl_status_t wifiManager(void);
bool writeWifiStatusToNVS(bool wifiStatus);
void bluetoothHandler(void);
uint8_t readOperationModeFromNVS(void);
bool setOperationMode(uint8_t operationMode);


/* Wrapper-function for serial-logging (with newline)
    _currentLogLevel: loglevel that's currently active
    _logBuffer: char* to log
    _minLogLevel: loglevel configured for this message.

    If (_currentLogLevel <= _minLogLevel) message will be logged
*/
void loggerNl(const uint8_t _currentLogLevel, const char *_logBuffer, const uint8_t _minLogLevel) {
  if (_currentLogLevel >= _minLogLevel) {
    Serial.println(_logBuffer);
  }
}

/* Wrapper-function for serial-logging (without newline) */
void logger(const uint8_t _currentLogLevel, const char *_logBuffer, const uint8_t _minLogLevel) {
  if (_currentLogLevel >= _minLogLevel) {
    Serial.print(_logBuffer);
  }
}


int countChars(const char* string, char ch) {
    int count = 0;
    int length = strlen(string);

    for (uint8_t i = 0; i < length; i++) {
        if (string[i] == ch) {
            count++;
        }
    }

    return count;
}

void IRAM_ATTR onTimer() {
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}


#ifdef PLAY_LAST_RFID_AFTER_REBOOT
    // Store last RFID-tag to NVS
    void storeLastRfidPlayed(char *_rfid) {
        prefsSettings.putString("lastRfid", (String) _rfid);
    }

    // Get last RFID-tag applied from NVS
    void recoverLastRfidPlayed(void) {
        if (recoverLastRfid) {
            recoverLastRfid = false;
            String lastRfidPlayed = prefsSettings.getString("lastRfid", "-1");
            if (!lastRfidPlayed.compareTo("-1")) {
                loggerNl(serialDebug,((char *) FPSTR(unableToRestoreLastRfidFromNVS), LOGLEVEL_INFO);
            } else {
                char *lastRfid = strdup(lastRfidPlayed.c_str());
                xQueueSend(rfidCardQueue, &lastRfid, 0);
                snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(restoredLastRfidFromNVS), lastRfidPlayed.c_str());
                loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
            }
        }
    }
#endif


#ifdef MEASURE_BATTERY_VOLTAGE
    // The average of several analog reads will be taken to reduce the noise (Note: One analog read takes ~10µs)
    float measureBatteryVoltage(void) {
        float factor = 1 / ((float) rdiv2/(rdiv2+rdiv1));
        float averagedAnalogValue = 0;
        uint8_t i;
        for (i=0; i<=19; i++) {
            averagedAnalogValue += (float)analogRead(VOLTAGE_READ_PIN);
        }
        averagedAnalogValue /= 20.0;
        return (averagedAnalogValue / maxAnalogValue) * referenceVoltage * factor + offsetVoltage;
    }

    // Measures voltage of a battery as per interval or after bootup (after allowing a few seconds to settle down)
    void batteryVoltageTester(void) {
        if ((millis() - lastVoltageCheckTimestamp >= voltageCheckInterval*60000) || (!lastVoltageCheckTimestamp && millis()>=10000)) {
            float voltage = measureBatteryVoltage();
            #ifdef NEOPIXEL_ENABLE
                if (voltage <= warningLowVoltage) {
                    snprintf(logBuf, serialLoglength, "%s: (%.2f V)", (char *) FPSTR(voltageTooLow), voltage);
                    loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
                    showVoltageWarning = true;
                }
            #endif

            #ifdef MQTT_ENABLE
                char vstr[6];
                snprintf(vstr, 6, "%.2f", voltage);
                publishMqtt((char *) FPSTR(topicBatteryVoltage), vstr, false);
            #endif
            snprintf(logBuf, serialLoglength, "%s: %.2f V", (char *) FPSTR(currentVoltageMsg), voltage);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
            lastVoltageCheckTimestamp = millis();
        }
    }
#endif


// If timer-semaphore is set, read buttons (unless controls are locked)
void buttonHandler() {
    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
        if (lockControls) {
            return;
        }
        unsigned long currentTimestamp = millis();
        buttons[0].currentState = digitalRead(NEXT_BUTTON);
        buttons[1].currentState = digitalRead(PREVIOUS_BUTTON);
        buttons[2].currentState = digitalRead(PAUSEPLAY_BUTTON);
        buttons[3].currentState = digitalRead(DREHENCODER_BUTTON);

        // Iterate over all buttons in struct-array
        for (uint8_t i=0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
            if (buttons[i].currentState != buttons[i].lastState && currentTimestamp - buttons[i].lastPressedTimestamp > buttonDebounceInterval) {
                if (!buttons[i].currentState) {
                    buttons[i].isPressed = true;
                    buttons[i].lastPressedTimestamp = currentTimestamp;
                } else {
                    buttons[i].isReleased = true;
                    buttons[i].lastReleasedTimestamp = currentTimestamp;
                }
            }
            buttons[i].lastState = buttons[i].currentState;
        }
    }
}


// Do corresponding actions for all buttons
void doButtonActions(void) {
    if (lockControls) {
        return; // Avoid button-handling if buttons are locked
    }

    // WiFi-toggle
    if (buttons[0].isPressed && buttons[1].isPressed) {
        if (!wifiStatusToggledTimestamp || (millis() - wifiStatusToggledTimestamp >= 2000)) {
            wifiStatusToggledTimestamp = millis();
            buttons[0].isPressed = false;
            buttons[1].isPressed = false;
            if (writeWifiStatusToNVS(!getWifiEnableStatusFromNVS())) {
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;       // Tell user action was accepted
                #endif
            } else {
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;    // Tell user action failed
                #endif
            }
        }
        return;
    }

    // FTP-enable
    #ifdef FTP_ENABLE
        if (!ftpEnableLastStatus && !ftpEnableCurrentStatus) {
            if (buttons[0].isPressed && buttons[2].isPressed) {
                buttons[0].isPressed = false;
                buttons[2].isPressed = false;
                ftpEnableLastStatus = true;
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            return;
           }
        }
    #endif

    for (uint8_t i=0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        if (buttons[i].isPressed) {
            if (buttons[i].lastReleasedTimestamp > buttons[i].lastPressedTimestamp) {
                if (buttons[i].lastReleasedTimestamp - buttons[i].lastPressedTimestamp >= intervalToLongPress) {
                    switch (i)      // Long-press-actions
                    {
                    case 0:
                        trackControlToQueueSender(LASTTRACK);
                        buttons[i].isPressed = false;
                        break;

                    case 1:
                        trackControlToQueueSender(FIRSTTRACK);
                        buttons[i].isPressed = false;
                        break;

                    case 2:
                        trackControlToQueueSender(PAUSEPLAY);
                        buttons[i].isPressed = false;
                        break;

                    case 3:
                        gotoSleep = true;
                        break;
                    }
                } else {
                    switch (i)      // Short-press-actions
                    {
                    case 0:
                        trackControlToQueueSender(NEXTTRACK);
                        buttons[i].isPressed = false;
                        break;

                    case 1:
                        trackControlToQueueSender(PREVIOUSTRACK);
                        buttons[i].isPressed = false;
                        break;

                    case 2:
                        trackControlToQueueSender(PAUSEPLAY);
                        buttons[i].isPressed = false;
                        break;

                    case 3:
                        buttons[i].isPressed = false;
                        #ifdef MEASURE_BATTERY_VOLTAGE
                            float voltage = measureBatteryVoltage();
                            snprintf(logBuf, serialLoglength, "%s: %.2f V", (char *) FPSTR(currentVoltageMsg), voltage);
                            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
                            #ifdef NEOPIXEL_ENABLE
                                showLedVoltage = true;
                            #endif
                            #ifdef MQTT_ENABLE
                                char vstr[6];
                                snprintf(vstr, 6, "%.2f", voltage);
                                publishMqtt((char *) FPSTR(topicBatteryVoltage), vstr, false);
                            #endif
                        #endif
                    }
                }
            }
        }
    }
}


/* Wrapper-functions for MQTT-publish */
#ifdef MQTT_ENABLE
bool publishMqtt(const char *topic, const char *payload, bool retained) {
  if (strcmp(topic, "") != 0) {
    if (MQTTclient.connected()) {
      MQTTclient.publish(topic, payload, retained);
      delay(100);
      return true;
    }
  }
  return false;
}


bool publishMqtt(const char *topic, int32_t payload, bool retained) {
    char buf[11];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%d", payload);
    return publishMqtt(topic, buf, retained);
}

bool publishMqtt(const char *topic, unsigned long payload, bool retained) {
    char buf[11];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%lu", payload);
    return publishMqtt(topic, buf, retained);
}

bool publishMqtt(const char *topic, uint32_t payload, bool retained) {
    char buf[11];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%u", payload);
    return publishMqtt(topic, buf, retained);
}


/* Cyclic posting via MQTT that ESP is still alive. Use case: when ESPuino is switched off, it will post via
   MQTT it's gonna be offline now. But when unplugging ESPuino e.g. openHAB doesn't know ESPuino is offline.
   One way to recognize this is to determine, when a topic has been updated for the last time. So by
   telling openHAB connection is timed out after 2mins for instance, this is the right topic to check for.  */
void postHeartbeatViaMqtt(void) {
    if (millis() - lastOnlineTimestamp >= stillOnlineInterval*1000) {
        lastOnlineTimestamp = millis();
        if (publishMqtt((char *) FPSTR(topicState), "Online", false)) {
            loggerNl(serialDebug, (char *) FPSTR(stillOnlineMqtt), LOGLEVEL_DEBUG);
        }
    }
}


/* Connects/reconnects to MQTT-Broker unless connection is not already available.
    Manages MQTT-subscriptions.
*/
bool reconnect() {
  uint8_t connect = false;
  uint8_t i = 0;

  if (!mqttLastRetryTimestamp || millis() - mqttLastRetryTimestamp >= mqttRetryInterval * 1000) {
      mqttLastRetryTimestamp = millis();
  } else {
      return false;
  }

  while (!MQTTclient.connected() && i < mqttMaxRetriesPerInterval) {
    i++;
    snprintf(logBuf, serialLoglength, "%s %s", (char *) FPSTR(tryConnectMqttS), mqtt_server);
    loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);

    // Try to connect to MQTT-server. If username AND password are set, they'll be used
    if (strlen(mqttUser) < 1 || strlen(mqttPassword) < 1) {
        loggerNl(serialDebug, (char *) FPSTR(mqttWithoutPwd), LOGLEVEL_NOTICE);
        if (MQTTclient.connect(DEVICE_HOSTNAME)) {
            connect = true;
        }
    } else {
        loggerNl(serialDebug, (char *) FPSTR(mqttWithPwd), LOGLEVEL_NOTICE);
        if (MQTTclient.connect(DEVICE_HOSTNAME, mqttUser, mqttPassword)) {
            connect = true;
        }
    }
    if (connect) {
        loggerNl(serialDebug, (char *) FPSTR(mqttOk), LOGLEVEL_NOTICE);

        // Deepsleep-subscription
        MQTTclient.subscribe((char *) FPSTR(topicSleepCmnd));

        // RFID-Tag-ID-subscription
        MQTTclient.subscribe((char *) FPSTR(topicRfidCmnd));

        // Loudness-subscription
        MQTTclient.subscribe((char *) FPSTR(topicLoudnessCmnd));

        // Sleep-Timer-subscription
        MQTTclient.subscribe((char *) FPSTR(topicSleepTimerCmnd));

        // Next/previous/stop/play-track-subscription
        MQTTclient.subscribe((char *) FPSTR(topicTrackControlCmnd));

        // Lock controls
        MQTTclient.subscribe((char *) FPSTR(topicLockControlsCmnd));

        // Current repeat-Mode
        MQTTclient.subscribe((char *) FPSTR(topicRepeatModeCmnd));

        // LED-brightness
        MQTTclient.subscribe((char *) FPSTR(topicLedBrightnessCmnd));

        // Publish some stuff
        publishMqtt((char *) FPSTR(topicState), "Online", false);
        publishMqtt((char *) FPSTR(topicTrackState), "---", false);
        publishMqtt((char *) FPSTR(topicLoudnessState), currentVolume, false);
        publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimerStartTimestamp, false);
        publishMqtt((char *) FPSTR(topicLockControlsState), "OFF", false);
        publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
        publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
        publishMqtt((char *) FPSTR(topicRepeatModeState), 0, false);

        char currentIPString[16];
        sprintf(currentIPString, "%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
        publishMqtt((char *) FPSTR(topicCurrentIPv4IP), currentIPString, false);

        return MQTTclient.connected();
    } else {
        snprintf(logBuf, serialLoglength, "%s: rc=%i (%d / %d)", (char *) FPSTR(mqttConnFailed), MQTTclient.state(), i, mqttMaxRetriesPerInterval);
        loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
    }
  }
  return false;
}


// Is called if there's a new MQTT-message for us
void callback(const char *topic, const byte *payload, uint32_t length) {
    char *receivedString = strndup((char*)payload, length);
    char *mqttTopic = strdup(topic);

    snprintf(logBuf, serialLoglength, "%s: [Topic: %s] [Command: %s]", (char *) FPSTR(mqttMsgReceived), mqttTopic, receivedString);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);

    // Go to sleep?
    if (strcmp_P(topic, topicSleepCmnd) == 0) {
        if ((strcmp(receivedString, "OFF") == 0) || (strcmp(receivedString, "0") == 0)) {
            gotoSleep = true;
        }
    }

    // New track to play? Take RFID-ID as input
    else if (strcmp_P(topic, topicRfidCmnd) == 0) {
        char *_rfidId = strdup(receivedString);
        xQueueSend(rfidCardQueue, &_rfidId, 0);
        //free(_rfidId);
    }
    // Loudness to change?
    else if (strcmp_P(topic, topicLoudnessCmnd) == 0) {
        unsigned long vol = strtoul(receivedString, NULL, 10);
        volumeToQueueSender(vol);
        encoder.clearCount();
        encoder.setCount(vol * 2);      // Update encoder-value to keep it in-sync with MQTT-updates
    }
    // Modify sleep-timer?
    else if (strcmp_P(topic, topicSleepTimerCmnd) == 0) {
        if (playProperties.playMode == NO_PLAYLIST) {       // Don't allow sleep-modications if no playlist is active
            loggerNl(serialDebug, (char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_INFO);
            publishMqtt((char *) FPSTR(topicSleepState), 0, false);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
            return;
        }
        if (strcmp(receivedString, "EOP") == 0) {
            playProperties.sleepAfterPlaylist = true;
            loggerNl(serialDebug, (char *) FPSTR(sleepTimerEOP), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            return;
        } else if (strcmp(receivedString, "EOT") == 0) {
            playProperties.sleepAfterCurrentTrack = true;
            loggerNl(serialDebug, (char *) FPSTR(sleepTimerEOT), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            return;
        }  else if (strcmp(receivedString, "EO5T") == 0) {
            if ((playProperties.numberOfTracks - 1) >= (playProperties.currentTrackNumber + 5)) {
                playProperties.playUntilTrackNumber = playProperties.currentTrackNumber + 5;
            } else {
                playProperties.sleepAfterPlaylist = true;
            }
            loggerNl(serialDebug, (char *) FPSTR(sleepTimerEO5), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            return;
        } else if (strcmp(receivedString, "0") == 0) {
            if (sleepTimerStartTimestamp) {
                sleepTimerStartTimestamp = 0;
                loggerNl(serialDebug, (char *) FPSTR(sleepTimerStop), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
                publishMqtt((char *) FPSTR(topicSleepState), 0, false);
                return;
            } else {
                loggerNl(serialDebug, (char *) FPSTR(sleepTimerAlreadyStopped), LOGLEVEL_INFO);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                return;
            }
        }
        sleepTimer = strtoul(receivedString, NULL, 10);
        snprintf(logBuf, serialLoglength, "%s: %u Minute(n)", (char *) FPSTR(sleepTimerSetTo), sleepTimer);
        loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
        #ifdef NEOPIXEL_ENABLE
            showLedOk = true;
        #endif

        sleepTimerStartTimestamp = millis();    // Activate timer
        playProperties.sleepAfterPlaylist = false;
        playProperties.sleepAfterCurrentTrack = false;
    }
    // Track-control (pause/play, stop, first, last, next, previous)
    else if (strcmp_P(topic, topicTrackControlCmnd) == 0) {
        uint8_t controlCommand = strtoul(receivedString, NULL, 10);
        trackControlToQueueSender(controlCommand);
    }

    // Check if controls should be locked
    else if (strcmp_P(topic, topicLockControlsCmnd) == 0) {
        if (strcmp(receivedString, "OFF") == 0) {
            lockControls = false;
            loggerNl(serialDebug, (char *) FPSTR(allowButtons), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif

        } else if (strcmp(receivedString, "ON") == 0) {
            lockControls = true;
            loggerNl(serialDebug, (char *) FPSTR(lockButtons), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
        }
    }

    // Check if playmode should be adjusted
    else if (strcmp_P(topic, topicRepeatModeCmnd) == 0) {
        char rBuf[2];
        uint8_t repeatMode = strtoul(receivedString, NULL, 10);
        Serial.printf("Repeat: %d" , repeatMode);
        if (playProperties.playMode != NO_PLAYLIST) {
            if (playProperties.playMode == NO_PLAYLIST) {
                snprintf(rBuf, 2, "%u", getRepeatMode());
				publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                loggerNl(serialDebug, (char *) FPSTR(noPlaylistNotAllowedMqtt), LOGLEVEL_ERROR);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
            } else {
                switch (repeatMode) {
                    case NO_REPEAT:
                        playProperties.repeatCurrentTrack = false;
                        playProperties.repeatPlaylist = false;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl(serialDebug, (char *) FPSTR(modeRepeatNone), LOGLEVEL_INFO);
                        #ifdef NEOPIXEL_ENABLE
                            showLedOk = true;
                        #endif
                        break;

                    case TRACK:
                        playProperties.repeatCurrentTrack = true;
                        playProperties.repeatPlaylist = false;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl(serialDebug, (char *) FPSTR(modeRepeatTrack), LOGLEVEL_INFO);
                        #ifdef NEOPIXEL_ENABLE
                            showLedOk = true;
                        #endif
                        break;

                    case PLAYLIST:
                        playProperties.repeatCurrentTrack = false;
                        playProperties.repeatPlaylist = true;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl(serialDebug, (char *) FPSTR(modeRepeatPlaylist), LOGLEVEL_INFO);
                        #ifdef NEOPIXEL_ENABLE
                            showLedOk = true;
                        #endif
                        break;

                    case TRACK_N_PLAYLIST:
                        playProperties.repeatCurrentTrack = true;
                        playProperties.repeatPlaylist = true;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl(serialDebug, (char *) FPSTR(modeRepeatTracknPlaylist), LOGLEVEL_INFO);
                        #ifdef NEOPIXEL_ENABLE
                            showLedOk = true;
                        #endif
                        break;

                    default:
                        #ifdef NEOPIXEL_ENABLE
                            showLedError = true;
                        #endif
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        break;
                }
            }
        }
    }

    // Check if LEDs should be dimmed
    else if (strcmp_P(topic, topicLedBrightnessCmnd) == 0) {
        ledBrightness = strtoul(receivedString, NULL, 10);
    }

    // Requested something that isn't specified?
    else {
        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(noValidTopic), topic);
        loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
        #ifdef NEOPIXEL_ENABLE
            showLedError = true;
        #endif
    }

    free(receivedString);
    free(mqttTopic);
}
#endif


// Returns current repeat-mode (mix of repeat current track and current playlist)
uint8_t getRepeatMode(void) {
    if (playProperties.repeatPlaylist && playProperties.repeatCurrentTrack) {
        return TRACK_N_PLAYLIST;
    } else if (playProperties.repeatPlaylist && !playProperties.repeatCurrentTrack) {
        return PLAYLIST;
    } else if (!playProperties.repeatPlaylist && playProperties.repeatCurrentTrack) {
        return TRACK;
    } else {
        return NO_REPEAT;
    }
}


// Checks if string starts with prefix
// Returns true if so
bool startsWith(const char *str, const char *pre) {
    if (strlen(pre) < 1) {
      return false;
    }

    return !strncmp(str, pre, strlen(pre));
}


// Checks if string ends with suffix
// Returns true if so
bool endsWith (const char *str, const char *suf) {
    const char *a = str + strlen(str);
    const char *b = suf + strlen(suf);

    while (a != str && b != suf) {
        if (*--a != *--b) break;
    }

    return b == suf && *a == *b;
}


// Release previously allocated memory
void freeMultiCharArray(char **arr, const uint32_t cnt) {
    for (uint32_t i=0; i<=cnt; i++) {
        /*snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(freePtr), *(arr+i));
        loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);*/
        free(*(arr+i));
    }
  *arr = NULL;
}


// Knuth-Fisher-Yates-algorithm to randomize playlist
void randomizePlaylist (char *str[], const uint32_t count) {
    if (count < 1) {
        return;
    }

    uint32_t i, r;
    char *swap = NULL;
    uint32_t max = count-1;

    for (i=0; i<count; i++) {
        if (max > 0) {
            r = rand() % max;
        } else {
            r = 0;
        }
        swap = *(str+max);
        *(str+max) = *(str+r);
        *(str+r) = swap;
        max--;
    }
}


// Helper to sort playlist alphabetically
static int arrSortHelper(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// Sort playlist alphabetically
void sortPlaylist(const char** arr, int n) {
    qsort(arr, n, sizeof(const char*), arrSortHelper);
}


// Check if file-type is correct
bool fileValid(const char *_fileItem) {
    const char ch = '/';
    char *subst;
    subst = strrchr(_fileItem, ch);     // Don't use files that start with .

    return (!startsWith(subst, (char *) "/.")) &&
        (endsWith(_fileItem, ".mp3") || endsWith(_fileItem, ".MP3") ||
         endsWith(_fileItem, ".aac") || endsWith(_fileItem, ".AAC") ||
         endsWith(_fileItem, ".m3u") || endsWith(_fileItem, ".M3U") ||
         endsWith(_fileItem, ".asx") || endsWith(_fileItem, ".ASX"));
}


// Adds webstream to playlist; same like returnPlaylistFromSD() but always only one entry
char ** returnPlaylistFromWebstream(const char *_webUrl) {
    char *webUrl = strdup(_webUrl);
    static char **url;

    if (url != NULL) {
        --url;
        freeMultiCharArray(url, strtoul(*url, NULL, 10));
    }

    url = (char **) malloc(sizeof(char *) * 2);
    url[0] = strdup("1");         // Number of files is always 1 in url-mode
    url[1] = strdup(webUrl);

    free(webUrl);
    return ++url;
}


/* Puts SD-file(s) or directory into a playlist
    First element of array always contains the number of payload-items. */
char ** returnPlaylistFromSD(File _fileOrDirectory) {
    static char **files;
    char fileNameBuf[255];

    snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(freeMemory), ESP.getFreeHeap());
    loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);

    if (files != NULL) {        // If **ptr already exists, de-allocate its memory
        loggerNl(serialDebug, (char *) FPSTR(releaseMemoryOfOldPlaylist), LOGLEVEL_DEBUG);
        --files;
        freeMultiCharArray(files, strtoul(*files, NULL, 10));
        snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(freeMemoryAfterFree), ESP.getFreeHeap());
        loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);
    }

    if (!_fileOrDirectory) {
        loggerNl(serialDebug, (char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
        return NULL;
    }

    // File-mode
    if (!_fileOrDirectory.isDirectory()) {
        files = (char **) malloc(sizeof(char *) * 2);        // +1 because [0] is used for number of elements; [1] -> [n] is used for payload
        if (files == NULL) {
            loggerNl(serialDebug, (char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
            return NULL;
        }
        loggerNl(serialDebug, (char *) FPSTR(fileModeDetected), LOGLEVEL_INFO);
        strncpy(fileNameBuf, (char *) _fileOrDirectory.name(), sizeof(fileNameBuf) / sizeof(fileNameBuf[0]));
        if (fileValid(fileNameBuf)) {
            files = (char **) malloc(sizeof(char *) * 2);
            files[1] = strdup(fileNameBuf);
        }
        files[0] = strdup("1");         // Number of files is always 1 in file-mode

        return ++files;
    }

    // Directory-mode
    uint16_t allocCount = 1;
    uint16_t allocSize = 512;
    char *serializedPlaylist = (char*) calloc(allocSize, sizeof(char));

    while (true) {
       File fileItem = _fileOrDirectory.openNextFile();
        if (!fileItem) {
            break;
        }
        if (fileItem.isDirectory()) {
            continue;
        } else {
            strncpy(fileNameBuf, (char *) fileItem.name(), sizeof(fileNameBuf) / sizeof(fileNameBuf[0]));

            // Don't support filenames that start with "." and only allow .mp3
            if (fileValid(fileNameBuf)) {
                /*snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(nameOfFileFound), fileNameBuf);
                loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);*/
                if ((strlen(serializedPlaylist) + strlen(fileNameBuf) + 2) >= allocCount * allocSize) {
                    serializedPlaylist = (char*) realloc(serializedPlaylist, ++allocCount * allocSize);
                    loggerNl(serialDebug, (char *) FPSTR(reallocCalled), LOGLEVEL_DEBUG);
                    if (serializedPlaylist == NULL) {
                        loggerNl(serialDebug, (char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
                        #ifdef NEOPIXEL_ENABLE
                            showLedError = true;
                        #endif
                        return files;
                    }
                }
                strcat(serializedPlaylist, stringDelimiter);
                strcat(serializedPlaylist, fileNameBuf);
            }
        }
    }

    // Get number of elements out of serialized playlist
    uint32_t cnt = 0;
    for (uint32_t k=0; k<(strlen(serializedPlaylist)); k++) {
        if (serializedPlaylist[k] == '#') {
            cnt++;
        }
    }

    // Alloc only necessary number of playlist-pointers
    files = (char **) malloc(sizeof(char *) * cnt + 1);
    if (files == NULL) {
        loggerNl(serialDebug, (char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
        #ifdef NEOPIXEL_ENABLE
            showLedError = true;
        #endif
        free(serializedPlaylist);
        return NULL;
    }

    // Extract elements out of serialized playlist and copy to playlist
    char *token;
    token = strtok(serializedPlaylist, stringDelimiter);
    uint32_t pos = 1;
    while (token != NULL) {
        files[pos++] = strdup(token);
        token = strtok(NULL, stringDelimiter);
    }

    free(serializedPlaylist);

    files[0] = (char *) malloc(sizeof(char) * 5);
    if (files[0] == NULL) {
        loggerNl(serialDebug, (char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
        #ifdef NEOPIXEL_ENABLE
            showLedError = true;
        #endif
        return NULL;
    }
    sprintf(files[0], "%u", cnt);
    snprintf(logBuf, serialLoglength, "%s: %d", (char *) FPSTR(numberOfValidFiles), cnt);
    loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);

    return ++files;         // return ptr+1 (starting at 1st payload-item); ptr+0 contains number of items
}


/* Wraps putString for writing settings into NVS for RFID-cards.
   Returns number of characters written. */
size_t nvsRfidWriteWrapper (const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks) {
    #ifdef NEOPIXEL_ENABLE
        pauseNeopixel = true;   // Workaround to prevent exceptions due to Neopixel-signalisation while NVS-write
    #endif
    char prefBuf[275];
    char trackBuf[255];
    snprintf(trackBuf, sizeof(trackBuf) / sizeof(trackBuf[0]), _track);

    // If it's a directory we just want to play/save basename(path)
    if (_numberOfTracks > 1) {
        const char s = '/';
        char *last = strrchr(_track, s);
        char *first = strchr(_track, s);
        unsigned long substr = last-first+1;
        if (substr <= sizeof(trackBuf) / sizeof(trackBuf[0])) {
            snprintf(trackBuf, substr, _track);     // save substring basename(_track)
        } else {
            return 0;   // Filename too long!
        }
    }

    snprintf(prefBuf, sizeof(prefBuf) / sizeof(prefBuf[0]), "%s%s%s%u%s%d%s%u", stringDelimiter, trackBuf, stringDelimiter, _playPosition, stringDelimiter, _playMode, stringDelimiter, _trackLastPlayed);
    #if (LANGUAGE == 1)
        snprintf(logBuf, serialLoglength, "Schreibe '%s' in NVS für RFID-Card-ID %s mit playmode %d und letzter Track %u\n", prefBuf, _rfidCardId, _playMode, _trackLastPlayed);
    #else
        snprintf(logBuf, serialLoglength, "Write '%s' to NVS for RFID-Card-ID %s with playmode %d and last track %u\n", prefBuf, _rfidCardId, _playMode, _trackLastPlayed);
    #endif
    logger(serialDebug, logBuf, LOGLEVEL_INFO);
    loggerNl(serialDebug, prefBuf, LOGLEVEL_INFO);
    #ifdef NEOPIXEL_ENABLE
        pauseNeopixel = false;
    #endif
    return prefsRfid.putString(_rfidCardId, prefBuf);
}


// Function to play music as task
void playAudio(void *parameter) {
    static Audio audio;
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(initVolume);


    uint8_t currentVolume;
    static BaseType_t trackQStatus;
    static uint8_t trackCommand = 0;
    bool audioReturnCode;

    for (;;) {
        if (xQueueReceive(volumeQueue, &currentVolume, 0) == pdPASS ) {
            snprintf(logBuf, serialLoglength, "%s: %d", (char *) FPSTR(newLoudnessReceivedQueue), currentVolume);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
            audio.setVolume(currentVolume);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicLoudnessState), currentVolume, false);
            #endif
        }

        if (xQueueReceive(trackControlQueue, &trackCommand, 0) == pdPASS) {
            snprintf(logBuf, serialLoglength, "%s: %d", (char *) FPSTR(newCntrlReceivedQueue), trackCommand);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        }

        trackQStatus = xQueueReceive(trackQueue, &playProperties.playlist, 0);
        if (trackQStatus == pdPASS || playProperties.trackFinished || trackCommand != 0) {
            if (trackQStatus == pdPASS) {
                if (playProperties.pausePlay) {
                    playProperties.pausePlay = !playProperties.pausePlay;
                }
                audio.stopSong();
                #if (LANGUAGE == 1)
                    snprintf(logBuf, serialLoglength, "%s mit %d Titel(n)", (char *) FPSTR(newPlaylistReceived), playProperties.numberOfTracks);
                #else
                    snprintf(logBuf, serialLoglength, "%s with %d track(s)", (char *) FPSTR(newPlaylistReceived), playProperties.numberOfTracks);
                #endif
                loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
                Serial.print(F("Free heap: "));
                Serial.println(ESP.getFreeHeap());

                // If we're in audiobook-mode and apply a modification-card, we don't
                // want to save lastPlayPosition for the mod-card but for the card that holds the playlist
                if(currentRfidTagId != NULL){
                    strncpy(playProperties.playRfidTag, currentRfidTagId, sizeof(playProperties.playRfidTag) / sizeof(playProperties.playRfidTag[0]));
                }

            }
            if (playProperties.trackFinished) {
                playProperties.trackFinished = false;
                if (playProperties.playMode == NO_PLAYLIST) {
                    playProperties.playlistFinished = true;
                    continue;
                }
                if (playProperties.saveLastPlayPosition) {     // Don't save for AUDIOBOOK_LOOP because not necessary
                    if (playProperties.currentTrackNumber + 1 < playProperties.numberOfTracks) {
                        // Only save if there's another track, otherwise it will be saved at end of playlist anyway
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber+1, playProperties.numberOfTracks);
                    }
                }
                if (playProperties.sleepAfterCurrentTrack) {  // Go to sleep if "sleep after track" was requested
                    gotoSleep = true;
                    break;
                }
                if (!playProperties.repeatCurrentTrack) {   // If endless-loop requested, track-number will not be incremented
                    playProperties.currentTrackNumber++;
                } else {
                    loggerNl(serialDebug, (char *) FPSTR(repeatTrackDueToPlaymode), LOGLEVEL_INFO);
                    #ifdef NEOPIXEL_ENABLE
                        showRewind = true;
                    #endif
                }
            }

            if (playProperties.playlistFinished && trackCommand != 0) {
                loggerNl(serialDebug, (char *) FPSTR(noPlaymodeChangeIfIdle), LOGLEVEL_NOTICE);
                trackCommand = 0;
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                continue;
            }
            /* Check if track-control was called
               (stop, start, next track, prev. track, last track, first track...) */
            switch (trackCommand) {
                case STOP:
                    audio.stopSong();
                    trackCommand = 0;
                    loggerNl(serialDebug, (char *) FPSTR(cmndStop), LOGLEVEL_INFO);
                    playProperties.pausePlay = true;
                    playProperties.playlistFinished = true;
                    playProperties.playMode = NO_PLAYLIST;
                    continue;

                case PAUSEPLAY:
                    audio.pauseResume();
                    trackCommand = 0;
                    loggerNl(serialDebug, (char *) FPSTR(cmndPause), LOGLEVEL_INFO);
                    if (playProperties.saveLastPlayPosition && !playProperties.pausePlay) {
                        snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(trackPausedAtPos), audio.getFilePos());
                        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), audio.getFilePos(), playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                    }
                    playProperties.pausePlay = !playProperties.pausePlay;
                    continue;

                case NEXTTRACK:
                    if (playProperties.pausePlay) {
                        audio.pauseResume();
                        playProperties.pausePlay = !playProperties.pausePlay;
                    }
                    if (playProperties.repeatCurrentTrack) {     // End loop if button was pressed
                        playProperties.repeatCurrentTrack = !playProperties.repeatCurrentTrack;
                        char rBuf[2];
						snprintf(rBuf, 2, "%u", getRepeatMode());
						#ifdef MQTT_ENABLE
                            publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        #endif
                    }
                    if (playProperties.currentTrackNumber+1 < playProperties.numberOfTracks) {
                        playProperties.currentTrackNumber++;
                        if (playProperties.saveLastPlayPosition) {
                            nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                            loggerNl(serialDebug, (char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }
                        loggerNl(serialDebug, (char *) FPSTR(cmndNextTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        loggerNl(serialDebug, (char *) FPSTR(lastTrackAlreadyActive), LOGLEVEL_NOTICE);
                        trackCommand = 0;
                        #ifdef NEOPIXEL_ENABLE
                            showLedError = true;
                        #endif
                        continue;
                    }
                    trackCommand = 0;
                    break;

                case PREVIOUSTRACK:
                    if (playProperties.pausePlay) {
                        audio.pauseResume();
                        playProperties.pausePlay = !playProperties.pausePlay;
                    }
                    if (playProperties.repeatCurrentTrack) {     // End loop if button was pressed
                        playProperties.repeatCurrentTrack = !playProperties.repeatCurrentTrack;
                        char rBuf[2];
						snprintf(rBuf, 2, "%u", getRepeatMode());
						#ifdef MQTT_ENABLE
                            publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        #endif
                    }
                    if (playProperties.currentTrackNumber > 0) {
                        // play previous track when current track time is small, else play current track again
                        if(audio.getAudioCurrentTime() < 2) {
                            playProperties.currentTrackNumber--;
                        }
                        if (playProperties.saveLastPlayPosition) {
                            nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                            loggerNl(serialDebug, (char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }

                        loggerNl(serialDebug, (char *) FPSTR(cmndPrevTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        if (playProperties.playMode == WEBSTREAM) {
                            loggerNl(serialDebug, (char *) FPSTR(trackChangeWebstream), LOGLEVEL_INFO);
                            #ifdef NEOPIXEL_ENABLE
                                showLedError = true;
                            #endif
                            trackCommand = 0;
                            continue;
                        }
                        if (playProperties.saveLastPlayPosition) {
                            nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                        }
                            audio.stopSong();
                            #ifdef NEOPIXEL_ENABLE
                                showRewind = true;
                            #endif
                            audioReturnCode = audio.connecttoFS(FSystem, *(playProperties.playlist + playProperties.currentTrackNumber));
                            // consider track as finished, when audio lib call was not successful
                            if (!audioReturnCode) {
                                #ifdef NEOPIXEL_ENABLE
                                    showLedError = true;
                                #endif
                                playProperties.trackFinished = true;
                                continue;
                            }
                            loggerNl(serialDebug, (char *) FPSTR(trackStart), LOGLEVEL_INFO);
                            trackCommand = 0;
                            continue;
                    }
                    trackCommand = 0;
                    break;

                case FIRSTTRACK:
                    if (playProperties.pausePlay) {
                        audio.pauseResume();
                        playProperties.pausePlay = !playProperties.pausePlay;
                    }
                    if (playProperties.currentTrackNumber > 0) {
                        playProperties.currentTrackNumber = 0;
                        if (playProperties.saveLastPlayPosition) {
                            nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                            loggerNl(serialDebug, (char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }
                        loggerNl(serialDebug, (char *) FPSTR(cmndFirstTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        loggerNl(serialDebug, (char *) FPSTR(firstTrackAlreadyActive), LOGLEVEL_NOTICE);
                        #ifdef NEOPIXEL_ENABLE
                            showLedError = true;
                        #endif
                        trackCommand = 0;
                        continue;
                    }
                    trackCommand = 0;
                    break;

                case LASTTRACK:
                    if (playProperties.pausePlay) {
                        audio.pauseResume();
                        playProperties.pausePlay = !playProperties.pausePlay;
                    }
                    if (playProperties.currentTrackNumber+1 < playProperties.numberOfTracks) {
                        playProperties.currentTrackNumber = playProperties.numberOfTracks-1;
                        if (playProperties.saveLastPlayPosition) {
                            nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                            loggerNl(serialDebug, (char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }
                        loggerNl(serialDebug, (char *) FPSTR(cmndLastTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        loggerNl(serialDebug, (char *) FPSTR(lastTrackAlreadyActive), LOGLEVEL_NOTICE);
                        #ifdef NEOPIXEL_ENABLE
                            showLedError = true;
                        #endif
                        trackCommand = 0;
                        continue;
                    }
                    trackCommand = 0;
                    break;

                case 0:
                    break;

                default:
                    trackCommand = 0;
                    loggerNl(serialDebug, (char *) FPSTR(cmndDoesNotExist), LOGLEVEL_NOTICE);
                    #ifdef NEOPIXEL_ENABLE
                        showLedError = true;
                    #endif
                    continue;
            }

            if (playProperties.playUntilTrackNumber == playProperties.currentTrackNumber && playProperties.playUntilTrackNumber > 0) {
                if (playProperties.saveLastPlayPosition) {
                    nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, 0, playProperties.numberOfTracks);
                }
                playProperties.playlistFinished = true;
                playProperties.playMode = NO_PLAYLIST;
                gotoSleep = true;
                continue;
            }

            if (playProperties.currentTrackNumber >= playProperties.numberOfTracks) {         // Check if last element of playlist is already reached
                loggerNl(serialDebug, (char *) FPSTR(endOfPlaylistReached), LOGLEVEL_NOTICE);
                if (!playProperties.repeatPlaylist) {
                    if (playProperties.saveLastPlayPosition) {
                        // Set back to first track
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + 0), 0, playProperties.playMode, 0, playProperties.numberOfTracks);
                    }
                    #ifdef MQTT_ENABLE
                        #if (LANGUAGE == 1)
                            publishMqtt((char *) FPSTR(topicTrackState), "<Ende>", false);
                        #else
                            publishMqtt((char *) FPSTR(topicTrackState), "<End>", false);
                        #endif
                    #endif
                    playProperties.playlistFinished = true;
                    playProperties.playMode = NO_PLAYLIST;
                    #ifdef MQTT_ENABLE
                        publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                    #endif
                    playProperties.currentTrackNumber = 0;
                    playProperties.numberOfTracks = 0;
                    if (playProperties.sleepAfterPlaylist) {
                        gotoSleep = true;
                    }
                    continue;
                } else {    // Check if sleep after current track/playlist was requested
                    if (playProperties.sleepAfterPlaylist || playProperties.sleepAfterCurrentTrack) {
                        playProperties.playlistFinished = true;
                        playProperties.playMode = NO_PLAYLIST;
                        gotoSleep = true;
                        continue;
                    }   // Repeat playlist; set current track number back to 0
                    loggerNl(serialDebug, (char *) FPSTR(repeatPlaylistDueToPlaymode), LOGLEVEL_NOTICE);
                    playProperties.currentTrackNumber = 0;
                    if (playProperties.saveLastPlayPosition) {
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + 0), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                    }
                }
            }

            if (playProperties.playMode == WEBSTREAM) {     // Webstream
                audio.connecttohost(*(playProperties.playlist + playProperties.currentTrackNumber));
                playProperties.playlistFinished = false;
            } else {
                // Files from SD
                if (!FSystem.exists(*(playProperties.playlist + playProperties.currentTrackNumber))) {                        // Check first if file/folder exists
                    snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(dirOrFileDoesNotExist), *(playProperties.playlist + playProperties.currentTrackNumber));
                    loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
                    playProperties.trackFinished = true;
                    continue;
                } else {
                    audioReturnCode = audio.connecttoFS(FSystem, *(playProperties.playlist + playProperties.currentTrackNumber));
                    // consider track as finished, when audio lib call was not successful
                    if(!audioReturnCode) {
                        #ifdef NEOPIXEL_ENABLE
                            showLedError = true;
                        #endif
                        playProperties.trackFinished = true;
                        continue;
                    }
                    #ifdef NEOPIXEL_ENABLE
                        showPlaylistProgress = true;
                    #endif
                    if (playProperties.startAtFilePos > 0) {
                        audio.setFilePos(playProperties.startAtFilePos);
                        snprintf(logBuf, serialLoglength, "%s %u", (char *) FPSTR(trackStartatPos), audio.getFilePos());
                        loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
                    }
                    char buf[255];
                    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "(%d/%d) %s", (playProperties.currentTrackNumber+1), playProperties.numberOfTracks, (const char*) *(playProperties.playlist + playProperties.currentTrackNumber));
                    #ifdef MQTT_ENABLE
                        publishMqtt((char *) FPSTR(topicTrackState), buf, false);
                    #endif
                    #if (LANGUAGE == 1)
                        snprintf(logBuf, serialLoglength, "'%s' wird abgespielt (%d von %d)", *(playProperties.playlist + playProperties.currentTrackNumber), (playProperties.currentTrackNumber+1) , playProperties.numberOfTracks);
                    #else
                        snprintf(logBuf, serialLoglength, "'%s' is being played (%d of %d)", *(playProperties.playlist + playProperties.currentTrackNumber), (playProperties.currentTrackNumber+1) , playProperties.numberOfTracks);
                    #endif
                    loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
                    playProperties.playlistFinished = false;
                }
            }
        }

        // Calculate relative position in file (for neopixel) for SD-card-mode
        #ifdef NEOPIXEL_ENABLE
            if (!playProperties.playlistFinished && playProperties.playMode != WEBSTREAM) {
                double fp = (double) audio.getFilePos() / (double) audio.getFileSize();
                if (millis() % 100 == 0) {
                    playProperties.currentRelPos = fp * 100;
                }
            } else {
                playProperties.currentRelPos = 0;
            }
        #endif

        audio.loop();
        if (playProperties.playlistFinished || playProperties.pausePlay) {
            vTaskDelay(portTICK_PERIOD_MS*10);                   // Waste some time if playlist is not active
        } else {
            lastTimeActiveTimestamp = millis();                  // Refresh if playlist is active so uC will not fall asleep due to reaching inactivity-time
        }

        esp_task_wdt_reset();                                    // Don't forget to feed the dog!
    }
    vTaskDelete(NULL);
}


#if defined RFID_READER_TYPE_MFRC522_SPI || defined RFID_READER_TYPE_MFRC522_I2C
// Instructs RFID-scanner to scan for new RFID-tags
void rfidScanner(void *parameter) {
    byte cardId[cardIdSize];
    char *cardIdString;

    for (;;) {
        esp_task_wdt_reset();
        vTaskDelay(10);
        if ((millis() - lastRfidCheckTimestamp) >= RFID_SCAN_INTERVAL) {
            lastRfidCheckTimestamp = millis();
            // Reset the loop if no new card is present on the sensor/reader. This saves the entire process when idle.

            if (!mfrc522.PICC_IsNewCardPresent()) {
                continue;
            }

            // Select one of the cards
            if (!mfrc522.PICC_ReadCardSerial()) {
                continue;
            }

            //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();

            cardIdString = (char *) malloc(cardIdSize*3 +1);
            if (cardIdString == NULL) {
                logger(serialDebug, (char *) FPSTR(unableToAllocateMem), LOGLEVEL_ERROR);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                continue;
            }

            uint8_t n = 0;
            logger(serialDebug, (char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE);
            for (uint8_t i=0; i<cardIdSize; i++) {
                cardId[i] = mfrc522.uid.uidByte[i];

                snprintf(logBuf, serialLoglength, "%02x", cardId[i]);
                logger(serialDebug, logBuf, LOGLEVEL_NOTICE);

                n += snprintf (&cardIdString[n], sizeof(cardIdString) / sizeof(cardIdString[0]), "%03d", cardId[i]);
                if (i<(cardIdSize-1)) {
                    logger(serialDebug, "-", LOGLEVEL_NOTICE);
                } else {
                    logger(serialDebug, "\n", LOGLEVEL_NOTICE);
                }
            }
            xQueueSend(rfidCardQueue, &cardIdString, 0);
//            free(cardIdString);
        }
    }
    vTaskDelete(NULL);
}
#endif



#ifdef RFID_READER_TYPE_PN5180
// Instructs RFID-scanner to scan for new RFID-tags using PN5180
void rfidScanner(void *parameter) {
    static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
    static PN5180ISO15693 nfc15693(RFID_CS, RFID_BUSY, RFID_RST);
    nfc14443.begin();
    nfc14443.reset();
    // show PN5180 reader version
    uint8_t firmwareVersion[2];
    nfc14443.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
    Serial.print(F("Firmware version="));
    Serial.print(firmwareVersion[1]);
    Serial.print(".");
    Serial.println(firmwareVersion[0]);

    // activate RF field
    delay(4);
    loggerNl(serialDebug, (char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
    byte cardId[cardIdSize], lastCardId[cardIdSize];
    char *cardIdString;

    for (;;) {
        esp_task_wdt_reset();
        if (sleeping)
          break;
        vTaskDelay(10);
        if ((millis() - lastRfidCheckTimestamp) >= RFID_SCAN_INTERVAL) {
            // Reset the loop if no new card is present on the sensor/reader. This saves the entire process when idle.
            lastRfidCheckTimestamp = millis();
            // 1. check for an ISO-14443 card
            nfc14443.reset();
            nfc14443.setupRF();
            uint8_t uid[10];
            if (nfc14443.isCardPresent() && nfc14443.readCardSerial(uid)) {
                cardIdString = (char *) malloc(cardIdSize*3 +1);
                if (cardIdString == NULL) {
                    logger(serialDebug, (char *) FPSTR(unableToAllocateMem), LOGLEVEL_ERROR);
                    #ifdef NEOPIXEL_ENABLE
                        showLedError = true;
                    #endif
                    continue;
                }
                for (uint8_t i=0; i<cardIdSize; i++)
                    cardId[i] = uid[i];
                // check for different card id
                if ( memcmp( (const void *)cardId, (const void *)lastCardId, sizeof(cardId)) == 0)
                    continue;
                memcpy(lastCardId, cardId, sizeof(cardId));
                uint8_t n = 0;
                logger(serialDebug, (char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE);
                for (uint8_t i=0; i<cardIdSize; i++) {
                    snprintf(logBuf, serialLoglength, "%02x", cardId[i]);
                    logger(serialDebug, logBuf, LOGLEVEL_NOTICE);

                    n += snprintf (&cardIdString[n], sizeof(cardIdString) / sizeof(cardIdString[0]), "%03d", cardId[i]);
                    if (i<(cardIdSize-1)) {
                        logger(serialDebug, "-", LOGLEVEL_NOTICE);
                    } else {
                        logger(serialDebug, "\n", LOGLEVEL_NOTICE);
                    }
                }
                xQueueSend(rfidCardQueue, &cardIdString, 0);
                continue;
            }
            // 2. check for an ISO-15693 card
            nfc15693.reset();
            nfc15693.setupRF();
            // check for ICODE-SLIX2 password protected tag
            // put your privacy password here, e.g.:
            // https://de.ifixit.com/Antworten/Ansehen/513422/nfc+Chips+f%C3%BCr+tonies+kaufen
            uint8_t password[] = {0x01, 0x02, 0x03, 0x04};
            ISO15693ErrorCode myrc = nfc15693.disablePrivacyMode(password);
            if (ISO15693_EC_OK == myrc) {
                Serial.println(F("disabling privacy-mode successful"));
            }
            // try to read ISO15693 inventory
            ISO15693ErrorCode rc = nfc15693.getInventory(uid);
            if (rc == ISO15693_EC_OK) {
                cardIdString = (char *) malloc(cardIdSize*3 +1);
                if (cardIdString == NULL) {
                    logger(serialDebug, (char *) FPSTR(unableToAllocateMem), LOGLEVEL_ERROR);
                    #ifdef NEOPIXEL_ENABLE
                        showLedError = true;
                    #endif
                    continue;
                }
                for (uint8_t i=0; i<cardIdSize; i++)
                    cardId[i] = uid[i];
                // check for different card id
                if ( memcmp( (const void *)cardId, (const void *)lastCardId, sizeof(cardId)) == 0)
                    continue;
                memcpy(lastCardId, cardId, sizeof(cardId));

                uint8_t n = 0;
                logger(serialDebug, (char *) FPSTR(rfid15693TagDetected), LOGLEVEL_NOTICE);
                for (uint8_t i=0; i<cardIdSize; i++) {
                    snprintf(logBuf, serialLoglength, "%02x", cardId[i]);
                    logger(serialDebug, logBuf, LOGLEVEL_NOTICE);

                    n += snprintf (&cardIdString[n], sizeof(cardIdString) / sizeof(cardIdString[0]), "%03d", cardId[i]);
                    if (i<(cardIdSize-1)) {
                        logger(serialDebug, "-", LOGLEVEL_NOTICE);
                    } else {
                        logger(serialDebug, "\n", LOGLEVEL_NOTICE);
                    }
                }
                xQueueSend(rfidCardQueue, &cardIdString, 0);
            }
        }
    }
    //Serial.println("deleted RFID scanner-task");
    vTaskDelete(NULL);
}
#endif

// This task handles everything for Neopixel-visualisation
#ifdef NEOPIXEL_ENABLE


// Switches Neopixel-addressing from clockwise to counter clockwise (and vice versa)
uint8_t ledAddress(uint8_t number) {
    #ifdef NEOPIXEL_REVERSE_ROTATION
        return NUM_LEDS-1-number;
    #else
        return number;
    #endif
}


void showLed(void *parameter) {
    static uint8_t hlastVolume = currentVolume;
    static uint8_t lastPos = playProperties.currentRelPos;
    static bool lastPlayState = false;
    static bool lastLockState = false;
    static bool ledBusyShown = false;
    static bool notificationShown = false;
    static bool volumeChangeShown = false;
    static bool showEvenError = false;
    static uint8_t ledPosWebstream = 0;
    static uint8_t ledSwitchInterval = 5; // time in secs (webstream-only)
    static uint8_t webstreamColor = 0;
    static unsigned long lastSwitchTimestamp = 0;
    static bool redrawProgress = false;
    static uint8_t lastLedBrightness = ledBrightness;
    static CRGB::HTMLColorCode idleColor;
    static CRGB leds[NUM_LEDS];
    FastLED.addLeds<CHIPSET , LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalSMD5050 );
    FastLED.setBrightness(ledBrightness);

    for (;;) {
        #ifdef NEOPIXEL_ENABLE
            if (pauseNeopixel) { // Workaround to prevent exceptions while NVS-writes take place
                vTaskDelay(portTICK_RATE_MS*10);
                continue;
            }
        #endif
        if (!bootComplete) {                    // Rotates orange unless boot isn't complete
            FastLED.clear();
            for (uint8_t led = 0; led < NUM_LEDS; led++) {
                if (showEvenError) {
                    if (ledAddress(led) % 2 == 0) {
                        if (millis() <= 10000) {
                            leds[ledAddress(led)] = CRGB::Orange;
                        } else {
                            leds[ledAddress(led)] = CRGB::Red;
                        }
                    }
                } else {
                    if (millis() >= 10000) {    // Flashes red after 10s (will remain forever if SD cannot be mounted)
                       leds[ledAddress(led)] = CRGB::Red;
                    } else {
                        if (ledAddress(led) % 2 == 1) {
                            leds[ledAddress(led)] = CRGB::Orange;
                        }
                    }
                }
            }
            FastLED.show();
            showEvenError = !showEvenError;
            vTaskDelay(portTICK_RATE_MS*500);
            esp_task_wdt_reset();
            continue;
        }

        if (lastLedBrightness != ledBrightness) {
            FastLED.setBrightness(ledBrightness);
            lastLedBrightness = ledBrightness;
        }

        if (!buttons[3].currentState) {
            FastLED.clear();
            for (uint8_t led = 0; led < NUM_LEDS; led++) {
                leds[ledAddress(led)] = CRGB::Red;
                if (buttons[3].currentState) {
                    FastLED.show();
                    delay(5);
                    deepSleepManager();
                    break;
                }
                FastLED.show();
                vTaskDelay(intervalToLongPress / NUM_LEDS * portTICK_RATE_MS);
            }
        }

        if (showLedError) {             // If error occured (e.g. RFID-modification not accepted)
            showLedError = false;
            notificationShown = true;
            FastLED.clear();

            for (uint8_t led = 0; led < NUM_LEDS; led++) {
                leds[ledAddress(led)] = CRGB::Red;
            }
            FastLED.show();
            vTaskDelay(portTICK_RATE_MS * 200);
        }

        if (showLedOk) {             // If action was accepted
            showLedOk = false;
            notificationShown = true;
            FastLED.clear();

            for (uint8_t led = 0; led < NUM_LEDS; led++) {
                leds[ledAddress(led)] = CRGB::Green;
            }
            FastLED.show();
            vTaskDelay(portTICK_RATE_MS * 400);
        }

        #ifdef MEASURE_BATTERY_VOLTAGE
            if (showVoltageWarning) {           // Flashes red three times if battery-voltage is low
                showVoltageWarning = false;
                notificationShown = true;
                for (uint8_t i=0; i<3; i++) {
                    FastLED.clear();

                    for (uint8_t led = 0; led < NUM_LEDS; led++) {
                        leds[ledAddress(led)] = CRGB::Red;
                    }
                    FastLED.show();
                    vTaskDelay(portTICK_RATE_MS * 200);
                    FastLED.clear();

                    for (uint8_t led = 0; led < NUM_LEDS; led++) {
                        leds[ledAddress(led)] = CRGB::Black;
                    }
                    FastLED.show();
                    vTaskDelay(portTICK_RATE_MS * 200);
                }
            }

            if (showLedVoltage) {
                showLedVoltage = false;
                float currentVoltage = measureBatteryVoltage();
                float vDiffIndicatorRange = voltageIndicatorHigh-voltageIndicatorLow;
                float vDiffCurrent = currentVoltage-voltageIndicatorLow;

                if (vDiffCurrent < 0) {     // If voltage is too low or no battery is connected
                    showLedError = true;
                    break;
                } else {
                    uint8_t numLedsToLight = ((float) vDiffCurrent/vDiffIndicatorRange) * NUM_LEDS;
                    FastLED.clear();
                    for (uint8_t led = 0; led < numLedsToLight; led++) {
                        if (((float) numLedsToLight / NUM_LEDS) >= 0.6) {
                            leds[ledAddress(led)] = CRGB::Green;
                        } else if (((float) numLedsToLight / NUM_LEDS) <= 0.6 && ((float) numLedsToLight / NUM_LEDS) >= 0.3) {
                            leds[ledAddress(led)] = CRGB::Orange;
                        } else {
                            leds[ledAddress(led)] = CRGB::Red;
                        }
                    FastLED.show();
                    vTaskDelay(portTICK_RATE_MS*20);
                    }

                    for (uint8_t i=0; i<=100; i++) {
                        if (hlastVolume != currentVolume || showLedError || showLedOk || !buttons[3].currentState) {
                            break;
                        }

                        vTaskDelay(portTICK_RATE_MS*20);
                    }
                }
            }
        #endif

        if (hlastVolume != currentVolume) {         // If volume has been changed
            uint8_t numLedsToLight = map(currentVolume, 0, maxVolume, 0, NUM_LEDS);
            hlastVolume = currentVolume;
            volumeChangeShown = true;
            FastLED.clear();

            for (int led = 0; led < numLedsToLight; led++) {     // (Inverse) color-gradient from green (85) back to (still) red (245) using unsigned-cast
                leds[ledAddress(led)].setHue((uint8_t) (85 - ((double) 95 / NUM_LEDS) * led));
            }
            FastLED.show();

            for (uint8_t i=0; i<=50; i++) {
                if (hlastVolume != currentVolume || showLedError || showLedOk || !buttons[3].currentState) {
                    if (hlastVolume != currentVolume) {
                        volumeChangeShown = false;
                    }
                    break;
                }

                vTaskDelay(portTICK_RATE_MS*20);
            }
        }

        if (showRewind) {
            showRewind = false;
            for (uint8_t i=NUM_LEDS-1; i>0; i--) {
                leds[ledAddress(i)] = CRGB::Black;
                FastLED.show();
                if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || !buttons[3].currentState) {
                    break;
                } else {
                    vTaskDelay(portTICK_RATE_MS*30);
                }
            }
        }

        if (showPlaylistProgress) {
            showPlaylistProgress = false;
            if (playProperties.numberOfTracks > 1 && playProperties.currentTrackNumber < playProperties.numberOfTracks) {
                uint8_t numLedsToLight = map(playProperties.currentTrackNumber, 0, playProperties.numberOfTracks-1, 0, NUM_LEDS);
                FastLED.clear();
                for (uint8_t i=0; i < numLedsToLight; i++) {
                    leds[ledAddress(i)] = CRGB::Blue;
                    FastLED.show();
                    #ifdef MEASURE_BATTERY_VOLTAGE
                        if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || showVoltageWarning || showLedVoltage || !buttons[3].currentState) {
                    #else
                        if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || !buttons[3].currentState) {
                    #endif
                        break;
                    } else {
                        vTaskDelay(portTICK_RATE_MS*30);
                    }
                }

                for (uint8_t i=0; i<=100; i++) {
                    #ifdef MEASURE_BATTERY_VOLTAGE
                        if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || showVoltageWarning || showLedVoltage || !buttons[3].currentState) {
                    #else
                        if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || !buttons[3].currentState) {
                    #endif
                        break;
                    } else {
                        vTaskDelay(portTICK_RATE_MS*15);
                    }
                }

                for (uint8_t i=numLedsToLight; i>0; i--) {
                    leds[ledAddress(i)-1] = CRGB::Black;
                    FastLED.show();
                    #ifdef MEASURE_BATTERY_VOLTAGE
                        if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || showVoltageWarning || showLedVoltage || !buttons[3].currentState) {
                    #else
                        if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || !buttons[3].currentState) {
                    #endif
                        break;
                    } else {
                        vTaskDelay(portTICK_RATE_MS*30);
                    }
                }
            }
        }

        switch (playProperties.playMode) {
            case NO_PLAYLIST:                   // If no playlist is active (idle)
                #ifdef BLUETOOTH_ENABLE
                if(operationMode == OPMODE_BLUETOOTH ) {
                    idleColor = CRGB::Blue;
                } else  {
                #endif
                    if(wifiManager() == WL_CONNECTED) {
                        idleColor = CRGB::White;
                    } else {
                        idleColor = CRGB::Green;
                    }
                #ifdef BLUETOOTH_ENABLE
                }
                #endif
                if (hlastVolume == currentVolume && lastLedBrightness == ledBrightness) {
                    for (uint8_t i=0; i<NUM_LEDS; i++) {
                        FastLED.clear();
                        if (ledAddress(i) == 0) {       // White if Wifi is enabled and blue if not
                            leds[0]            = idleColor;
                            leds[NUM_LEDS/4]   = idleColor;
                            leds[NUM_LEDS/2]   = idleColor;
                            leds[NUM_LEDS/4*3] = idleColor;
                        } else {
                            leds[ledAddress(i) % NUM_LEDS]                = idleColor;
                            leds[(ledAddress(i)+NUM_LEDS/4) % NUM_LEDS]   = idleColor;
                            leds[(ledAddress(i)+NUM_LEDS/2) % NUM_LEDS]   = idleColor;
                            leds[(ledAddress(i)+NUM_LEDS/4*3) % NUM_LEDS] = idleColor;
                        }
                        FastLED.show();
                        for (uint8_t i=0; i<=50; i++) {
                            #ifdef MEASURE_BATTERY_VOLTAGE
                                if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || showVoltageWarning || showLedVoltage || playProperties.playMode != NO_PLAYLIST || !buttons[3].currentState) {
                            #else
                                if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || playProperties.playMode != NO_PLAYLIST || !buttons[3].currentState) {
                            #endif
                                break;
                            } else {
                                vTaskDelay(portTICK_RATE_MS * 10);
                            }
                        }
                    }
                }
                break;

            case BUSY:                          // If uC is busy (parsing SD-card)
                ledBusyShown = true;
                for (uint8_t i=0; i < NUM_LEDS; i++) {
                    FastLED.clear();
                    if (ledAddress(i) == 0) {
                        leds[0] = CRGB::BlueViolet;
                        leds[NUM_LEDS/4] = CRGB::BlueViolet;
                        leds[NUM_LEDS/2] = CRGB::BlueViolet;
                        leds[NUM_LEDS/4*3] = CRGB::BlueViolet;
                    } else {
                        leds[ledAddress(i) % NUM_LEDS] = CRGB::BlueViolet;
                        leds[(ledAddress(i)+NUM_LEDS/4) % NUM_LEDS] = CRGB::BlueViolet;
                        leds[(ledAddress(i)+NUM_LEDS/2) % NUM_LEDS] = CRGB::BlueViolet;
                        leds[(ledAddress(i)+NUM_LEDS/4*3) % NUM_LEDS] = CRGB::BlueViolet;
                    }
                    FastLED.show();
                    if (playProperties.playMode != BUSY) {
                        break;
                    }
                    vTaskDelay(portTICK_RATE_MS * 50);
                }
                break;

            default:                            // If playlist is active (doesn't matter which type)
                if (!playProperties.playlistFinished) {
                    #ifdef MEASURE_BATTERY_VOLTAGE
                        if (playProperties.pausePlay != lastPlayState || lockControls != lastLockState || notificationShown || ledBusyShown || volumeChangeShown || showVoltageWarning || showLedVoltage || !buttons[3].currentState) {
                    #else
                        if (playProperties.pausePlay != lastPlayState || lockControls != lastLockState || notificationShown || ledBusyShown || volumeChangeShown || !buttons[3].currentState) {
                    #endif
                        lastPlayState = playProperties.pausePlay;
                        lastLockState = lockControls;
                        notificationShown = false;
                        volumeChangeShown = false;
                        if (ledBusyShown) {
                            ledBusyShown = false;
                            FastLED.clear();
                            FastLED.show();
                        }
                        redrawProgress = true;
                    }

                    if (playProperties.playMode != WEBSTREAM) {
                        if (playProperties.currentRelPos != lastPos || redrawProgress) {
                            redrawProgress = false;
                            lastPos = playProperties.currentRelPos;
                            uint8_t numLedsToLight = map(playProperties.currentRelPos, 0, 98, 0, NUM_LEDS);
                            FastLED.clear();
                            for (uint8_t led = 0; led < numLedsToLight; led++) {
                                if (lockControls) {
                                    leds[ledAddress(led)] = CRGB::Red;
                                } else if (!playProperties.pausePlay) { // Hue-rainbow
                                    leds[ledAddress(led)].setHue((uint8_t) (85 - ((double) 95 / NUM_LEDS) * led));
                                }
                            }
                            if (playProperties.pausePlay) {
                                leds[ledAddress(0)] = CRGB::Orange;
                                    leds[(ledAddress(NUM_LEDS/4)) % NUM_LEDS] = CRGB::Orange;
                                    leds[(ledAddress(NUM_LEDS/2)) % NUM_LEDS] = CRGB::Orange;
                                    leds[(ledAddress(NUM_LEDS/4*3)) % NUM_LEDS] = CRGB::Orange;
                                    break;
                            }
                        }
                    } else { // ... but do things a little bit different for Webstream as there's no progress available
                        if (lastSwitchTimestamp == 0 || (millis() - lastSwitchTimestamp >= ledSwitchInterval * 1000) || redrawProgress) {
                            redrawProgress = false;
                            lastSwitchTimestamp = millis();
                            FastLED.clear();
                            if (ledPosWebstream + 1 < NUM_LEDS) {
                                ledPosWebstream++;
                            } else {
                                ledPosWebstream = 0;
                            }
                            if (lockControls) {
                                leds[ledAddress(ledPosWebstream)] = CRGB::Red;
                                leds[(ledAddress(ledPosWebstream)+NUM_LEDS/2) % NUM_LEDS] = CRGB::Red;
                            } else if (!playProperties.pausePlay) {
                                leds[ledAddress(ledPosWebstream)].setHue(webstreamColor);
                                leds[(ledAddress(ledPosWebstream)+NUM_LEDS/2) % NUM_LEDS].setHue(webstreamColor++);
                            } else if (playProperties.pausePlay) {
                                leds[ledAddress(ledPosWebstream)] = CRGB::Orange;
                                leds[(ledAddress(ledPosWebstream)+NUM_LEDS/2) % NUM_LEDS] = CRGB::Orange;
                            }
                        }
                    }
                    FastLED.show();
                    vTaskDelay(portTICK_RATE_MS * 5);
                }
        }
        //vTaskDelay(portTICK_RATE_MS * 10);
        esp_task_wdt_reset();
    }
    vTaskDelete(NULL);
}
#endif


// Sets deep-sleep-flag if max. inactivity-time is reached
void sleepHandler(void) {
    unsigned long m = millis();
    if (m >= lastTimeActiveTimestamp && (m - lastTimeActiveTimestamp >= maxInactivityTime * 1000 * 60)) {
        loggerNl(serialDebug, (char *) FPSTR(goToSleepDueToIdle), LOGLEVEL_INFO);
        gotoSleep = true;
    } else if (sleepTimerStartTimestamp > 0) {
        if (m - sleepTimerStartTimestamp >= sleepTimer * 1000 * 60) {
            loggerNl(serialDebug, (char *) FPSTR(goToSleepDueToTimer), LOGLEVEL_INFO);
            gotoSleep = true;
        }
    }
}

#ifdef PN5180_ENABLE_LPCD
// goto low power card detection mode
void gotoLPCD() {
    static PN5180 nfc(RFID_CS, RFID_BUSY, RFID_RST);
    nfc.begin();
    // show PN5180 reader version
    uint8_t firmwareVersion[2];
    nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
    Serial.print(F("Firmware version="));
    Serial.print(firmwareVersion[1]);
    Serial.print(".");
    Serial.println(firmwareVersion[0]);
    // check firmware version: PN5180 firmware < 4.0 has several bugs preventing the LPCD mode
    // you can flash latest firmware with this project: https://github.com/abidxraihan/PN5180_Updater_ESP32
    if (firmwareVersion[1] < 4) {
       Serial.println(F("This PN5180 firmware does not work with LPCD! use firmware >= 4.0"));
       return;
    }
    Serial.println(F("prepare low power card detection..."));
    nfc.prepareLPCD();
    nfc.clearIRQStatus(0xffffffff);
    Serial.print(F("PN5180 IRQ PIN: ")); Serial.println(digitalRead(RFID_IRQ));
    // turn on LPCD
    uint16_t wakeupCounterInMs = 0x3FF; //  must be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
    if (nfc.switchToLPCD(wakeupCounterInMs)) {
        Serial.println(F("switch to low power card detection: success"));
        // configure wakeup pin for deep-sleep wake-up, use ext1
        esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
        // freeze pin states in deep sleep
        gpio_hold_en(gpio_num_t(RFID_CS));  // CS/NSS
        gpio_hold_en(gpio_num_t(RFID_RST)); // RST
        gpio_deep_sleep_hold_en();
   } else {
        Serial.println(F("switchToLPCD failed"));
    }
}
#endif

// Puts uC to deep-sleep if flag is set
void deepSleepManager(void) {
    if (gotoSleep) {
        if (sleeping)
            return;
        sleeping = true;
        loggerNl(serialDebug, (char *) FPSTR(goToSleepNow), LOGLEVEL_NOTICE);
        #ifdef MQTT_ENABLE
            publishMqtt((char *) FPSTR(topicState), "Offline", false);
            publishMqtt((char *) FPSTR(topicTrackState), "---", false);
            MQTTclient.disconnect();
        #endif
        #ifdef NEOPIXEL_ENABLE
            FastLED.clear();
            FastLED.show();
        #endif
        #ifdef USE_LAST_VOLUME_AFTER_REBOOT
            prefsSettings.putUInt("previousVolume", currentVolume);
        #endif
        // SD card goto idle mode
        #ifdef SD_MMC_1BIT_MODE
            SD_MMC.end();
        #else
            /*SPI.end();
            spiSD.end();*/
        #endif
        Serial.flush();
        // switch off power
        digitalWrite(POWER, LOW);
        delay(200);
        #ifdef PN5180_ENABLE_LPCD
            // prepare and go to low power card detection mode
            gotoLPCD();
        #endif
        Serial.println(F("deep-sleep, good night......."));
        esp_deep_sleep_start();
    }
}


// Adds new volume-entry to volume-queue
void volumeToQueueSender(const int32_t _newVolume) {
    uint32_t _volume;
    if (_newVolume <= minVolume) {
        _volume = minVolume;
    } else if (_newVolume > maxVolume) {
        _volume = maxVolume;
    } else {
        _volume = _newVolume;
    }
    xQueueSend(volumeQueue, &_volume, 0);
}


// Adds new control-command to control-queue
void trackControlToQueueSender(const uint8_t trackCommand) {
    xQueueSend(trackControlQueue, &trackCommand, 0);
}


// Handles volume directed by rotary encoder
void volumeHandler(const int32_t _minVolume, const int32_t _maxVolume) {
    if (lockControls) {
        encoder.clearCount();
        encoder.setCount(currentVolume*2);
        return;
    }

    currentEncoderValue = encoder.getCount();
    // Only if initial run or value has changed. And only after "full step" of rotary encoder
    if (((lastEncoderValue != currentEncoderValue) || lastVolume == -1) && (currentEncoderValue % 2 == 0)) {
        lastTimeActiveTimestamp = millis();        // Set inactive back if rotary encoder was used
        if (_maxVolume * 2 < currentEncoderValue) {
            encoder.clearCount();
            encoder.setCount(_maxVolume * 2);
            loggerNl(serialDebug, (char *) FPSTR(maxLoudnessReached), LOGLEVEL_INFO);
            currentEncoderValue = encoder.getCount();
        } else if (currentEncoderValue < _minVolume) {
            encoder.clearCount();
            encoder.setCount(_minVolume);
            loggerNl(serialDebug, (char *) FPSTR(minLoudnessReached), LOGLEVEL_INFO);
            currentEncoderValue = encoder.getCount();
        }
        lastEncoderValue = currentEncoderValue;
        currentVolume = lastEncoderValue / 2;
        if (currentVolume != lastVolume) {
            lastVolume = currentVolume;
            volumeToQueueSender(currentVolume);
        }
    }
}


// Receives de-serialized RFID-data (from NVS) and dispatches playlists for the given
// playmode to the track-queue.
void trackQueueDispatcher(const char *_itemToPlay, const uint32_t _lastPlayPos, const uint32_t _playMode, const uint16_t _trackLastPlayed) {
    char *filename = (char *) malloc(sizeof(char) * 255);
    strncpy(filename, _itemToPlay, 255);
    playProperties.startAtFilePos = _lastPlayPos;
    playProperties.currentTrackNumber = _trackLastPlayed;
    char **musicFiles;
    playProperties.playMode = BUSY;     // Show @Neopixel, if uC is busy with creating playlist

    #ifdef MQTT_ENABLE
        publishMqtt((char *) FPSTR(topicLedBrightnessState), 0, false);
        publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
    #endif
    if (_playMode != WEBSTREAM) {
        musicFiles = returnPlaylistFromSD(FSystem.open(filename));
    } else {
        musicFiles = returnPlaylistFromWebstream(filename);
    }
    #ifdef MQTT_ENABLE
        publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
    #endif

    if (musicFiles == NULL) {
        loggerNl(serialDebug, (char *) FPSTR(errorOccured), LOGLEVEL_ERROR);
        #ifdef NEOPIXEL_ENABLE
            showLedError = true;
        #endif
        playProperties.playMode = NO_PLAYLIST;
        return;
    } else if (!strcmp(*(musicFiles-1), "0")) {
        loggerNl(serialDebug, (char *) FPSTR(noMp3FilesInDir), LOGLEVEL_NOTICE);
        #ifdef NEOPIXEL_ENABLE
            showLedError = true;
        #endif
        playProperties.playMode = NO_PLAYLIST;
        free (filename);
        return;
    }

    playProperties.playMode = _playMode;
    playProperties.numberOfTracks = strtoul(*(musicFiles-1), NULL, 10);
    // Set some default-values
    playProperties.repeatCurrentTrack = false;
    playProperties.repeatPlaylist = false;
    playProperties.sleepAfterCurrentTrack = false;
    playProperties.sleepAfterPlaylist = false;
    playProperties.saveLastPlayPosition = false;
    playProperties.playUntilTrackNumber = 0;

    #ifdef PLAY_LAST_RFID_AFTER_REBOOT
        storeLastRfidPlayed(currentRfidTagId);
    #endif

    switch(playProperties.playMode) {
        case SINGLE_TRACK: {
            loggerNl(serialDebug, (char *) FPSTR(modeSingleTrack), LOGLEVEL_NOTICE);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case SINGLE_TRACK_LOOP: {
            playProperties.repeatCurrentTrack = true;
            loggerNl(serialDebug, (char *) FPSTR(modeSingleTrackLoop), LOGLEVEL_NOTICE);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), TRACK, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case AUDIOBOOK: {   // Tracks need to be alph. sorted!
            playProperties.saveLastPlayPosition = true;
            loggerNl(serialDebug, (char *) FPSTR(modeSingleAudiobook), LOGLEVEL_NOTICE);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            #endif
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case AUDIOBOOK_LOOP: {  // Tracks need to be alph. sorted!
            playProperties.repeatPlaylist = true;
            playProperties.saveLastPlayPosition = true;
            loggerNl(serialDebug, (char *) FPSTR(modeSingleAudiobookLoop), LOGLEVEL_NOTICE);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            #endif
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_SORTED: {
            snprintf(logBuf, serialLoglength, "%s '%s' ", (char *) FPSTR(modeAllTrackAlphSorted), filename);
            loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_RANDOM: {
            loggerNl(serialDebug, (char *) FPSTR(modeAllTrackRandom), LOGLEVEL_NOTICE);
            randomizePlaylist(musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_SORTED_LOOP: {
            playProperties.repeatPlaylist = true;
            loggerNl(serialDebug, (char *) FPSTR(modeAllTrackAlphSortedLoop), LOGLEVEL_NOTICE);
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_RANDOM_LOOP: {
            playProperties.repeatPlaylist = true;
            loggerNl(serialDebug, (char *) FPSTR(modeAllTrackRandomLoop), LOGLEVEL_NOTICE);
            randomizePlaylist(musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case WEBSTREAM: {   // This is always just one "track"
            loggerNl(serialDebug, (char *) FPSTR(modeWebstream), LOGLEVEL_NOTICE);
            if (wifiManager() == WL_CONNECTED) {
                xQueueSend(trackQueue, &(musicFiles), 0);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                    publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
                #endif
            } else {
                loggerNl(serialDebug, (char *) FPSTR(webstreamNotAvailable), LOGLEVEL_ERROR);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                playProperties.playMode = NO_PLAYLIST;
            }
            break;
        }

        default:
            loggerNl(serialDebug, (char *) FPSTR(modeDoesNotExist), LOGLEVEL_ERROR);
            playProperties.playMode = NO_PLAYLIST;
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
    }
    free (filename);
}


// Modification-cards can change some settings (e.g. introducing track-looping or sleep after track/playlist).
// This function handles them.
void doRfidCardModifications(const uint32_t mod) {
    #ifdef PLAY_LAST_RFID_AFTER_REBOOT
	    if (recoverLastRfid) {
            recoverLastRfid = false;
            // We don't want to remember modification-cards
            return;
        }
	#endif

    switch (mod) {
        case LOCK_BUTTONS_MOD:      // Locks/unlocks all buttons
            lockControls = !lockControls;
            if (lockControls) {
                loggerNl(serialDebug, (char *) FPSTR(modificatorAllButtonsLocked), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLockControlsState), "ON", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            } else {
                loggerNl(serialDebug, (char *) FPSTR(modificatorAllButtonsUnlocked), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLockControlsState), "OFF", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            }
            break;

        case SLEEP_TIMER_MOD_15:    // Enables/disables sleep after 15 minutes
            if (sleepTimerStartTimestamp && sleepTimer == 15) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #endif
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 15;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepTimer15), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
                #endif
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            playProperties.playUntilTrackNumber = 0;
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            break;

        case SLEEP_TIMER_MOD_30:    // Enables/disables sleep after 30 minutes
            if (sleepTimerStartTimestamp && sleepTimer == 30) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #endif
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 30;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepTimer30), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
                #endif
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            playProperties.playUntilTrackNumber = 0;
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            break;

        case SLEEP_TIMER_MOD_60:    // Enables/disables sleep after 60 minutes
            if (sleepTimerStartTimestamp && sleepTimer == 60) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 60;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepTimer60), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
                #endif
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            playProperties.playUntilTrackNumber = 0;
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            break;

        case SLEEP_TIMER_MOD_120:    // Enables/disables sleep after 2 hrs
            if (sleepTimerStartTimestamp && sleepTimer == 120) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 120;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepTimer120), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
                #endif
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            playProperties.playUntilTrackNumber = 0;
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            break;

        case SLEEP_AFTER_END_OF_TRACK:  // Puts uC to sleep after end of current track
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl(serialDebug, (char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                return;
            }
            if (playProperties.sleepAfterCurrentTrack) {
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepAtEOTd), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
            } else {
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepAtEOT), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), "EOT", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
            }
            playProperties.sleepAfterCurrentTrack = !playProperties.sleepAfterCurrentTrack;
            playProperties.sleepAfterPlaylist = false;
            sleepTimerStartTimestamp = 0;
            playProperties.playUntilTrackNumber = 0;

            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
            #endif
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            break;

        case SLEEP_AFTER_END_OF_PLAYLIST:   // Puts uC to sleep after end of whole playlist (can take a while :->)
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl(serialDebug, (char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                return;
            }
            if (playProperties.sleepAfterCurrentTrack) {
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepAtEOPd), LOGLEVEL_NOTICE);
            } else {
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl(serialDebug, (char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepAtEOP), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), "EOP", false);
                #endif
            }

            playProperties.sleepAfterCurrentTrack = false;
            playProperties.sleepAfterPlaylist = !playProperties.sleepAfterPlaylist;
            sleepTimerStartTimestamp = 0;
            playProperties.playUntilTrackNumber = 0;
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
            #endif
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            break;

        case SLEEP_AFTER_5_TRACKS:
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl(serialDebug, (char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                return;
            }

            playProperties.sleepAfterCurrentTrack = false;
            playProperties.sleepAfterPlaylist = false;
            sleepTimerStartTimestamp = 0;

            if (playProperties.playUntilTrackNumber > 0) {
                playProperties.playUntilTrackNumber = 0;
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
                loggerNl(serialDebug, (char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
            } else {
                if (playProperties.currentTrackNumber + 5 > playProperties.numberOfTracks) {        // If currentTrack + 5 exceeds number of tracks in playlist, sleep after end of playlist
                    playProperties.sleepAfterPlaylist = true;
                    #ifdef MQTT_ENABLE
                        publishMqtt((char *) FPSTR(topicSleepTimerState), "EOP", false);
                    #endif
                } else {
                    playProperties.playUntilTrackNumber = playProperties.currentTrackNumber + 5;
                    #ifdef MQTT_ENABLE
                        publishMqtt((char *) FPSTR(topicSleepTimerState), "EO5T", false);
                    #endif
                }
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                #endif
                loggerNl(serialDebug, (char *) FPSTR(sleepTimerEO5), LOGLEVEL_NOTICE);
            }

            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
            #endif
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            break;

        case REPEAT_PLAYLIST:
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl(serialDebug, (char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
            } else {
                if (playProperties.repeatPlaylist) {
                    loggerNl(serialDebug, (char *) FPSTR(modificatorPlaylistLoopDeactive), LOGLEVEL_NOTICE);
                } else {
                    loggerNl(serialDebug, (char *) FPSTR(modificatorPlaylistLoopActive), LOGLEVEL_NOTICE);
                }
                playProperties.repeatPlaylist = !playProperties.repeatPlaylist;
                char rBuf[2];
                snprintf(rBuf, 2, "%u", getRepeatMode());
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            }
            break;

        case REPEAT_TRACK:      // Introduces looping for track-mode
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl(serialDebug, (char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
            } else {
                if (playProperties.repeatCurrentTrack) {
                    loggerNl(serialDebug, (char *) FPSTR(modificatorTrackDeactive), LOGLEVEL_NOTICE);
                } else {
                    loggerNl(serialDebug, (char *) FPSTR(modificatorTrackActive), LOGLEVEL_NOTICE);
                }
                playProperties.repeatCurrentTrack = !playProperties.repeatCurrentTrack;
                char rBuf[2];
                snprintf(rBuf, 2, "%u", getRepeatMode());
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            }
            break;

        case DIMM_LEDS_NIGHTMODE:
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
            #endif
            loggerNl(serialDebug, (char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
            #ifdef NEOPIXEL_ENABLE
                ledBrightness = nightLedBrightness;
                showLedOk = true;
            #endif
            break;

        case TOGGLE_WIFI_STATUS:
            if (writeWifiStatusToNVS(!getWifiEnableStatusFromNVS())) {
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            } else {
                 #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
            }

            break;
        #ifdef BLUETOOTH_ENABLE
            case TOGGLE_BLUETOOTH_MODE:
                if (readOperationModeFromNVS() == OPMODE_NORMAL) {
                    #ifdef NEOPIXEL_ENABLE
                        showLedOk = true;
                    #endif
                    setOperationMode(OPMODE_BLUETOOTH);
                } else if (readOperationModeFromNVS() == OPMODE_BLUETOOTH) {
                    #ifdef NEOPIXEL_ENABLE
                        showLedOk = true;
                    #endif
                    setOperationMode(OPMODE_NORMAL);
                } else {
                    #ifdef NEOPIXEL_ENABLE
                        showLedError = true;
                    #endif
                }
                break;
        #endif
        case ENABLE_FTP_SERVER:
            if (!ftpEnableLastStatus && !ftpEnableCurrentStatus) {
                ftpEnableLastStatus = true;
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            } else {
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
            }

        break;
        default:
            snprintf(logBuf, serialLoglength, "%s %d !", (char *) FPSTR(modificatorDoesNotExist), mod);
            loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
    }
}


// Tries to lookup RFID-tag-string in NVS and extracts parameter from it if found
void rfidPreferenceLookupHandler (void) {
    BaseType_t rfidStatus;
    char *rfidTagId;
    char _file[255];
    uint32_t _lastPlayPos = 0;
    uint16_t _trackLastPlayed = 0;
    uint32_t _playMode = 1;

    rfidStatus = xQueueReceive(rfidCardQueue, &rfidTagId, 0);
    if (rfidStatus == pdPASS) {
        lastTimeActiveTimestamp = millis();
        free(currentRfidTagId);
        currentRfidTagId = strdup(rfidTagId);
        free(rfidTagId);
        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(rfidTagReceived), currentRfidTagId);
        sendWebsocketData(0, 10);       // Push new rfidTagId to all websocket-clients
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);

        String s = prefsRfid.getString(currentRfidTagId, "-1");                 // Try to lookup rfidId in NVS
        if (!s.compareTo("-1")) {
            loggerNl(serialDebug, (char *) FPSTR(rfidTagUnknownInNvs), LOGLEVEL_ERROR);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
            return;
        }

        char *token;
        uint8_t i = 1;
        token = strtok((char *) s.c_str(), stringDelimiter);
        while (token != NULL) {                               // Try to extract data from string after lookup
            if (i == 1) {
                strncpy(_file, token, sizeof(_file) / sizeof(_file[0]));
            } else if (i == 2) {
                _lastPlayPos = strtoul(token, NULL, 10);
            } else if (i == 3) {
                _playMode = strtoul(token, NULL, 10);
            }else if (i == 4) {
                _trackLastPlayed = strtoul(token, NULL, 10);
            }
            i++;
            token = strtok(NULL, stringDelimiter);
        }

        if (i != 5) {
            loggerNl(serialDebug, (char *) FPSTR(errorOccuredNvs), LOGLEVEL_ERROR);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
        } else {
        // Only pass file to queue if strtok revealed 3 items
            if (_playMode >= 100) {
                doRfidCardModifications(_playMode);
            } else {
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicRfidState), currentRfidTagId, false);
                #endif

                #ifdef BLUETOOTH_ENABLE
                // if music rfid was read, go back to normal mode
                if(operationMode == OPMODE_BLUETOOTH) {
                    setOperationMode(OPMODE_NORMAL);
                }
                #endif

                trackQueueDispatcher(_file, _lastPlayPos, _playMode, _trackLastPlayed);
            }
        }
    }
}


// Initialize soft access-point
void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, ip, netmask);
    WiFi.softAP(SSID);
    delay(500);

    loggerNl(serialDebug, (char *) FPSTR(apReady), LOGLEVEL_NOTICE);
    snprintf(logBuf, serialLoglength, "IP-Adresse: %d.%d.%d.%d", apIP[0],apIP[1], apIP[2], apIP[3]);
    loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);

    wServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", accesspoint_HTML);
        });

    wServer.on("/init", HTTP_POST, [] (AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("pwd", true) && request->hasParam("hostname", true)) {
            Serial.println(request->getParam("ssid", true)->value());
            Serial.println(request->getParam("pwd", true)->value());
            Serial.println(request->getParam("hostname", true)->value());
            prefsSettings.putString("SSID", request->getParam("ssid", true)->value());
            prefsSettings.putString("Password", request->getParam("pwd", true)->value());
            prefsSettings.putString("Hostname", request->getParam("hostname", true)->value());
        }
        request->send_P(200, "text/html", accesspoint_HTML);
    });

    wServer.on("/restart", HTTP_GET, [] (AsyncWebServerRequest *request) {
        #if (LANGUAGE == 1)
            request->send(200, "text/html", "ESP wird neu gestartet...");
        #else
            request->send(200, "text/html", "ESP is being restarted...");
        #endif
        Serial.flush();
        ESP.restart();
    });

    // allow cors for local debug
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    wServer.begin();
    loggerNl(serialDebug, (char *) FPSTR(httpReady), LOGLEVEL_NOTICE);
    accessPointStarted = true;
}

// Reads stored WiFi-status from NVS
bool getWifiEnableStatusFromNVS(void) {
    uint32_t wifiStatus = prefsSettings.getUInt("enableWifi", 99);

    // if not set so far, preseed with 1 (enable)
    if (wifiStatus == 99) {
        prefsSettings.putUInt("enableWifi", 1);
        wifiStatus = 1;
    }

    return wifiStatus;
}


// Writes to NVS whether WiFi should be activated
bool writeWifiStatusToNVS(bool wifiStatus) {
    if (!wifiStatus) {
        if (prefsSettings.putUInt("enableWifi", 0)) {  // disable
            loggerNl(serialDebug, (char *) FPSTR(wifiDisabledAfterRestart), LOGLEVEL_NOTICE);
            if (playProperties.playMode == WEBSTREAM) {
                trackControlToQueueSender(STOP);
            }
            delay(300);
            WiFi.mode(WIFI_OFF);
            wifiEnabled = false;
            return true;
        }

    } else {
        if (prefsSettings.putUInt("enableWifi", 1)) {  // enable
            loggerNl(serialDebug, (char *) FPSTR(wifiEnabledAfterRestart), LOGLEVEL_NOTICE);
            wifiNeedsRestart = true;
            wifiEnabled = true;
            return true;
        }
    }
    return true;
}

uint8_t readOperationModeFromNVS(void) {
    return prefsSettings.getUChar("operationMode", OPMODE_NORMAL);
}

bool setOperationMode(uint8_t newOperationMode) {
    uint8_t currentOperationMode = prefsSettings.getUChar("operationMode", OPMODE_NORMAL);
    if(currentOperationMode != newOperationMode) {
        if (prefsSettings.putUChar("operationMode", newOperationMode)) {
            ESP.restart();
        }
    }
    return true;
}


// Creates FTP-instance only when requested
#ifdef FTP_ENABLE
    void ftpManager(void) {
        if (ftpEnableLastStatus && !ftpEnableCurrentStatus) {
            snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(freeHeapWithoutFtp), ESP.getFreeHeap());
            loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);
            ftpEnableCurrentStatus = true;
            ftpSrv = new FtpServer();
            ftpSrv->begin(FSystem, ftpUser, ftpPassword);
            snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(freeHeapWithFtp), ESP.getFreeHeap());
            loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);
            #if (LANGUAGE == 1)
                Serial.println(F("FTP-Server gestartet"));
            #else
                Serial.println(F("FTP-server started"));
            #endif
        }
    }
#endif


// Provides management for WiFi
wl_status_t wifiManager(void) {
    // If wifi whould not be activated, return instantly
    if (!wifiEnabled) {
        return WiFi.status();
    }

    if (!wifiCheckLastTimestamp || wifiNeedsRestart) {
        // Get credentials from NVS
        String strSSID = prefsSettings.getString("SSID", "-1");
        if (!strSSID.compareTo("-1")) {
            loggerNl(serialDebug, (char *) FPSTR(ssidNotFoundInNvs), LOGLEVEL_ERROR);
        }
        String strPassword = prefsSettings.getString("Password", "-1");
        if (!strPassword.compareTo("-1")) {
            loggerNl(serialDebug, (char *) FPSTR(wifiPwdNotFoundInNvs), LOGLEVEL_ERROR);
        }
        const char *_ssid = strSSID.c_str();
        const char *_pwd = strPassword.c_str();

        // Get (optional) hostname-configration from NVS
        String hostname = prefsSettings.getString("Hostname", "-1");
        if (hostname.compareTo("-1")) {
            //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
            WiFi.setHostname(hostname.c_str());
            snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(restoredHostnameFromNvs), hostname.c_str());
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {
            loggerNl(serialDebug, (char *) FPSTR(wifiHostnameNotSet), LOGLEVEL_INFO);
        }

        // Add configration of static IP (if requested)
        #ifdef STATIC_IP_ENABLE
            snprintf(logBuf, serialLoglength, "%s", (char *) FPSTR(tryStaticIpConfig));
            loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
            if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
                snprintf(logBuf, serialLoglength, "%s", (char *) FPSTR(staticIPConfigFailed));
                loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
            }
        #endif

        // Try to join local WiFi. If not successful, an access-point is opened
        WiFi.begin(_ssid, _pwd);

        uint8_t tryCount=0;
        while (WiFi.status() != WL_CONNECTED && tryCount <= 4) {
            delay(500);
            Serial.print(F("."));
            tryCount++;
            wifiCheckLastTimestamp = millis();
            if (tryCount >= 4 && WiFi.status() == WL_CONNECT_FAILED) {
                WiFi.begin(_ssid, _pwd);        // ESP32-workaround (otherwise WiFi-connection sometimes fails)
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
            myIP = WiFi.localIP();
            #if (LANGUAGE == 1)
                snprintf(logBuf, serialLoglength, "Aktuelle IP: %d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
            #else
                snprintf(logBuf, serialLoglength, "Current IP: %d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
            #endif
            loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
        } else { // Starts AP if WiFi-connect wasn't successful
            accessPointStart((char *) FPSTR(accessPointNetworkSSID), apIP, apNetmask);
        }

        #ifdef MDNS_ENABLE
        // zero conf, make device available as <hostname>.local
        if (MDNS.begin(hostname.c_str())) {
            MDNS.addService("http", "tcp", 80);
        }
        #endif

        wifiNeedsRestart = false;
    }

    return WiFi.status();
}

const char mqttTab[] PROGMEM = "<a class=\"nav-item nav-link\" id=\"nav-mqtt-tab\" data-toggle=\"tab\" href=\"#nav-mqtt\" role=\"tab\" aria-controls=\"nav-mqtt\" aria-selected=\"false\"><i class=\"fas fa-network-wired\"></i> MQTT</a>";
const char ftpTab[] PROGMEM = "<a class=\"nav-item nav-link\" id=\"nav-ftp-tab\" data-toggle=\"tab\" href=\"#nav-ftp\" role=\"tab\" aria-controls=\"nav-ftp\" aria-selected=\"false\"><i class=\"fas fa-folder\"></i> FTP</a>";

// Used for substitution of some variables/templates of html-files. Is called by webserver's template-engine
String templateProcessor(const String& templ) {
    if (templ == "FTP_USER") {
        return prefsSettings.getString("ftpuser", "-1");
    } else if (templ == "FTP_PWD") {
        return prefsSettings.getString("ftppassword", "-1");
    } else if (templ == "FTP_USER_LENGTH") {
        return String(ftpUserLength-1);
    } else if (templ == "FTP_PWD_LENGTH") {
        return String(ftpPasswordLength-1);
    } else if (templ == "SHOW_FTP_TAB") {         // Only show FTP-tab if FTP-support was compiled
        #ifdef FTP_ENABLE
            return (String) FPSTR(ftpTab);
        #else
            return String();
        #endif
    } else if (templ == "INIT_LED_BRIGHTNESS") {
        return String(prefsSettings.getUChar("iLedBrightness", 0));
    } else if (templ == "NIGHT_LED_BRIGHTNESS") {
        return String(prefsSettings.getUChar("nLedBrightness", 0));
    } else if (templ == "MAX_INACTIVITY") {
        return String(prefsSettings.getUInt("mInactiviyT", 0));
    } else if (templ == "INIT_VOLUME") {
        return String(prefsSettings.getUInt("initVolume", 0));
    } else if (templ == "MAX_VOLUME_SPEAKER") {
        return String(prefsSettings.getUInt("maxVolumeSp", 0));
    } else if (templ == "MAX_VOLUME_HEADPHONE") {
        return String(prefsSettings.getUInt("maxVolumeHp", 0));
    } else if (templ == "WARNING_LOW_VOLTAGE") {
        return String(prefsSettings.getFloat("wLowVoltage", warningLowVoltage));
    } else if (templ == "VOLTAGE_INDICATOR_LOW") {
        return String(prefsSettings.getFloat("vIndicatorLow", voltageIndicatorLow));
    } else if (templ == "VOLTAGE_INDICATOR_HIGH") {
        return String(prefsSettings.getFloat("vIndicatorHigh", voltageIndicatorHigh));
    } else if (templ == "VOLTAGE_CHECK_INTERVAL") {
        return String(prefsSettings.getUInt("vCheckIntv", voltageCheckInterval));
    } else if (templ == "MQTT_SERVER") {
        return prefsSettings.getString("mqttServer", "-1");
    } else if (templ == "SHOW_MQTT_TAB") {        // Only show MQTT-tab if MQTT-support was compiled
        #ifdef MQTT_ENABLE
            return (String) FPSTR(mqttTab);
        #else
            return String();
        #endif
    } else if (templ == "MQTT_ENABLE") {
        if (enableMqtt) {
            return String("checked=\"checked\"");
        } else {
            return String();
        }
    } else if (templ == "MQTT_USER") {
        return prefsSettings.getString("mqttUser", "-1");
    } else if (templ == "MQTT_PWD") {
        return prefsSettings.getString("mqttPassword", "-1");
    } else if (templ == "MQTT_USER_LENGTH") {
        return String(mqttUserLength-1);
    } else if (templ == "MQTT_PWD_LENGTH") {
        return String(mqttPasswordLength-1);
    } else if (templ == "MQTT_SERVER_LENGTH") {
        return String(mqttServerLength-1);
    } else if (templ == "IPv4") {
        myIP = WiFi.localIP();
        snprintf(logBuf, serialLoglength, "%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
        return String(logBuf);
    } else if (templ == "RFID_TAG_ID") {
        return String(currentRfidTagId);
    } else if (templ == "HOSTNAME") {
        return prefsSettings.getString("Hostname", "-1");
    }

    return String();
}


// Takes inputs from webgui, parses JSON and saves values in NVS
// If operation was successful (NVS-write is verified) true is returned
bool processJsonRequest(char *_serialJson) {
    StaticJsonDocument<1000> doc;
    DeserializationError error = deserializeJson(doc, _serialJson);
    JsonObject object = doc.as<JsonObject>();

    if (error) {
        #if (LANGUAGE == 1)
            Serial.print(F("deserializeJson() fehlgeschlagen: "));
        #else
            Serial.print(F("deserializeJson() failed: "));
        #endif
        Serial.println(error.c_str());
        return false;
    }

    if (doc.containsKey("general")) {
        uint8_t iVol = doc["general"]["iVol"].as<uint8_t>();
        uint8_t mVolSpeaker = doc["general"]["mVolSpeaker"].as<uint8_t>();
        uint8_t mVolHeadphone = doc["general"]["mVolHeadphone"].as<uint8_t>();
        uint8_t iBright = doc["general"]["iBright"].as<uint8_t>();
        uint8_t nBright = doc["general"]["nBright"].as<uint8_t>();
        uint8_t iTime = doc["general"]["iTime"].as<uint8_t>();
        float vWarning = doc["general"]["vWarning"].as<float>();
        float vIndLow = doc["general"]["vIndLow"].as<float>();
        float vIndHi = doc["general"]["vIndHi"].as<float>();
        uint8_t vInt = doc["general"]["vInt"].as<uint8_t>();

        prefsSettings.putUInt("initVolume", iVol);
        prefsSettings.putUInt("maxVolumeSp", mVolSpeaker);
        prefsSettings.putUInt("maxVolumeHp", mVolHeadphone);
        prefsSettings.putUChar("iLedBrightness", iBright);
        prefsSettings.putUChar("nLedBrightness", nBright);
        prefsSettings.putUInt("mInactiviyT", iTime);
        prefsSettings.putFloat("wLowVoltage", vWarning);
        prefsSettings.putFloat("vIndicatorLow", vIndLow);
        prefsSettings.putFloat("vIndicatorHigh", vIndHi);
        prefsSettings.putUInt("vCheckIntv", vInt);

        // Check if settings were written successfully
        if (prefsSettings.getUInt("initVolume", 0) != iVol ||
            prefsSettings.getUInt("maxVolumeSp", 0) != mVolSpeaker ||
            prefsSettings.getUInt("maxVolumeHp", 0) != mVolHeadphone ||
            prefsSettings.getUChar("iLedBrightness", 0) != iBright ||
            prefsSettings.getUChar("nLedBrightness", 0) != nBright ||
            prefsSettings.getUInt("mInactiviyT", 0) != iTime ||
            prefsSettings.getFloat("wLowVoltage", 999.99) != vWarning ||
            prefsSettings.getFloat("vIndicatorLow", 999.99) != vIndLow ||
            prefsSettings.getFloat("vIndicatorHigh", 999.99) != vIndHi ||
            prefsSettings.getUInt("vCheckIntv", 17777) != vInt) {
            return false;
        }

    } else if (doc.containsKey("ftp")) {
        const char *_ftpUser = doc["ftp"]["ftpUser"];
        const char *_ftpPwd = doc["ftp"]["ftpPwd"];

        prefsSettings.putString("ftpuser", (String) _ftpUser);
        prefsSettings.putString("ftppassword", (String) _ftpPwd);

        if (!(String(_ftpUser).equals(prefsSettings.getString("ftpuser", "-1")) ||
           String(_ftpPwd).equals(prefsSettings.getString("ftppassword", "-1")))) {
            return false;
        }

    } else if (doc.containsKey("mqtt")) {
        uint8_t _mqttEnable = doc["mqtt"]["mqttEnable"].as<uint8_t>();
        const char *_mqttServer = object["mqtt"]["mqttServer"];
        prefsSettings.putUChar("enableMQTT", _mqttEnable);
        prefsSettings.putString("mqttServer", (String) _mqttServer);
        const char *_mqttUser = doc["mqtt"]["mqttUser"];
        const char *_mqttPwd = doc["mqtt"]["mqttPwd"];

        prefsSettings.putUChar("enableMQTT", _mqttEnable);
        prefsSettings.putUChar("enableMQTT", _mqttEnable);
        prefsSettings.putString("mqttServer", (String) _mqttServer);
        prefsSettings.putString("mqttServer", (String) _mqttServer);
        prefsSettings.putString("mqttUser", (String) _mqttUser);
        prefsSettings.putString("mqttPassword", (String) _mqttPwd);

        if ((prefsSettings.getUChar("enableMQTT", 99) != _mqttEnable) ||
            (!String(_mqttServer).equals(prefsSettings.getString("mqttServer", "-1")))) {
            return false;
        }

    } else if (doc.containsKey("rfidMod")) {
        const char *_rfidIdModId = object["rfidMod"]["rfidIdMod"];
        uint8_t _modId = object["rfidMod"]["modId"];
        char rfidString[12];
        snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s0%s0%s%u%s0", stringDelimiter, stringDelimiter, stringDelimiter, _modId, stringDelimiter);
        prefsRfid.putString(_rfidIdModId, rfidString);

        String s = prefsRfid.getString(_rfidIdModId, "-1");
        if (s.compareTo(rfidString)) {
            return false;
        }
        dumpNvsToSd("rfidTags", (char *) FPSTR(backupFile));        // Store backup-file every time when a new rfid-tag is programmed

    } else if (doc.containsKey("rfidAssign")) {
        const char *_rfidIdAssinId = object["rfidAssign"]["rfidIdMusic"];
        char _fileOrUrlAscii[MAX_FILEPATH_LENTGH];
        convertUtf8ToAscii(object["rfidAssign"]["fileOrUrl"], _fileOrUrlAscii);
        uint8_t _playMode = object["rfidAssign"]["playMode"];
        char rfidString[275];
        snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s%s%s0%s%u%s0", stringDelimiter, _fileOrUrlAscii, stringDelimiter, stringDelimiter, _playMode, stringDelimiter);
        prefsRfid.putString(_rfidIdAssinId, rfidString);
        Serial.println(_rfidIdAssinId);
        Serial.println(rfidString);

        String s = prefsRfid.getString(_rfidIdAssinId, "-1");
        if (s.compareTo(rfidString)) {
            return false;
        }
        dumpNvsToSd("rfidTags", (char *) FPSTR(backupFile));                     // Store backup-file every time when a new rfid-tag is programmed

    } else if (doc.containsKey("wifiConfig")) {
        const char *_ssid = object["wifiConfig"]["ssid"];
        const char *_pwd = object["wifiConfig"]["pwd"];
        const char *_hostname = object["wifiConfig"]["hostname"];

        prefsSettings.putString("SSID", _ssid);
        prefsSettings.putString("Password", _pwd);
        prefsSettings.putString("Hostname", (String) _hostname);

        String sSsid = prefsSettings.getString("SSID", "-1");
        String sPwd = prefsSettings.getString("Password", "-1");
        String sHostname = prefsSettings.getString("Hostname", "-1");

        if (sSsid.compareTo(_ssid) || sPwd.compareTo(_pwd)) {
            return false;
        }
    } else if (doc.containsKey("ping")) {
        sendWebsocketData(0, 20);
        return false;
    }

    return true;
}

char *jBuf = (char *) calloc(255, sizeof(char));        // In heap to save static memory
// Sends JSON-answers via websocket
void sendWebsocketData(uint32_t client, uint8_t code) {
    const size_t CAPACITY = JSON_OBJECT_SIZE(1) + 20;
    StaticJsonDocument<CAPACITY> doc;
    JsonObject object = doc.to<JsonObject>();

    if (code == 1) {
        object["status"] = "ok";
    } else if (code == 2) {
        object["status"] = "error";
    } else if (code == 10) {
        object["rfidId"] = currentRfidTagId;
    } else if (code == 20) {
        object["pong"] = "pong";
    }

    serializeJson(doc, jBuf, 255);

    if (client == 0) {
        ws.printfAll(jBuf);
    } else {
        ws.printf(client, jBuf);
    }
}


// Processes websocket-requests
void onWebsocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
    if (type == WS_EVT_CONNECT){
        //client connected
        Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
        //client->printf("Hello Client %u :)", client->id());
        client->ping();
    } else if (type == WS_EVT_DISCONNECT) {
        //client disconnected
        Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), uint8_t(client->id()));
    } else if (type == WS_EVT_ERROR) {
        //error was received from the other end
        Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    } else if (type == WS_EVT_PONG) {
        //pong message was received (in response to a ping request maybe)
        Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
    } else if (type == WS_EVT_DATA) {
        //data packet
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len) {
        //the whole message is in a single frame and we got all of it's data
            Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

            if (processJsonRequest((char*)data)) {
                sendWebsocketData(client->id(), 1);
            }

            if (info->opcode == WS_TEXT) {
                data[len] = 0;
                Serial.printf("%s\n", (char*)data);
            } else {
                for (size_t i=0; i < info->len; i++){
                    Serial.printf("%02x ", data[i]);
                }
                Serial.printf("\n");
            }
        }
    }
}

// Set maxVolume depending on headphone-adjustment is enabled and headphone is/is not connected
void setupVolume(void) {

    #ifndef HEADPHONE_ADJUST_ENABLE
        maxVolume = maxVolumeSpeaker;
        return;
    #else
        if (digitalRead(HP_DETECT)) {
            maxVolume = maxVolumeSpeaker;               // 1 if headphone is not connected
        } else {
            maxVolume = maxVolumeHeadphone;             // 0 if headphone is connected (put to GND)
        }
        snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(maxVolumeSet), maxVolume);
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        return;
    #endif
}


#ifdef HEADPHONE_ADJUST_ENABLE
    void headphoneVolumeManager(void) {
        bool currentHeadPhoneDetectionState = digitalRead(HP_DETECT);

        if (headphoneLastDetectionState != currentHeadPhoneDetectionState && (millis() - headphoneLastDetectionTimestamp >= headphoneLastDetectionDebounce)) {
            if (currentHeadPhoneDetectionState) {
                maxVolume = maxVolumeSpeaker;
            } else {
                maxVolume = maxVolumeHeadphone;
                if (currentVolume > maxVolume) {
                    volumeToQueueSender(maxVolume);         // Lower volume for headphone if headphone's maxvolume is exceeded by volume set in speaker-mode
                }
            }
            headphoneLastDetectionState = currentHeadPhoneDetectionState;
            headphoneLastDetectionTimestamp = millis();
            snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(maxVolumeSet), maxVolume);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        }
    }
#endif


bool isNumber(const char *str) {
  byte i = 0;

  while (*(str + i) != '\0') {
    if (!isdigit(*(str + i++))) {
      return false;
    }
  }

  if (i>0) {
      return true;
  } else {
      return false;
  }

}

void webserverStart(void) {
    if (wifiManager() == WL_CONNECTED && !webserverStarted) {
    // attach AsyncWebSocket for Mgmt-Interface
    ws.onEvent(onWebsocketEvent);
    wServer.addHandler(&ws);

    // attach AsyncEventSource
    wServer.addHandler(&events);

    // Default
    wServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", management_HTML, templateProcessor);
    });

    // NVS-backup-upload
    wServer.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", backupRecoveryWebsite);
    }, handleUpload);

    // ESP-restart
    wServer.on("/restart", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", restartWebsite);
        Serial.flush();
        ESP.restart();
    });

    // Fileexplorer (realtime)
    wServer.on("/explorer", HTTP_GET, explorerHandleListRequest);

    wServer.on("/explorer", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
    }, explorerHandleFileUpload);

    wServer.on("/explorer", HTTP_DELETE, explorerHandleDeleteRequest);

    wServer.on("/explorer", HTTP_PUT, explorerHandleCreateRequest);

    wServer.on("/explorer", HTTP_PATCH, explorerHandleRenameRequest);

    wServer.on("/exploreraudio", HTTP_POST, explorerHandleAudioRequest);

    wServer.onNotFound(notFound);

    // allow cors for local debug
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    wServer.begin();
    webserverStarted = true;
    }
}

// Dumps all RFID-entries from NVS into a file on SD-card
    bool dumpNvsToSd(char *_namespace, char *_destFile) {
        #ifdef NEOPIXEL_ENABLE
            pauseNeopixel = true;   // Workaround to prevent exceptions due to Neopixel-signalisation while NVS-write
        #endif
        esp_partition_iterator_t pi;                // Iterator for find
        const esp_partition_t* nvs;                 // Pointer to partition struct
        esp_err_t result = ESP_OK;
        const char* partname = "nvs";
        uint8_t pagenr = 0;                         // Page number in NVS
        uint8_t i;                                  // Index in Entry 0..125
        uint8_t bm;                                 // Bitmap for an entry
        uint32_t offset = 0;                        // Offset in nvs partition
        uint8_t namespace_ID;                       // Namespace ID found

        pi = esp_partition_find ( ESP_PARTITION_TYPE_DATA,          // Get partition iterator for
                                    ESP_PARTITION_SUBTYPE_ANY,      // this partition
                                    partname ) ;
        if (pi) {
            nvs = esp_partition_get(pi);                            // Get partition struct
            esp_partition_iterator_release(pi);                     // Release the iterator
            dbgprint ( "Partition %s found, %d bytes",
                    partname,
                    nvs->size ) ;
        } else {
            snprintf(logBuf, serialLoglength, "Partition %s not found!", partname);
            loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
            return NULL;
        }
        namespace_ID = FindNsID (nvs, _namespace) ;             // Find ID of our namespace in NVS
        File backupFile = FSystem.open(_destFile, FILE_WRITE);
        if (!backupFile) {
            return false;
        }
        while (offset < nvs->size) {
            result = esp_partition_read (nvs, offset,                // Read 1 page in nvs partition
                                        &buf,
                                        sizeof(nvs_page));
            if (result != ESP_OK) {
                snprintf(logBuf, serialLoglength, "Error reading NVS!");
                loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
                return false;
            }

            i = 0;

            while (i < 126) {
                bm = (buf.Bitmap[i/4] >> ((i % 4) * 2 )) & 0x03;  // Get bitmap for this entry
                if (bm == 2) {
                    if ((namespace_ID == 0xFF) ||                      // Show all if ID = 0xFF
                        (buf.Entry[i].Ns == namespace_ID)) {           // otherwise just my namespace
                        if (isNumber(buf.Entry[i].Key)) {
                            String s = prefsRfid.getString((const char *)buf.Entry[i].Key);
                            backupFile.printf("%s%s%s%s\n", stringOuterDelimiter, buf.Entry[i].Key, stringOuterDelimiter, s.c_str());
                        }
                    }
                    i += buf.Entry[i].Span;                              // Next entry
                } else {
                    i++;
                }
            }
            offset += sizeof(nvs_page);                              // Prepare to read next page in nvs
            pagenr++;
        }

        backupFile.close();
        #ifdef NEOPIXEL_ENABLE
            pauseNeopixel = false;
        #endif

        return true;
    }

// Conversion routine
void convertAsciiToUtf8(String asciiString, char *utf8String) {

    int k=0;

    for (int i=0; i<asciiString.length() && k<MAX_FILEPATH_LENTGH-2; i++)
    {

        switch (asciiString[i]) {
            case 0x8e: utf8String[k++]=0xc3; utf8String[k++]=0x84;  break; // Ä
            case 0x84: utf8String[k++]=0xc3; utf8String[k++]=0xa4;  break; // ä
            case 0x9a: utf8String[k++]=0xc3; utf8String[k++]=0x9c;  break; // Ü
            case 0x81: utf8String[k++]=0xc3; utf8String[k++]=0xbc;  break; // ü
            case 0x99: utf8String[k++]=0xc3; utf8String[k++]=0x96;  break; // Ö
            case 0x94: utf8String[k++]=0xc3; utf8String[k++]=0xb6;  break; // ö
            case 0xe1: utf8String[k++]=0xc3; utf8String[k++]=0x9f;  break; // ß
            default: utf8String[k++]=asciiString[i];
        }
    }

    utf8String[k]=0;

}

void convertUtf8ToAscii(String utf8String, char *asciiString) {

  int k=0;
  bool f_C3_seen = false;

  for (int i=0; i<utf8String.length() && k<MAX_FILEPATH_LENTGH-1; i++)
  {

    if(utf8String[i] == 195){                                   // C3
      f_C3_seen = true;
      continue;
    } else {
      if (f_C3_seen == true) {
        f_C3_seen = false;
        switch (utf8String[i]) {
          case 0x84: asciiString[k++]=0x8e;  break; // Ä
          case 0xa4: asciiString[k++]=0x84;  break; // ä
          case 0x9c: asciiString[k++]=0x9a;  break; // Ü
          case 0xbc: asciiString[k++]=0x81;  break; // ü
          case 0x96: asciiString[k++]=0x99;  break; // Ö
          case 0xb6: asciiString[k++]=0x94;  break; // ö
          case 0x9f: asciiString[k++]=0xe1;  break; // ß
          default: asciiString[k++]=0xdb;  // Unknow...
        }
      } else {
        asciiString[k++]=utf8String[i];
      }

    }
  }

  asciiString[k]=0;

}

// Handles file upload request from the explorer
// requires a GET parameter path, as directory path to the file
void explorerHandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {

    lastTimeActiveTimestamp = millis();

    // New File
    if (!index) {
        String utf8FilePath;
        static char filePath[MAX_FILEPATH_LENTGH];
        if (request->hasParam("path")) {
            AsyncWebParameter *param = request->getParam("path");
            utf8FilePath = param->value() + "/" + filename;

        } else {
            utf8FilePath = "/" + filename;
        }

        convertUtf8ToAscii(utf8FilePath, filePath);

        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(writingFile), utf8FilePath.c_str());
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);

        // Create Ringbuffer for upload
        if(explorerFileUploadRingBuffer == NULL) {
            explorerFileUploadRingBuffer = xRingbufferCreate(4096, RINGBUF_TYPE_BYTEBUF);
        }

        // Create Queue for receiving a signal from the store task as synchronisation
        if(explorerFileUploadStatusQueue == NULL) {
            explorerFileUploadStatusQueue = xQueueCreate(1, sizeof(uint8_t));
        }

        // Create Task for handling the storage of the data
        xTaskCreate(
            explorerHandleFileStorageTask, /* Function to implement the task */
            "fileStorageTask", /* Name of the task */
            4000,  /* Stack size in words */
            filePath,  /* Task input parameter */
            2 | portPRIVILEGE_BIT,  /* Priority of the task */
            &fileStorageTaskHandle  /* Task handle. */
        );

    }

    if (len) {
        // stream the incoming chunk to the ringbuffer
        xRingbufferSend(explorerFileUploadRingBuffer, data, len, portTICK_PERIOD_MS * 1000);
    }

    if (final) {
        // notify storage task that last data was stored on the ring buffer
        xTaskNotify(fileStorageTaskHandle, 1u, eNoAction);
        // watit until the storage task is sending the signal to finish
        uint8_t signal;
        xQueueReceive(explorerFileUploadStatusQueue, &signal, portMAX_DELAY);

        // delete task
        vTaskDelete(fileStorageTaskHandle);
    }
}

void explorerHandleFileStorageTask(void *parameter) {

    File uploadFile;
    size_t item_size;
    uint8_t *item;
    uint8_t value = 0;

    BaseType_t uploadFileNotification;
    uint32_t uploadFileNotificationValue;

    uploadFile = FSystem.open((char *)parameter, "w");

    for(;;) {
        esp_task_wdt_reset();

        item = (uint8_t *)xRingbufferReceive(explorerFileUploadRingBuffer, &item_size, portTICK_PERIOD_MS * 100);
        if (item != NULL) {
            uploadFile.write(item, item_size);
            vRingbufferReturnItem(explorerFileUploadRingBuffer, (void *)item);
        } else {
            // No data in the buffer, check if all data arrived for the file
            uploadFileNotification = xTaskNotifyWait(0,0,&uploadFileNotificationValue,0);
            if(uploadFileNotification == pdPASS) {
                uploadFile.close();
                // done exit loop to terminate
                break;
            }
            vTaskDelay(portTICK_PERIOD_MS * 100);
        }
    }
    // send signal to upload function to terminate
    xQueueSend(explorerFileUploadStatusQueue, &value, 0);
    vTaskDelete(NULL);
}

// Sends a list of the content of a directory as JSON file
// requires a GET parameter path for the directory
void explorerHandleListRequest(AsyncWebServerRequest *request) {
    DynamicJsonDocument jsonBuffer(16384);
    //StaticJsonDocument<4096> jsonBuffer;
    String serializedJsonString;
    AsyncWebParameter *param;
    char filePath[MAX_FILEPATH_LENTGH];
    JsonArray obj = jsonBuffer.createNestedArray();
    File root;
    if(request->hasParam("path")){
        param = request->getParam("path");
        convertUtf8ToAscii(param->value(), filePath);
        root = FSystem.open(filePath);
    } else {
        root = FSystem.open("/");
    }

    if (!root) {
        snprintf(logBuf, serialLoglength, (char *) FPSTR(failedToOpenDirectory));
        loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);
        return;
    }

    if (!root.isDirectory()) {
        snprintf(logBuf, serialLoglength, (char *) FPSTR(notADirectory));
        loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);
        return;
    }

    File file = root.openNextFile();

    while(file) {
        JsonObject entry = obj.createNestedObject();
        convertAsciiToUtf8(file.name(), filePath);
        std::string path = filePath;
        std::string fileName = path.substr(path.find_last_of("/") + 1);

        entry["name"] = fileName;
        entry["dir"].set(file.isDirectory());

        file = root.openNextFile();

        esp_task_wdt_reset();

    }

    serializeJson(obj, serializedJsonString);
    request->send(200, "application/json; charset=utf-8", serializedJsonString);
}

bool explorerDeleteDirectory(File dir) {

    File file = dir.openNextFile();
    while(file) {

        if(file.isDirectory()) {
            explorerDeleteDirectory(file);
        } else {
            FSystem.remove(file.name());
        }

        file = dir.openNextFile();

        esp_task_wdt_reset();
    }

    return FSystem.rmdir(dir.name());

}

// Handles delete request of a file or directory
// requires a GET parameter path to the file or directory
void explorerHandleDeleteRequest(AsyncWebServerRequest *request) {
    File file;
    AsyncWebParameter *param;
    char filePath[MAX_FILEPATH_LENTGH];
    if(request->hasParam("path")){
        param = request->getParam("path");
        convertUtf8ToAscii(param->value(), filePath);
        if(FSystem.exists(filePath)) {
            file = FSystem.open(filePath);
            if(file.isDirectory()) {
                if(explorerDeleteDirectory(file)) {
                    snprintf(logBuf, serialLoglength, "DELETE:  %s deleted", param->value().c_str());
                    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
                } else {
                    snprintf(logBuf, serialLoglength, "DELETE:  Cannot delete %s", param->value().c_str());
                    loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
                }
            } else {
                if(FSystem.remove(filePath)) {
                    snprintf(logBuf, serialLoglength, "DELETE:  %s deleted", param->value().c_str());
                    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
                } else {
                    snprintf(logBuf, serialLoglength, "DELETE:  Cannot delete %s", param->value().c_str());
                    loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
                }
            }
        } else {
            snprintf(logBuf, serialLoglength, "DELETE: Path %s does not exist", param->value().c_str());
            loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
        }
    } else {
        loggerNl(serialDebug, "DELETE: No path variable set", LOGLEVEL_ERROR);
    }
    request->send(200);
    esp_task_wdt_reset();
}

// Handles create request of a directory
// requires a GET parameter path to the new directory
void explorerHandleCreateRequest(AsyncWebServerRequest *request) {
    AsyncWebParameter *param;
    char filePath[MAX_FILEPATH_LENTGH];
    if(request->hasParam("path")){
        param = request->getParam("path");
        convertUtf8ToAscii(param->value(), filePath);
        if(FSystem.mkdir(filePath)) {
            snprintf(logBuf, serialLoglength, "CREATE:  %s created", param->value().c_str());
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {
            snprintf(logBuf, serialLoglength, "CREATE:  Cannot create %s", param->value().c_str());
            loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
        }
    } else {
        loggerNl(serialDebug, "CREATE: No path variable set", LOGLEVEL_ERROR);
    }
    request->send(200);
}

// Handles rename request of a file or directory
// requires a GET parameter srcpath to the old file or directory name
// requires a GET parameter dstpath to the new file or directory name
void explorerHandleRenameRequest(AsyncWebServerRequest *request) {
    AsyncWebParameter *srcPath;
    AsyncWebParameter *dstPath;
    char srcFullFilePath[MAX_FILEPATH_LENTGH];
    char dstFullFilePath[MAX_FILEPATH_LENTGH];
    if(request->hasParam("srcpath") && request->hasParam("dstpath")) {
        srcPath = request->getParam("srcpath");
        dstPath = request->getParam("dstpath");
        convertUtf8ToAscii(srcPath->value(), srcFullFilePath);
        convertUtf8ToAscii(dstPath->value(), dstFullFilePath);
        if(FSystem.exists(srcFullFilePath)) {
            if(FSystem.rename(srcFullFilePath, dstFullFilePath)) {
                    snprintf(logBuf, serialLoglength, "RENAME:  %s renamed to %s", srcPath->value().c_str(), dstPath->value().c_str());
                    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
            } else {
                    snprintf(logBuf, serialLoglength, "RENAME:  Cannot rename %s", srcPath->value().c_str());
                    loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
            }
        } else {
            snprintf(logBuf, serialLoglength, "RENAME: Path %s does not exist", srcPath->value().c_str());
            loggerNl(serialDebug, logBuf, LOGLEVEL_ERROR);
        }
    } else {
        loggerNl(serialDebug, "RENAME: No path variable set", LOGLEVEL_ERROR);
    }

    request->send(200);
}

// Handles audio play requests
// requires a GET parameter path to the audio file or directory
// requires a GET parameter playmode
void explorerHandleAudioRequest(AsyncWebServerRequest *request) {
    AsyncWebParameter *param;
    String playModeString;
    uint32_t playMode;
    char filePath[MAX_FILEPATH_LENTGH];
    if(request->hasParam("path") && request->hasParam("playmode")) {
        param = request->getParam("path");
        convertUtf8ToAscii(param->value(), filePath);
        param = request->getParam("playmode");
        playModeString = param->value();

        playMode = atoi(playModeString.c_str());
        trackQueueDispatcher(filePath,0,playMode,0);
    } else {
        loggerNl(serialDebug, "AUDIO: No path variable set", LOGLEVEL_ERROR);
    }

    request->send(200);
}


// Handles uploaded backup-file and writes valid entries into NVS
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    #ifdef NEOPIXEL_ENABLE
        pauseNeopixel = true;   // Workaround to prevent exceptions due to Neopixel-signalisation while NVS-write
    #endif
    char ebuf[290];
    uint16_t j=0;
    char *token;
    uint8_t count=0;
    nvs_t nvsEntry[1];

    for (size_t i=0; i<len; i++) {
        if (data[i] != '\n') {
            ebuf[j++] = data[i];
        } else {
            ebuf[j] = '\0';
            j=0;
            token = strtok(ebuf, stringOuterDelimiter);
            while (token != NULL) {
                if (!count) {
                    count++;
                    memcpy(nvsEntry[0].nvsKey, token, strlen(token));
                    nvsEntry[0].nvsKey[strlen(token)] = '\0';
                } else if (count == 1) {
                    count = 0;
                    memcpy(nvsEntry[0].nvsEntry, token, strlen(token));
                    nvsEntry[0].nvsEntry[strlen(token)] = '\0';
                }
                token = strtok(NULL, stringOuterDelimiter);
            }
            if (isNumber(nvsEntry[0].nvsKey) && nvsEntry[0].nvsEntry[0] == '#') {
                snprintf(logBuf, serialLoglength, "%s: %s => %s", (char *) FPSTR(writeEntryToNvs), nvsEntry[0].nvsKey, nvsEntry[0].nvsEntry);
                loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
                prefsRfid.putString(nvsEntry[0].nvsKey, nvsEntry[0].nvsEntry);
            }
        }
    }
    #ifdef NEOPIXEL_ENABLE
        pauseNeopixel = false;
    #endif
}

// Print the wake-up reason why ESP32 is awake now
void printWakeUpReason() {
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 : Serial.println(F("Wakeup caused by push button")); break;
        case ESP_SLEEP_WAKEUP_EXT1 : Serial.println(F("Wakeup caused by low power card detection")); break;
        case ESP_SLEEP_WAKEUP_TIMER : Serial.println(F("Wakeup caused by timer")); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println(F("Wakeup caused by touchpad")); break;
        case ESP_SLEEP_WAKEUP_ULP : Serial.println(F("Wakeup caused by ULP program")); break;
        default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }
}

#ifdef PN5180_ENABLE_LPCD
    // wake up from LPCD, check card is present. This works only for ISO-14443 compatible cards
    void checkCardIsPresentLPCD() {
        static PN5180ISO14443 nfc14443(RFID_CS, RFID_BUSY, RFID_RST);
        nfc14443.begin();
        nfc14443.reset();
        nfc14443.setupRF();
        if (!nfc14443.isCardPresent()) {
            nfc14443.clearIRQStatus(0xffffffff);
            Serial.print(F("Logic level at PN5180' IRQ-PIN: ")); Serial.println(digitalRead(RFID_IRQ));
            // turn on LPCD
            uint16_t wakeupCounterInMs = 0x3FF; //  needs to be in the range of 0x0 - 0xA82. max wake-up time is 2960 ms.
            if (nfc14443.switchToLPCD(wakeupCounterInMs)) {
                loggerNl(serialDebug, (char *) FPSTR(lowPowerCardSuccess), LOGLEVEL_INFO);
                // configure wakeup pin for deep-sleep wake-up, use ext1
                esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
                // freeze pin states in deep sleep
                gpio_hold_en(gpio_num_t(RFID_CS));  // CS/NSS
                gpio_hold_en(gpio_num_t(RFID_RST)); // RST
                gpio_deep_sleep_hold_en();
                loggerNl(serialDebug, (char *) FPSTR(wakeUpRfidNoIso14443), LOGLEVEL_ERROR);
                esp_deep_sleep_start();
        } else {
                Serial.println(F("switchToLPCD failed"));
            }
        }
    }
#endif

void setup() {
    Serial.begin(115200);
    esp_sleep_enable_ext0_wakeup((gpio_num_t) DREHENCODER_BUTTON, 0);
    #ifdef PN5180_ENABLE_LPCD
        // disable pin hold from deep sleep (LPCD)
        gpio_deep_sleep_hold_dis();
        gpio_hold_dis(gpio_num_t(RFID_CS)); // NSS
        gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
        pinMode(RFID_IRQ, INPUT);
        // check wakeup reason is a card detection
        esp_sleep_wakeup_cause_t wakeup_reason;
        wakeup_reason = esp_sleep_get_wakeup_cause();
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
            checkCardIsPresentLPCD();
        }
    #endif
    srand(esp_random());
    pinMode(POWER, OUTPUT);
    digitalWrite(POWER, HIGH);
    prefsRfid.begin((char *) FPSTR(prefsRfidNamespace));
    prefsSettings.begin((char *) FPSTR(prefsSettingsNamespace));

    playProperties.playMode = NO_PLAYLIST;
    playProperties.playlist = NULL;
    playProperties.repeatCurrentTrack = false;
    playProperties.repeatPlaylist = false;
    playProperties.currentTrackNumber = 0;
    playProperties.numberOfTracks = 0;
    playProperties.startAtFilePos = 0;
    playProperties.currentRelPos = 0;
    playProperties.sleepAfterCurrentTrack = false;
    playProperties.sleepAfterPlaylist = false;
    playProperties.saveLastPlayPosition = false;
    playProperties.pausePlay = false;
    playProperties.trackFinished = NULL;
    playProperties.playlistFinished = true;

    // Examples for serialized RFID-actions that are stored in NVS
    // #<file/folder>#<startPlayPositionInBytes>#<playmode>#<trackNumberToStartWith>
    // Please note: There's no need to do this manually (unless you want to)
    /*prefsRfid.putString("215123125075", "#/mp3/Kinderlieder#0#6#0");
    prefsRfid.putString("169239075184", "#http://radio.koennmer.net/evosonic.mp3#0#8#0");
    prefsRfid.putString("244105171042", "#0#0#111#0"); // modification-card (repeat track)
    prefsRfid.putString("228064156042", "#0#0#110#0"); // modification-card (repeat playlist)
    prefsRfid.putString("212130160042", "#/mp3/Hoerspiele/Yakari/Sammlung2#0#3#0");*/

#ifdef NEOPIXEL_ENABLE
    xTaskCreatePinnedToCore(
        showLed, /* Function to implement the task */
        "LED", /* Name of the task */
        2000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        1 | portPRIVILEGE_BIT,  /* Priority of the task */
        &LED,  /* Task handle. */
        0 /* Core where the task should run */
    );
#endif

#if (HAL == 2)
    i2cBusOne.begin(IIC_DATA, IIC_CLK, 40000);

    while (not ac.begin()) {
        Serial.println(F("AC101 Failed!"));
        delay(1000);
    }
    Serial.println(F("AC101 via I2C - OK!"));

    pinMode(22, OUTPUT);
    digitalWrite(22, HIGH);

    pinMode(GPIO_PA_EN, OUTPUT);
    digitalWrite(GPIO_PA_EN, HIGH);
    Serial.println(F("Built-in amplifier enabled\n"));
#endif

    #ifdef RFID_READER_TYPE_MFRC522_SPI
        #if (HAL == 4)
            SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_CS);     // ToDo: Not sure if this should be the default-case
        #else
            SPI.begin();
        #endif
        SPI.setFrequency(1000000);
    #endif

    #ifndef SINGLE_SPI_ENABLE
        #ifdef SD_MMC_1BIT_MODE
            pinMode(2, INPUT_PULLUP);
            while (!SD_MMC.begin("/sdcard", true)) {
        #else
            pinMode(SPISD_CS, OUTPUT);
            digitalWrite(SPISD_CS, HIGH);
            spiSD.begin(SPISD_SCK, SPISD_MISO, SPISD_MOSI, SPISD_CS);
            spiSD.setFrequency(1000000);
            while (!SD.begin(SPISD_CS, spiSD)) {
        #endif
    #else
        #ifdef SD_MMC_1BIT_MODE
            pinMode(2, INPUT_PULLUP);
            while (!SD_MMC.begin("/sdcard", true)) {
        #else
            while (!SD.begin(SPISD_CS)) {
        #endif
    #endif
                loggerNl(serialDebug, (char *) FPSTR(unableToMountSd), LOGLEVEL_ERROR);
                delay(500);
                #ifdef SHUTDOWN_IF_SD_BOOT_FAILS
                    if (millis() >= deepsleepTimeAfterBootFails*1000) {
                        loggerNl(serialDebug, (char *) FPSTR(sdBootFailedDeepsleep), LOGLEVEL_ERROR);
                        esp_deep_sleep_start();
                    }
                #endif
            }

    #ifdef RFID_READER_TYPE_MFRC522_I2C
        i2cBusTwo.begin(ext_IIC_DATA, ext_IIC_CLK, 40000);
        delay(50);
        loggerNl(serialDebug, (char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
    #endif

    #ifdef RFID_READER_TYPE_MFRC522_SPI
        mfrc522.PCD_Init();
        mfrc522.PCD_SetAntennaGain(rfidGain);
        delay(50);
        loggerNl(serialDebug, (char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
    #endif

   // welcome message
   Serial.println(F(""));
   Serial.println(F("  _____   ____    ____            _                 "));
   Serial.println(F(" | ____| / ___|  |  _ \\   _   _  (_)  _ __     ___  "));
   Serial.println(F(" |  _|   \\__  \\  | |_) | | | | | | | | '_ \\   / _ \\"));
   Serial.println(F(" | |___   ___) | |  __/  | |_| | | | | | | | | (_) |"));
   Serial.println(F(" |_____| |____/  |_|      \\__,_| |_| |_| |_|  \\___/ "));
   Serial.println(F(" Rfid-controlled musicplayer\n"));
   // print wake-up reason
   printWakeUpReason();
   #ifdef PN5180_ENABLE_LPCD
     // disable pin hold from deep sleep
     gpio_deep_sleep_hold_dis();
     gpio_hold_dis(gpio_num_t(RFID_CS)); // NSS
     gpio_hold_dis(gpio_num_t(RFID_RST)); // RST
   #endif
  // show SD card type
    #ifdef SD_MMC_1BIT_MODE
      loggerNl(serialDebug, (char *) FPSTR(sdMountedMmc1BitMode), LOGLEVEL_NOTICE);
      uint8_t cardType = SD_MMC.cardType();
    #else
      loggerNl(serialDebug, (char *) FPSTR(sdMountedSpiMode), LOGLEVEL_NOTICE);
      uint8_t cardType = SD.cardType();
    #endif
    Serial.print(F("SD card type: "));
    if (cardType == CARD_MMC) {
        Serial.println(F("MMC"));
    } else if(cardType == CARD_SD){
        Serial.println(F("SDSC"));
    } else if(cardType == CARD_SDHC){
        Serial.println(F("SDHC"));
    } else {
        Serial.println(F("UNKNOWN"));
    }

    #ifdef HEADPHONE_ADJUST_ENABLE
        pinMode(HP_DETECT, INPUT);
        headphoneLastDetectionState = digitalRead(HP_DETECT);
    #endif

    // Create queues
    volumeQueue = xQueueCreate(1, sizeof(int));
    if (volumeQueue == NULL) {
        loggerNl(serialDebug, (char *) FPSTR(unableToCreateVolQ), LOGLEVEL_ERROR);
    }

    rfidCardQueue = xQueueCreate(1, (cardIdSize + 1) * sizeof(char));
    if (rfidCardQueue == NULL) {
        loggerNl(serialDebug, (char *) FPSTR(unableToCreateRfidQ), LOGLEVEL_ERROR);
    }

    trackControlQueue = xQueueCreate(1, sizeof(uint8_t));
    if (trackControlQueue == NULL) {
        loggerNl(serialDebug, (char *) FPSTR(unableToCreateMgmtQ), LOGLEVEL_ERROR);
    }

    char **playlistArray;
    trackQueue = xQueueCreate(1, sizeof(playlistArray));
    if (trackQueue == NULL) {
        loggerNl(serialDebug, (char *) FPSTR(unableToCreatePlayQ), LOGLEVEL_ERROR);
    }

    // Get some stuff from NVS...
    // Get initial LED-brightness from NVS
    uint8_t nvsILedBrightness = prefsSettings.getUChar("iLedBrightness", 0);
    if (nvsILedBrightness) {
            initialLedBrightness = nvsILedBrightness;
            ledBrightness = nvsILedBrightness;
        snprintf(logBuf, serialLoglength, "%s: %d", (char *) FPSTR(initialBrightnessfromNvs), nvsILedBrightness);
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUChar("iLedBrightness", initialLedBrightness);
        loggerNl(serialDebug, (char *) FPSTR(wroteInitialBrightnessToNvs), LOGLEVEL_ERROR);
    }

    // Get night LED-brightness from NVS
    uint8_t nvsNLedBrightness = prefsSettings.getUChar("nLedBrightness", 0);
    if (nvsNLedBrightness) {
        nightLedBrightness = nvsNLedBrightness;
        snprintf(logBuf, serialLoglength, "%s: %d", (char *) FPSTR(restoredInitialBrightnessForNmFromNvs), nvsNLedBrightness);
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUChar("nLedBrightness", nightLedBrightness);
        loggerNl(serialDebug, (char *) FPSTR(wroteNmBrightnessToNvs), LOGLEVEL_ERROR);
    }

    // Get FTP-user from NVS
    String nvsFtpUser = prefsSettings.getString("ftpuser", "-1");
    if (!nvsFtpUser.compareTo("-1")) {
        prefsSettings.putString("ftpuser", (String) ftpUser);
        loggerNl(serialDebug, (char *) FPSTR(wroteFtpUserToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(ftpUser, nvsFtpUser.c_str(), ftpUserLength);
        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(restoredFtpUserFromNvs), nvsFtpUser.c_str());
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    }

    // Get FTP-password from NVS
    String nvsFtpPassword = prefsSettings.getString("ftppassword", "-1");
    if (!nvsFtpPassword.compareTo("-1")) {
        prefsSettings.putString("ftppassword", (String) ftpPassword);
        loggerNl(serialDebug, (char *) FPSTR(wroteFtpPwdToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(ftpPassword, nvsFtpPassword.c_str(), ftpPasswordLength);
        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(restoredFtpPwdFromNvs), nvsFtpPassword.c_str());
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    }

    // Get maximum inactivity-time from NVS
    uint32_t nvsMInactivityTime = prefsSettings.getUInt("mInactiviyT", 0);
    if (nvsMInactivityTime) {
        maxInactivityTime = nvsMInactivityTime;
        snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(restoredMaxInactivityFromNvs), nvsMInactivityTime);
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("mInactiviyT", maxInactivityTime);
        loggerNl(serialDebug, (char *) FPSTR(wroteMaxInactivityToNvs), LOGLEVEL_ERROR);
    }

    #ifndef USE_LAST_VOLUME_AFTER_REBOOT
        // Get initial volume from NVS
        uint32_t nvsInitialVolume = prefsSettings.getUInt("initVolume", 0);
    #else
        // Get volume used at last shutdown
        uint32_t nvsInitialVolume = prefsSettings.getUInt("previousVolume", 999);
        if (nvsInitialVolume == 999) {
            prefsSettings.putUInt("previousVolume", initVolume);
            nvsInitialVolume = initVolume;
        } else {
            loggerNl(serialDebug, (char *) FPSTR(rememberLastVolume), LOGLEVEL_ERROR);
        }
    #endif
        if (nvsInitialVolume) {
            initVolume = nvsInitialVolume;
            snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(restoredInitialLoudnessFromNvs), nvsInitialVolume);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {
            prefsSettings.putUInt("initVolume", initVolume);
            loggerNl(serialDebug, (char *) FPSTR(wroteInitialLoudnessToNvs), LOGLEVEL_ERROR);
        }

    // Get maximum volume for speaker from NVS
    uint32_t nvsMaxVolumeSpeaker = prefsSettings.getUInt("maxVolumeSp", 0);
    if (nvsMaxVolumeSpeaker) {
        maxVolumeSpeaker = nvsMaxVolumeSpeaker;
        maxVolume = maxVolumeSpeaker;
        snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(restoredMaxLoudnessForSpeakerFromNvs), nvsMaxVolumeSpeaker);
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("maxVolumeSp", nvsMaxVolumeSpeaker);
        loggerNl(serialDebug, (char *) FPSTR(wroteMaxLoudnessForSpeakerToNvs), LOGLEVEL_ERROR);
    }

    #ifdef HEADPHONE_ADJUST_ENABLE
        // Get maximum volume for headphone from NVS
        uint32_t nvsMaxVolumeHeadphone = prefsSettings.getUInt("maxVolumeHp", 0);
        if (nvsMaxVolumeHeadphone) {
            maxVolumeHeadphone = nvsMaxVolumeHeadphone;
            snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(restoredMaxLoudnessForHeadphoneFromNvs), nvsMaxVolumeHeadphone);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {
            prefsSettings.putUInt("maxVolumeHp", nvsMaxVolumeHeadphone);
            loggerNl(serialDebug, (char *) FPSTR(wroteMaxLoudnessForHeadphoneToNvs), LOGLEVEL_ERROR);
        }
    #endif

    // Adjust volume depending on headphone is connected and volume-adjustment is enabled
    setupVolume();

    // Get MQTT-enable from NVS
    uint8_t nvsEnableMqtt = prefsSettings.getUChar("enableMQTT", 99);
    switch (nvsEnableMqtt) {
        case 99:
            prefsSettings.putUChar("enableMQTT", enableMqtt);
            loggerNl(serialDebug, (char *) FPSTR(wroteMqttFlagToNvs), LOGLEVEL_ERROR);
            break;
        case 1:
            //prefsSettings.putUChar("enableMQTT", enableMqtt);
            enableMqtt = nvsEnableMqtt;
            snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(restoredMqttActiveFromNvs), nvsEnableMqtt);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
            break;
        case 0:
            enableMqtt = nvsEnableMqtt;
            snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(restoredMqttDeactiveFromNvs), nvsEnableMqtt);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
            break;
    }

    // Get MQTT-server from NVS
    String nvsMqttServer = prefsSettings.getString("mqttServer", "-1");
    if (!nvsMqttServer.compareTo("-1")) {
        prefsSettings.putString("mqttServer", (String) mqtt_server);
        loggerNl(serialDebug, (char*) FPSTR(wroteMqttServerToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(mqtt_server, nvsMqttServer.c_str(), mqttServerLength);
        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(restoredMqttServerFromNvs), nvsMqttServer.c_str());
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    }

    // Get MQTT-user from NVS
    String nvsMqttUser = prefsSettings.getString("mqttUser", "-1");
    if (!nvsMqttUser.compareTo("-1")) {
        prefsSettings.putString("mqttUser", (String) mqttUser);
        loggerNl(serialDebug, (char *) FPSTR(wroteMqttUserToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(mqttUser, nvsMqttUser.c_str(), mqttUserLength);
        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(restoredMqttUserFromNvs), nvsMqttUser.c_str());
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    }

    // Get MQTT-password from NVS
    String nvsMqttPassword = prefsSettings.getString("mqttPassword", "-1");
    if (!nvsMqttPassword.compareTo("-1")) {
        prefsSettings.putString("mqttPassword", (String) mqttPassword);
        loggerNl(serialDebug, (char *) FPSTR(wroteMqttPwdToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(mqttPassword, nvsMqttPassword.c_str(), mqttPasswordLength);
        snprintf(logBuf, serialLoglength, "%s: %s", (char *) FPSTR(restoredMqttPwdFromNvs), nvsMqttPassword.c_str());
        loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    }

    #ifdef MEASURE_BATTERY_VOLTAGE
        // Get voltages from NVS for Neopixel
        float vLowIndicator = prefsSettings.getFloat("vIndicatorLow", 999.99);
        if (vLowIndicator <= 999) {
            voltageIndicatorLow = vLowIndicator;
            snprintf(logBuf, serialLoglength, "%s: %.2f V", (char *) FPSTR(voltageIndicatorLowFromNVS), vLowIndicator);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {    // preseed if not set
            prefsSettings.putFloat("vIndicatorLow", voltageIndicatorLow);
        }

        float vHighIndicator = prefsSettings.getFloat("vIndicatorHigh", 999.99);
        if (vHighIndicator <= 999) {
            voltageIndicatorHigh = vHighIndicator;
            snprintf(logBuf, serialLoglength, "%s: %.2f V", (char *) FPSTR(voltageIndicatorHighFromNVS), vHighIndicator);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {
            prefsSettings.putFloat("vIndicatorHigh", voltageIndicatorHigh);
        }

        float vLowWarning = prefsSettings.getFloat("wLowVoltage", 999.99);
        if (vLowWarning <= 999) {
            warningLowVoltage = vLowWarning;
            snprintf(logBuf, serialLoglength, "%s: %.2f V", (char *) FPSTR(warningLowVoltageFromNVS), vLowWarning);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {
            prefsSettings.putFloat("wLowVoltage", warningLowVoltage);
        }

        uint32_t vInterval = prefsSettings.getUInt("vCheckIntv", 17777);
        if (vInterval != 17777) {
            voltageCheckInterval = vInterval;
            snprintf(logBuf, serialLoglength, "%s: %u Minuten", (char *) FPSTR(voltageCheckIntervalFromNVS), vInterval);
            loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
        } else {
            prefsSettings.putUInt("vCheckIntv", voltageCheckInterval);
        }
    #endif

    // Create 1000Hz-HW-Timer (currently only used for buttons)
    timerSemaphore = xSemaphoreCreateBinary();
    timer = timerBegin(0, 240, true);           // Prescaler: CPU-clock in MHz
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000, true);         // 1000 Hz
    timerAlarmEnable(timer);

    // Create tasks
    xTaskCreatePinnedToCore(
        rfidScanner, /* Function to implement the task */
        "rfidhandling", /* Name of the task */
        2000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        1,  /* Priority of the task */
        &rfid,  /* Task handle. */
        0 /* Core where the task should run */
    );

    // Activate internal pullups for all buttons
    pinMode(DREHENCODER_BUTTON, INPUT_PULLUP);
    pinMode(PAUSEPLAY_BUTTON, INPUT_PULLUP);
    pinMode(NEXT_BUTTON, INPUT_PULLUP);
    pinMode(PREVIOUS_BUTTON, INPUT_PULLUP);

    // Init rotary encoder
    encoder.attachHalfQuad(DREHENCODER_CLK, DREHENCODER_DT);
    encoder.clearCount();
    encoder.setCount(initVolume*2);         // Ganzes Raster ist immer +2, daher initiale Lautstärke mit 2 multiplizieren

    // Only enable MQTT if requested
    #ifdef MQTT_ENABLE
        if (enableMqtt) {
            MQTTclient.setServer(mqtt_server, 1883);
            MQTTclient.setCallback(callback);
        }
    #endif

    operationMode = readOperationModeFromNVS();
    wifiEnabled = getWifiEnableStatusFromNVS();

    #ifdef BLUETOOTH_ENABLE
    if(operationMode == OPMODE_BLUETOOTH) {
        a2dp_sink = new BluetoothA2DPSink();
        i2s_pin_config_t pin_config = {
            .bck_io_num = I2S_BCLK,
            .ws_io_num = I2S_LRC,
            .data_out_num = I2S_DOUT,
            .data_in_num = I2S_PIN_NO_CHANGE
        };
        a2dp_sink->set_pin_config(pin_config);
        a2dp_sink->start((char *) FPSTR(nameBluetoothDevice));
    } else {
        esp_bt_mem_release(ESP_BT_MODE_BTDM);
    #endif
        xTaskCreatePinnedToCore(
            playAudio, /* Function to implement the task */
            "mp3play", /* Name of the task */
            11000,  /* Stack size in words */
            NULL,  /* Task input parameter */
            2 | portPRIVILEGE_BIT,  /* Priority of the task */
            &mp3Play,  /* Task handle. */
            1 /* Core where the task should run */
        );
        wifiManager();
    #ifdef BLUETOOTH_ENABLE
    }
    #endif

    lastTimeActiveTimestamp = millis();     // initial set after boot

    bootComplete = true;

    snprintf(logBuf, serialLoglength, "%s: %u", (char *) FPSTR(freeHeapAfterSetup), ESP.getFreeHeap());
    loggerNl(serialDebug, logBuf, LOGLEVEL_DEBUG);
    Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
}

#ifdef BLUETOOTH_ENABLE
void bluetoothHandler(void) {
    esp_a2d_audio_state_t state = a2dp_sink->get_audio_state();
    // Reset Sleep Timer when audio is playing
    if(state == ESP_A2D_AUDIO_STATE_STARTED) {
        lastTimeActiveTimestamp = millis();
    }
}
#endif

void loop() {

    #ifdef BLUETOOTH_ENABLE
    if(operationMode == OPMODE_BLUETOOTH) {
        bluetoothHandler();
    } else {
    #endif
        webserverStart();
        #ifdef FTP_ENABLE
            ftpManager();
        #endif
        volumeHandler(minVolume, maxVolume);
        if (wifiManager() == WL_CONNECTED) {
            #ifdef MQTT_ENABLE
                if (enableMqtt) {
                    reconnect();
                    MQTTclient.loop();
                    postHeartbeatViaMqtt();
                }
            #endif
            #ifdef FTP_ENABLE
                if (ftpEnableLastStatus && ftpEnableCurrentStatus) {
                    ftpSrv->handleFTP();
                }
            #endif
        }
        #ifdef FTP_ENABLE
            if (ftpEnableLastStatus && ftpEnableCurrentStatus) {
                if (ftpSrv->isConnected()) {
                    lastTimeActiveTimestamp = millis();     // Re-adjust timer while client is connected to avoid ESP falling asleep
                }
            }
        #endif
        ws.cleanupClients();
    #ifdef BLUETOOTH_ENABLE
    }
    #endif


    #ifdef HEADPHONE_ADJUST_ENABLE
        headphoneVolumeManager();
    #endif
    #ifdef MEASURE_BATTERY_VOLTAGE
        batteryVoltageTester();
    #endif
    buttonHandler();
    doButtonActions();
    sleepHandler();
    deepSleepManager();
    rfidPreferenceLookupHandler();

    #ifdef PLAY_LAST_RFID_AFTER_REBOOT
        recoverLastRfidPlayed();
    #endif

}


// Some mp3-lib-stuff (slightly changed from default)
void audio_info(const char *info) {
    snprintf(logBuf, serialLoglength, "info        : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
}
void audio_id3data(const char *info) {  //id3 metadata
    snprintf(logBuf, serialLoglength, "id3data     : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
}
void audio_eof_mp3(const char *info) {  //end of file
    snprintf(logBuf, serialLoglength, "eof_mp3     : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
    playProperties.trackFinished = true;
}
void audio_showstation(const char *info) {
    snprintf(logBuf, serialLoglength, "station     : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_NOTICE);
    char buf[255];
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "Webradio: %s", info);
    #ifdef MQTT_ENABLE
        publishMqtt((char *) FPSTR(topicTrackState), buf, false);
    #endif
}
void audio_showstreamtitle(const char *info) {
    snprintf(logBuf, serialLoglength, "streamtitle : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
}
void audio_bitrate(const char *info) {
    snprintf(logBuf, serialLoglength, "bitrate     : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
}
void audio_commercial(const char *info) {  //duration in sec
    snprintf(logBuf, serialLoglength, "commercial  : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
}
void audio_icyurl(const char *info) {  //homepage
    snprintf(logBuf, serialLoglength, "icyurl      : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
}
void audio_lasthost(const char *info) {  //stream URL played
    snprintf(logBuf, serialLoglength, "lasthost    : %s", info);
    loggerNl(serialDebug, logBuf, LOGLEVEL_INFO);
}