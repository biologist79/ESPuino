// Define modules to compile:
#define MQTT_ENABLE
#define FTP_ENABLE
#define NEOPIXEL_ENABLE             // Don't forget configuration of NUM_LEDS
#define NEOPIXEL_REVERSE_ROTATION   // Some Neopixels are adressed/soldered counter-clockwise. This can be configured here.
#define LANGUAGE 1                  // 1 = deutsch; 2 = english

#include <ESP32Encoder.h>
#include "Arduino.h"
#include <WiFi.h>
#ifdef FTP_ENABLE
    #include "ESP32FtpServer.h"
#endif
#include "Audio.h"
#include "AC101.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "esp_task_wdt.h"
#include <MFRC522.h>                // https://github.com/madias123/STM32duino-RFID-Mp3-Player/tree/master/3d%20party%20libraries%20(modified)/MFRC522
#include <Preferences.h>
#ifdef MQTT_ENABLE
    #include <PubSubClient.h>
#endif
#include <WebServer.h>
#ifdef NEOPIXEL_ENABLE
    #include <FastLED.h>
#endif

#if (LANGUAGE == 1)
    #include "logmessages.h"
    #include "websiteMgmt.h"
    #include "websiteBasic.h"
#endif
#if (LANGUAGE == 2)
    #include "logmessages_EN.h"
    #include "websiteMgmt_EN.h"
    #include "websiteBasic_EN.h"
#endif

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <nvsDump.h>



// Info-docs:
// https://docs.aws.amazon.com/de_de/freertos-kernel/latest/dg/queue-management.html
// https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html#how-do-i-declare-a-global-flash-string-and-use-it

// Loglevels
#define LOGLEVEL_ERROR                  1           // only errors
#define LOGLEVEL_NOTICE                 2           // errors + important messages
#define LOGLEVEL_INFO                   3           // infos + errors + important messages
#define LOGLEVEL_DEBUG                  4           // almost everything

// Serial-logging-configuration
const uint8_t serialDebug = LOGLEVEL_INFO;          // Current loglevel for serial console

// Serial-logging buffer
char logBuf[160];                                   // Buffer for all log-messages

// GPIOs (uSD card-reader)
#define SD_CS                           13          // internal
#define SPI_MOSI                        15          // JT_MTDO
#define SPI_MISO                         2          // Solder to pin7 of SD
#define SPI_SCK                         14          // JT_MTMS

// GPIOs (RFID-readercurrentRfidTagId)
#define MFRC522_RST_PIN                 35          // 14-pin-header
#define MFRC522_CS_PIN                  12          // JT_MTDI
extern SPIClass SPI_MFRC;
MFRC522 mfrc522(MFRC522_CS_PIN, MFRC522_RST_PIN);

// DAC (internal)
#define I2S_DSIN                        25          // internal
#define I2S_BCLK                        27          // internal
#define I2S_LRC                         26          // internal
#define I2S_MCLK                         0          // internal
#define I2S_DOUT                        35          // internal

// I2C GPIOs
#define IIC_CLK                         32          // internal
#define IIC_DATA                        33          // internal

// GPIO used to trigger transistor-circuit / RFID-reader
#define POWER                           19

// GPIOs (Rotary encoder)
#define DREHENCODER_CLK                 5
#define DREHENCODER_DT                  18
#define DREHENCODER_BUTTON              4           // Solder to pin8 of SD

// GPIOs (Control-buttons)                          // Currently deactivated; please read README.md
/*#define PAUSEPLAY_BUTTON                5
#define NEXT_BUTTON                     4
#define PREVIOUS_BUTTON                 33*/

// Amp enable
#define GPIO_PA_EN                      21          // internal

// Headphone?
#define HEADPHONE_PLUGGED_IN            39          // internal

// GPIOs (LEDs)
#define LED_PIN                         23

// Neopixel-configuration
#ifdef NEOPIXEL_ENABLE
    #define NUM_LEDS                    24          // number of LEDs
    #define CHIPSET                     WS2812B     // type of Neopixel
    #define COLOR_ORDER                 GRB
#endif

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

// Repeat-Modes
#define NO_REPEAT                       0
#define TRACK                           1
#define PLAYLIST                        2
#define TRACK_N_PLAYLIST                3

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

// Configuration of initial values (for the first start) goes here....
// There's no need to change them here as they can be configured via webinterface
// Neopixel
uint8_t initialLedBrightness = 16;                      // Initial brightness of Neopixel
uint8_t ledBrightness = initialLedBrightness;
uint8_t nightLedBrightness = 2;                         // Brightness of Neopixel in nightmode

// MQTT
bool enableMqtt = true;
#ifdef MQTT_ENABLE
    uint8_t mqttFailCount = 3;                              // Number of times mqtt-reconnect is allowed to fail. If >= mqttFailCount to further reconnects take place
    uint8_t const stillOnlineInterval = 60;                 // Interval 'I'm still alive' is sent via MQTT (in seconds)
#endif
// RFID
#define RFID_SCAN_INTERVAL 300                         //in ms
uint8_t const cardIdSize = 4;                           // RFID
// Volume
uint8_t maxVolume = 21;                                 // Maximum volume that can be adjusted
uint8_t minVolume = 0;                                  // Lowest volume that can be adjusted
uint8_t initVolume = 3;                                 // 0...21 (If not found in NVS, this one will be taken)
// Sleep
uint8_t maxInactivityTime = 10;                         // Time in minutes, after uC is put to deep sleep because of inactivity
uint8_t sleepTimer = 30;                                // Sleep timer in minutes that can be optionally used (and modified later via MQTT or RFID)
// FTP
char ftpUser[10] = "esp32";                             // FTP-user
char ftpPassword[15] = "esp32";                         // FTP-password


// Button-configuration (change according your needs)
uint8_t buttonDebounceInterval = 50;                    // Interval in ms to software-debounce buttons
uint16_t intervalToLongPress = 700;                     // Interval in ms to distinguish between short and long press of previous/next-button

// Where to store the backup-file
static const char backupFile[] PROGMEM = "/backup.txt"; // File is written every time a (new) RFID-assignment via GUI is done

// Don't change anything here unless you know what you're doing
// HELPER //
// WiFi
unsigned long wifiCheckLastTimestamp = 0;
// Neopixel
#ifdef NEOPIXEL_ENABLE
    bool showLedError = false;
    bool showLedOk = false;
    bool showPlaylistProgress = false;
    bool showRewind = false;
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
bool lockControls = false;                              // Flag if buttons and rotary encoder is locked
bool bootComplete = false;
// Rotary encoder-helper
int32_t lastEncoderValue;
int32_t currentEncoderValue;
int32_t lastVolume = -1;                                // Don't change -1 as initial-value!
uint8_t currentVolume = initVolume;
////////////

// AP-WiFi
static const char accessPointNetworkSSID[] PROGMEM = "Tonuino";     // Access-point's SSID
IPAddress apIP(192, 168, 4, 1);                         // Access-point's static IP
IPAddress apNetmask(255, 255, 255, 0);                  // Access-point's netmask
bool accessPointStarted = false;


// MQTT-configuration
char mqtt_server[16] = "192.168.2.43";                  // IP-address of MQTT-server (if not found in NVS this one will be taken)
char mqttUser[16] = "mqtt-user";                        // MQTT-user
char mqttPassword[16] = "mqtt-password";                // MQTT-password
#ifdef MQTT_ENABLE
    #define DEVICE_HOSTNAME "ESP32-Tonuino"                 // Name that that is used for MQTT
    static const char topicSleepCmnd[] PROGMEM = "Cmnd/Tonuino/Sleep";
    static const char topicSleepState[] PROGMEM = "State/Tonuino/Sleep";
    static const char topicTrackCmnd[] PROGMEM = "Cmnd/Tonuino/Track";
    static const char topicTrackState[] PROGMEM = "State/Tonuino/Track";
    static const char topicTrackControlCmnd[] PROGMEM = "Cmnd/Tonuino/TrackControl";
    static const char topicLoudnessCmnd[] PROGMEM = "Cmnd/Tonuino/Loudness";
    static const char topicLoudnessState[] PROGMEM = "State/Tonuino/Loudness";
    static const char topicSleepTimerCmnd[] PROGMEM = "Cmnd/Tonuino/SleepTimer";
    static const char topicSleepTimerState[] PROGMEM = "State/Tonuino/SleepTimer";
    static const char topicState[] PROGMEM = "State/Tonuino/State";
    static const char topicCurrentIPv4IP[] PROGMEM = "State/Tonuino/IPv4";
    static const char topicLockControlsCmnd[] PROGMEM ="Cmnd/Tonuino/LockControls";
    static const char topicLockControlsState[] PROGMEM ="State/Tonuino/LockControls";
    static const char topicPlaymodeState[] PROGMEM = "State/Tonuino/Playmode";
    static const char topicRepeatModeCmnd[] PROGMEM = "Cmnd/Tonuino/RepeatMode";
    static const char topicRepeatModeState[] PROGMEM = "State/Tonuino/RepeatMode";
    static const char topicLedBrightnessCmnd[] PROGMEM = "Cmnd/Tonuino/LedBrightness";
    static const char topicLedBrightnessState[] PROGMEM = "State/Tonuino/LedBrightness";
#endif

char stringDelimiter[] = "#";                               // Character used to encapsulate data in linear NVS-strings (don't change)
char stringOuterDelimiter[] = "^";                          // Character used to encapsulate encapsulated data along with RFID-ID in backup-file


void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}
AsyncWebServer wServer(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

static const char backupRecoveryWebsite[] PROGMEM = "<p>Das Backup-File wird eingespielt...<br />Zur letzten Seite <a href=\"javascript:history.back()\">zur&uuml;ckkehren</a>.</p>";
static const char restartWebsite[] PROGMEM = "<p>Der Tonuino wird neu gestartet...<br />Zur letzten Seite <a href=\"javascript:history.back()\">zur&uuml;ckkehren</a>.</p>";


// Audio/mp3
SPIClass spiSD(HSPI);
TaskHandle_t mp3Play;
TaskHandle_t rfid;
#ifdef NEOPIXEL_ENABLE
    TaskHandle_t LED;
#endif

// FTP
#ifdef FTP_ENABLE
    FtpServer ftpSrv;
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


// Prototypes
void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask);
static int arrSortHelper(const void* a, const void* b);
#ifdef MQTT_ENABLE
    void callback(const char *topic, const byte *payload, uint32_t length);
#endif
void buttonHandler();
void deepSleepManager(void);
void doButtonActions(void);
void doRfidCardModifications(const uint32_t mod);
bool dumpNvsToSd(char *_namespace, char *_destFile);
bool endsWith (const char *str, const char *suf);
bool fileValid(const char *_fileItem);
void freeMultiCharArray(char **arr, const uint32_t cnt);
uint8_t getRepeatMode(void);
bool isNumber(const char *str);
void loggerNl(const char *str, const uint8_t logLevel);
void logger(const char *str, const uint8_t logLevel);
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
void trackQueueDispatcher(const char *_sdFile, const uint32_t _lastPlayPos, const uint32_t _playMode, const uint16_t _trackLastPlayed);
void volumeHandler(const int32_t _minVolume, const int32_t _maxVolume);
void volumeToQueueSender(const int32_t _newVolume);
wl_status_t wifiManager(void);


/* Wrapper-Funktion for Serial-logging (with newline) */
void loggerNl(const char *str, const uint8_t logLevel) {
  if (serialDebug >= logLevel) {
    Serial.println(str);
  }
}

/* Wrapper-Funktion for Serial-Logging (without newline) */
void logger(const char *str, const uint8_t logLevel) {
  if (serialDebug >= logLevel) {
    Serial.print(str);
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


// Used to print content of sd-card (currently not used, maybe later :-))
/*void printSdContent(File dir, uint16_t allocSize, uint8_t allocCount, char *sdContent, uint8_t depth) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            dir.rewindDirectory();
            break;
        }

        if (countChars(entry.name(), '/') > depth+1) {
            continue;
        }

        Serial.println(entry.name());

        if ((strlen(sdContent) + strlen(entry.name()) + 2) >= allocCount * allocSize) {
            sdContent = (char*) realloc(sdContent, ++allocCount * allocSize);
            Serial.printf("Free heap: %u", ESP.getFreeHeap());
            Serial.printf("realloc! -%d-\n", allocCount);
            if (sdContent == NULL) {
                return;
            }
        }
        strcat(sdContent, stringDelimiter);
        strcat(sdContent, entry.name());

        if (entry.isDirectory()) {
            printSdContent(entry, allocSize, allocCount, sdContent, depth);
        }
        entry.close();
    }
}*/

void IRAM_ATTR onTimer() {
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}


// If timer-semaphore is set, read buttons (unless controls are locked)
void buttonHandler() {
    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
        if (lockControls) {
            return;
        }
        unsigned long currentTimestamp = millis();
        //buttons[0].currentState = digitalRead(NEXT_BUTTON);
        //buttons[1].currentState = digitalRead(PREVIOUS_BUTTON);
        //buttons[2].currentState = digitalRead(PAUSEPLAY_BUTTON);
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
                        //gotoSleep = true;
                        break;
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


/* Cyclic posting via MQTT that ESP is still alive  */
void postHeartbeatViaMqtt(void) {
    if (millis() - lastOnlineTimestamp >= stillOnlineInterval*1000) {
        lastOnlineTimestamp = millis();
        if (publishMqtt((char *) FPSTR(topicState), "Online", false)) {
            loggerNl((char *) FPSTR(stillOnlineMqtt), LOGLEVEL_DEBUG);
        }
    }
}


/* Connects/reconnects to MQTT-Broker unless connection is not already available.
    Manages MQTT-subscriptions.
*/
bool reconnect() {
  uint8_t maxRetries = 10;
  uint8_t connect = false;

  while (!MQTTclient.connected() && mqttFailCount < maxRetries) {
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s %s", (char *) FPSTR(tryConnectMqttS), mqtt_server);
    loggerNl(logBuf, LOGLEVEL_NOTICE);

    // Try to connect to MQTT-server. If username AND password are set, they'll be used
    if (strlen(mqttUser) < 1 || strlen(mqttPassword) < 1) {
        loggerNl((char *) FPSTR(mqttWithoutPwd), LOGLEVEL_NOTICE);
        if (MQTTclient.connect(DEVICE_HOSTNAME)) {
            connect = true;
        }
    } else {
        loggerNl((char *) FPSTR(mqttWithPwd), LOGLEVEL_NOTICE);
        if (MQTTclient.connect(DEVICE_HOSTNAME, mqttUser, mqttPassword)) {
            connect = true;
        }
    }
    if (connect) {
        loggerNl((char *) FPSTR(mqttOk), LOGLEVEL_NOTICE);

        // Deepsleep-subscription
        MQTTclient.subscribe((char *) FPSTR(topicSleepCmnd));

        // Trackname-subscription
        MQTTclient.subscribe((char *) FPSTR(topicTrackCmnd));

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
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: rc=%i (%d / %d)", (char *) FPSTR(mqttConnFailed), MQTTclient.state(), mqttFailCount+1, maxRetries);
        loggerNl(logBuf, LOGLEVEL_ERROR);
        mqttFailCount++;
        delay(500);
    }
  }
  return false;
}


// Is called if there's a new MQTT-message for us
void callback(const char *topic, const byte *payload, uint32_t length) {
    char *receivedString = strndup((char*)payload, length);
    char *mqttTopic = strdup(topic);

    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "MQTT-Nachricht empfangen: [Topic: %s] [Kommando: %s]", mqttTopic, receivedString);
    loggerNl(logBuf, LOGLEVEL_INFO);

    // Go to sleep?
    if (strcmp_P(topic, topicSleepCmnd) == 0) {
        if ((strcmp(receivedString, "OFF") == 0) || (strcmp(receivedString, "0") == 0)) {
            gotoSleep = true;
        }
    }

    // New track to play? Take RFID-ID as input
    else if (strcmp_P(topic, topicTrackCmnd) == 0) {
        char *_rfidId = strdup(receivedString);
        xQueueSend(rfidCardQueue, &_rfidId, 0);
        free(_rfidId);
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
            loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_INFO);
            publishMqtt((char *) FPSTR(topicSleepState), 0, false);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
            return;
        }
        if (strcmp(receivedString, "EOP") == 0) {
            playProperties.sleepAfterPlaylist = true;
            loggerNl((char *) FPSTR(sleepTimerEOP), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            return;
        } else if (strcmp(receivedString, "EOT") == 0) {
            playProperties.sleepAfterCurrentTrack = true;
            loggerNl((char *) FPSTR(sleepTimerEOT), LOGLEVEL_NOTICE);
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
            loggerNl((char *) FPSTR(sleepTimerEO5), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif
            return;
        } else if (strcmp(receivedString, "0") == 0) {
            if (sleepTimerStartTimestamp) {
                sleepTimerStartTimestamp = 0;
                loggerNl((char *) FPSTR(sleepTimerStop), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
                publishMqtt((char *) FPSTR(topicSleepState), 0, false);
                return;
            } else {
                loggerNl((char *) FPSTR(sleepTimerAlreadyStopped), LOGLEVEL_INFO);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                return;
            }
        }
        sleepTimer = strtoul(receivedString, NULL, 10);
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u Minute(n)", (char *) FPSTR(sleepTimerSetTo), sleepTimer);
        loggerNl(logBuf, LOGLEVEL_NOTICE);
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
            loggerNl((char *) FPSTR(allowButtons), LOGLEVEL_NOTICE);
            #ifdef NEOPIXEL_ENABLE
                showLedOk = true;
            #endif

        } else if (strcmp(receivedString, "ON") == 0) {
            lockControls = true;
            loggerNl((char *) FPSTR(lockButtons), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(noPlaylistNotAllowedMqtt), LOGLEVEL_ERROR);
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
                        loggerNl((char *) FPSTR(modeRepeatNone), LOGLEVEL_INFO);
                        #ifdef NEOPIXEL_ENABLE
                            showLedOk = true;
                        #endif
                        break;

                    case TRACK:
                        playProperties.repeatCurrentTrack = true;
                        playProperties.repeatPlaylist = false;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl((char *) FPSTR(modeRepeatTrack), LOGLEVEL_INFO);
                        #ifdef NEOPIXEL_ENABLE
                            showLedOk = true;
                        #endif
                        break;

                    case PLAYLIST:
                        playProperties.repeatCurrentTrack = false;
                        playProperties.repeatPlaylist = true;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl((char *) FPSTR(modeRepeatPlaylist), LOGLEVEL_INFO);
                        #ifdef NEOPIXEL_ENABLE
                            showLedOk = true;
                        #endif
                        break;

                    case TRACK_N_PLAYLIST:
                        playProperties.repeatCurrentTrack = true;
                        playProperties.repeatPlaylist = true;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl((char *) FPSTR(modeRepeatTracknPlaylist), LOGLEVEL_INFO);
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
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(noValidTopic), topic);
        loggerNl(logBuf, LOGLEVEL_ERROR);
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
        /*snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(freePtr), *(arr+i));
        loggerNl(logBuf, LOGLEVEL_DEBUG);*/
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

    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(freeMemory), ESP.getFreeHeap());
    loggerNl(logBuf, LOGLEVEL_DEBUG);

    if (files != NULL) {        // If **ptr already exists, de-allocate its memory
        loggerNl((char *) FPSTR(releaseMemoryOfOldPlaylist), LOGLEVEL_DEBUG);
        --files;
        freeMultiCharArray(files, strtoul(*files, NULL, 10));
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(freeMemoryAfterFree), ESP.getFreeHeap());
        loggerNl(logBuf, LOGLEVEL_DEBUG);
    }

    if (!_fileOrDirectory) {
        loggerNl((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
        return NULL;
    }

    // File-mode
    if (!_fileOrDirectory.isDirectory()) {
        files = (char **) malloc(sizeof(char *) * 2);        // +1 because [0] is used for number of elements; [1] -> [n] is used for payload
        if (files == NULL) {
            loggerNl((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
            return NULL;
        }
        loggerNl((char *) FPSTR(fileModeDetected), LOGLEVEL_INFO);
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
                /*snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(nameOfFileFound), fileNameBuf);
                loggerNl(logBuf, LOGLEVEL_INFO);*/
                if ((strlen(serializedPlaylist) + strlen(fileNameBuf) + 2) >= allocCount * allocSize) {
                    serializedPlaylist = (char*) realloc(serializedPlaylist, ++allocCount * allocSize);
                    loggerNl((char *) FPSTR(reallocCalled), LOGLEVEL_DEBUG);
                    if (serializedPlaylist == NULL) {
                        loggerNl((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
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
        loggerNl((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
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
        loggerNl((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
        #ifdef NEOPIXEL_ENABLE
            showLedError = true;
        #endif
        return NULL;
    }
    sprintf(files[0], "%u", cnt);
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(numberOfValidFiles), cnt);
    loggerNl(logBuf, LOGLEVEL_NOTICE);

    return ++files;         // return ptr+1 (starting at 1st payload-item)
}


/* Wraps putString for writing settings into NVS for RFID-cards.
   Returns number of characters written. */
size_t nvsRfidWriteWrapper (const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks) {
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
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "Schreibe '%s' in NVS für RFID-Card-ID %s mit playmode %d und letzter Track %u\n", prefBuf, _rfidCardId, _playMode, _trackLastPlayed);
    logger(logBuf, LOGLEVEL_INFO);
    loggerNl(prefBuf, LOGLEVEL_INFO);
    return prefsRfid.putString(_rfidCardId, prefBuf);
}


// Function to play music as task
void playAudio(void *parameter) {
    static Audio audio;
    static AC101 ac;
    static bool currentHeadphoneState = digitalRead(HEADPHONE_PLUGGED_IN);
    static bool lastHeadphoneState = currentHeadphoneState;
    static uint32_t lastHeadphoneStateTimestamp = 0;

    while (!ac.begin(IIC_DATA, IIC_CLK)) {
        Serial.printf("Failed!\n");
        delay(1000);
    }

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DSIN);
    audio.setVolume(initVolume);

    pinMode(GPIO_PA_EN, OUTPUT);
    digitalWrite(GPIO_PA_EN, HIGH);

    uint8_t currentVolume;
    static BaseType_t trackQStatus;
    static uint8_t trackCommand = 0;

    for (;;) {
        // GPIO is 0 if headphone is plugged in. So turn of speaker if so and vice versa.
        currentHeadphoneState = digitalRead(HEADPHONE_PLUGGED_IN);
        if (((lastHeadphoneState != currentHeadphoneState) && (millis() - lastHeadphoneStateTimestamp >= 1000)) || !lastHeadphoneStateTimestamp) {
            if (!currentHeadphoneState) {
                ac.SetVolumeHeadphone(100);
                ac.SetVolumeSpeaker(0);
            } else {
                ac.SetVolumeHeadphone(0);
                ac.SetVolumeSpeaker(50);
            }
            lastHeadphoneState = currentHeadphoneState;
            lastHeadphoneStateTimestamp = millis();
        }

        if (xQueueReceive(volumeQueue, &currentVolume, 0) == pdPASS ) {
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(newLoudnessReceivedQueue), currentVolume);
            loggerNl(logBuf, LOGLEVEL_INFO);
            audio.setVolume(currentVolume);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicLoudnessState), currentVolume, false);
            #endif
        }

        if (xQueueReceive(trackControlQueue, &trackCommand, 0) == pdPASS) {
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(newCntrlReceivedQueue), trackCommand);
            loggerNl(logBuf, LOGLEVEL_INFO);
        }

        trackQStatus = xQueueReceive(trackQueue, &playProperties.playlist, 0);
        if (trackQStatus == pdPASS || playProperties.trackFinished || trackCommand != 0) {
            if (trackQStatus == pdPASS) {
                if (playProperties.pausePlay) {
                    playProperties.pausePlay = !playProperties.pausePlay;
                }
                audio.stopSong();
                snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s mit %d Titel(n)", (char *) FPSTR(newPlaylistReceived), playProperties.numberOfTracks);
                loggerNl(logBuf, LOGLEVEL_NOTICE);

                // If we're in audiobook-mode and apply a modification-card, we don't
                // want to save lastPlayPosition for the mod-card but for the card that holds the playlist
                strncpy(playProperties.playRfidTag, currentRfidTagId, sizeof(playProperties.playRfidTag) / sizeof(playProperties.playRfidTag[0]));
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
                    loggerNl((char *) FPSTR(repeatTrackDueToPlaymode), LOGLEVEL_INFO);
                    #ifdef NEOPIXEL_ENABLE
                        showRewind = true;
                    #endif
                }
            }

            if (playProperties.playlistFinished && trackCommand != 0) {
                loggerNl((char *) FPSTR(noPlaymodeChangeIfIdle), LOGLEVEL_NOTICE);
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
                    loggerNl((char *) FPSTR(cmndStop), LOGLEVEL_INFO);
                    playProperties.pausePlay = true;
                    continue;

                case PAUSEPLAY:
                    audio.pauseResume();
                    trackCommand = 0;
                    loggerNl((char *) FPSTR(cmndPause), LOGLEVEL_INFO);
                    if (playProperties.saveLastPlayPosition && !playProperties.pausePlay) {
                        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "Titel wurde bei Position %u pausiert.", audio.getFilePos());
                        loggerNl(logBuf, LOGLEVEL_INFO);
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
                            loggerNl((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }
                        loggerNl((char *) FPSTR(cmndNextTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        loggerNl((char *) FPSTR(lastTrackAlreadyActive), LOGLEVEL_NOTICE);
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
                        playProperties.currentTrackNumber--;
                        if (playProperties.saveLastPlayPosition) {
                            nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                            loggerNl((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }

                        loggerNl((char *) FPSTR(cmndPrevTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        if (playProperties.playMode == WEBSTREAM) {
                            loggerNl((char *) FPSTR(trackChangeWebstream), LOGLEVEL_INFO);
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
                            audio.connecttoSD(*(playProperties.playlist + playProperties.currentTrackNumber));
                            loggerNl((char *) FPSTR(trackStart), LOGLEVEL_INFO);
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
                            loggerNl((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }
                        loggerNl((char *) FPSTR(cmndFirstTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        loggerNl((char *) FPSTR(firstTrackAlreadyActive), LOGLEVEL_NOTICE);
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
                            loggerNl((char *) FPSTR(trackStartAudiobook), LOGLEVEL_INFO);
                        }
                        loggerNl((char *) FPSTR(cmndLastTrack), LOGLEVEL_INFO);
                        if (!playProperties.playlistFinished) {
                            audio.stopSong();
                        }
                    } else {
                        loggerNl((char *) FPSTR(lastTrackAlreadyActive), LOGLEVEL_NOTICE);
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
                    loggerNl((char *) FPSTR(cmndDoesNotExist), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(endOfPlaylistReached), LOGLEVEL_NOTICE);
                if (!playProperties.repeatPlaylist) {
                    if (playProperties.saveLastPlayPosition) {
                        // Set back to first track
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + 0), 0, playProperties.playMode, 0, playProperties.numberOfTracks);
                    }
                    #ifdef MQTT_ENABLE
                        publishMqtt((char *) FPSTR(topicTrackState), "<Ende>", false);
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
                    loggerNl((char *) FPSTR(repeatPlaylistDueToPlaymode), LOGLEVEL_NOTICE);
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
                if (!SD.exists(*(playProperties.playlist + playProperties.currentTrackNumber))) {                        // Check first if file/folder exists
                    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "Datei/Ordner '%s' existiert nicht", *(playProperties.playlist + playProperties.currentTrackNumber));
                    loggerNl(logBuf, LOGLEVEL_ERROR);
                    playProperties.trackFinished = true;
                    continue;
                } else {
                    audio.connecttoSD(*(playProperties.playlist + playProperties.currentTrackNumber));
                    #ifdef NEOPIXEL_ENABLE
                        showPlaylistProgress = true;
                    #endif
                    if (playProperties.startAtFilePos > 0) {
                        audio.setFilePos(playProperties.startAtFilePos);
                        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s %u", (char *) FPSTR(trackStartatPos), audio.getFilePos());
                        loggerNl(logBuf, LOGLEVEL_NOTICE);
                    }
                    char buf[255];
                    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "(%d/%d) %s", (playProperties.currentTrackNumber+1), playProperties.numberOfTracks, (const char*) *(playProperties.playlist + playProperties.currentTrackNumber));
                    #ifdef MQTT_ENABLE
                        publishMqtt((char *) FPSTR(topicTrackState), buf, false);
                    #endif
                    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "'%s' wird abgespielt (%d von %d)", *(playProperties.playlist + playProperties.currentTrackNumber), (playProperties.currentTrackNumber+1) , playProperties.numberOfTracks);
                    loggerNl(logBuf, LOGLEVEL_NOTICE);
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


// Instructs RFID-scanner to scan for new RFID-tags
void rfidScanner(void *parameter) {
    //static MFRC522 mfrc522(MFRC522_CS_PIN, MFRC522_RST_PIN);
    //static MFRC522 mfrc522(RFID_CS, RST_PIN);
    //SPI.begin();
    pinMode(MFRC522_CS_PIN, OUTPUT);
    digitalWrite(MFRC522_CS_PIN, HIGH);
    mfrc522.PCD_Init();
    mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader detail
    delay(4);
    loggerNl((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
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
                logger((char *) FPSTR(unableToAllocateMem), LOGLEVEL_ERROR);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                continue;
            }

            uint8_t n = 0;
            logger((char *) FPSTR(rfidTagDetected), LOGLEVEL_NOTICE);
            for (uint8_t i=0; i<cardIdSize; i++) {
                cardId[i] = mfrc522.uid.uidByte[i];

                snprintf(logBuf, sizeof(logBuf)/sizeof(logBuf[0]), "%02x", cardId[i]);
                logger(logBuf, LOGLEVEL_NOTICE);

                n += snprintf (&cardIdString[n], sizeof(cardIdString) / sizeof(cardIdString[0]), "%03d", cardId[i]);
                if (i<(cardIdSize-1)) {
                    logger("-", LOGLEVEL_NOTICE);
                } else {
                    logger("\n", LOGLEVEL_NOTICE);
                }
            }
            xQueueSend(rfidCardQueue, &cardIdString, 0);
            free(cardIdString);
        }
    }
    vTaskDelete(NULL);
}


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

    static CRGB leds[NUM_LEDS];
    FastLED.addLeds<CHIPSET , LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalSMD5050 );
    FastLED.setBrightness(ledBrightness);

    for (;;) {
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
                    FastLED.clear();
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
                    if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || !buttons[3].currentState) {
                        break;
                    } else {
                        vTaskDelay(portTICK_RATE_MS*30);
                    }
                }

                for (uint8_t i=0; i<=100; i++) {
                   if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || !buttons[3].currentState) {
                        break;
                    } else {
                        vTaskDelay(portTICK_RATE_MS*15);
                    }
                }

                for (uint8_t i=numLedsToLight; i>0; i--) {
                    leds[ledAddress(i)-1] = CRGB::Black;
                    FastLED.show();
                    if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || !buttons[3].currentState) {
                        break;
                    } else {
                        vTaskDelay(portTICK_RATE_MS*30);
                    }
                }
            }
        }

        switch (playProperties.playMode) {
            case NO_PLAYLIST:                   // If no playlist is active (idle)
                if (hlastVolume == currentVolume && lastLedBrightness == ledBrightness) {
                    for (uint8_t i=0; i<NUM_LEDS; i++) {
                        FastLED.clear();
                        if (ledAddress(i) == 0) {
                            leds[0] = CRGB::White;
                            leds[NUM_LEDS/4] = CRGB::White;
                            leds[NUM_LEDS/2] = CRGB::White;
                            leds[NUM_LEDS/4*3] = CRGB::White;
                        } else {
                            leds[ledAddress(i) % NUM_LEDS] = CRGB::White;
                            leds[(ledAddress(i)+NUM_LEDS/4) % NUM_LEDS] = CRGB::White;
                            leds[(ledAddress(i)+NUM_LEDS/2) % NUM_LEDS] = CRGB::White;
                            leds[(ledAddress(i)+NUM_LEDS/4*3) % NUM_LEDS] = CRGB::White;
                        }
                        FastLED.show();
                        for (uint8_t i=0; i<=50; i++) {
                            if (hlastVolume != currentVolume || lastLedBrightness != ledBrightness || showLedError || showLedOk || playProperties.playMode != NO_PLAYLIST || !buttons[3].currentState) {
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
                    if (playProperties.pausePlay != lastPlayState || lockControls != lastLockState || notificationShown || ledBusyShown || volumeChangeShown || !buttons[3].currentState) {
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
                                } else if (playProperties.pausePlay) {
                                    leds[ledAddress(led) % NUM_LEDS] = CRGB::Orange;
                                    leds[(ledAddress(led)+NUM_LEDS/4) % NUM_LEDS] = CRGB::Orange;
                                    leds[(ledAddress(led)+NUM_LEDS/2) % NUM_LEDS] = CRGB::Orange;
                                    leds[(ledAddress(led)+NUM_LEDS/4*3) % NUM_LEDS] = CRGB::Orange;
                                    break;
                                }
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
        esp_task_wdt_reset();
    }
    vTaskDelete(NULL);
}
#endif


// Sets deep-sleep-flag if max. inactivity-time is reached
void sleepHandler(void) {
    unsigned long m = millis();
    if (m >= lastTimeActiveTimestamp && (m - lastTimeActiveTimestamp >= maxInactivityTime * 1000 * 60)) {
        loggerNl((char *) FPSTR(goToSleepDueToIdle), LOGLEVEL_INFO);
        gotoSleep = true;
    } else if (sleepTimerStartTimestamp > 0) {
        if (m - sleepTimerStartTimestamp >= sleepTimer * 1000 * 60) {
            loggerNl((char *) FPSTR(goToSleepDueToTimer), LOGLEVEL_INFO);
            gotoSleep = true;
        }
    }
}


// Puts uC to deep-sleep if flag is set
void deepSleepManager(void) {
    if (gotoSleep) {
        loggerNl((char *) FPSTR(goToSleepNow), LOGLEVEL_NOTICE);
        Serial.flush();
        #ifdef MQTT_ENABLE
            publishMqtt((char *) FPSTR(topicState), "Offline", false);
            publishMqtt((char *) FPSTR(topicTrackState), "---", false);
            MQTTclient.disconnect();
        #endif
        #ifdef NEOPIXEL_ENABLE
            FastLED.clear();
            FastLED.show();
        #endif
        /*SPI.end();
        spiSD.end();*/
        digitalWrite(POWER, LOW);
        digitalWrite(GPIO_PA_EN, LOW);
        delay(200);
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
            loggerNl((char *) FPSTR(maxLoudnessReached), LOGLEVEL_INFO);
            currentEncoderValue = encoder.getCount();
        } else if (currentEncoderValue < _minVolume) {
            encoder.clearCount();
            encoder.setCount(_minVolume);
            loggerNl((char *) FPSTR(minLoudnessReached), LOGLEVEL_INFO);
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
        musicFiles = returnPlaylistFromSD(SD.open(filename));
    } else {
        musicFiles = returnPlaylistFromWebstream(filename);
    }
    #ifdef MQTT_ENABLE
        publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
    #endif

    if (musicFiles == NULL) {
        loggerNl((char *) FPSTR(errorOccured), LOGLEVEL_ERROR);
        #ifdef NEOPIXEL_ENABLE
            showLedError = true;
        #endif
        playProperties.playMode = NO_PLAYLIST;
        return;
    } else if (!strcmp(*(musicFiles-1), "0")) {
        loggerNl((char *) FPSTR(noMp3FilesInDir), LOGLEVEL_NOTICE);
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

    switch(playProperties.playMode) {
        case SINGLE_TRACK: {
            loggerNl((char *) FPSTR(modeSingleTrack), LOGLEVEL_NOTICE);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case SINGLE_TRACK_LOOP: {
            playProperties.repeatCurrentTrack = true;
            loggerNl((char *) FPSTR(modeSingleTrackLoop), LOGLEVEL_NOTICE);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), TRACK, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case AUDIOBOOK: {   // Tracks need to be alph. sorted!
            playProperties.saveLastPlayPosition = true;
            loggerNl((char *) FPSTR(modeSingleAudiobook), LOGLEVEL_NOTICE);
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
            loggerNl((char *) FPSTR(modeSingleAudiobookLoop), LOGLEVEL_NOTICE);
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            #endif
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_SORTED: {
            snprintf(logBuf, sizeof(logBuf)/sizeof(logBuf[0]), "%s '%s' ", (char *) FPSTR(modeAllTrackAlphSorted), filename);
            loggerNl(logBuf, LOGLEVEL_NOTICE);
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_RANDOM: {
            loggerNl((char *) FPSTR(modeAllTrackRandom), LOGLEVEL_NOTICE);
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
            loggerNl((char *) FPSTR(modeAllTrackAlphSortedLoop), LOGLEVEL_NOTICE);
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
            loggerNl((char *) FPSTR(modeAllTrackRandomLoop), LOGLEVEL_NOTICE);
            randomizePlaylist(musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            #ifdef MQTT_ENABLE
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            #endif
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case WEBSTREAM: {   // This is always just one "track"
            loggerNl((char *) FPSTR(modeWebstream), LOGLEVEL_NOTICE);
            if (wifiManager() == WL_CONNECTED) {
                xQueueSend(trackQueue, &(musicFiles), 0);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                    publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
                #endif
            } else {
                loggerNl((char *) FPSTR(webstreamNotAvailable), LOGLEVEL_ERROR);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                playProperties.playMode = NO_PLAYLIST;
            }
            break;
        }

        default:
            loggerNl((char *) FPSTR(modeDoesNotExist), LOGLEVEL_ERROR);
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
    switch (mod) {
        case LOCK_BUTTONS_MOD:      // Locks/unlocks all buttons
            lockControls = !lockControls;
            if (lockControls) {
                loggerNl((char *) FPSTR(modificatorAllButtonsLocked), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLockControlsState), "ON", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            } else {
                loggerNl((char *) FPSTR(modificatorAllButtonsUnlocked), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLockControlsState), "OFF", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    showLedOk = true;
                #endif
            }
            break;

        case SLEEP_TIMER_MOD_15:    // Puts/undo uC to sleep after 15 minutes
            if (sleepTimer == 15) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                    loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #endif
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 15;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl((char *) FPSTR(modificatorSleepTimer15), LOGLEVEL_NOTICE);
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

        case SLEEP_TIMER_MOD_30:    // Puts/undo uC to sleep after 30 minutes
            if (sleepTimer == 30) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                    loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #endif
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 30;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl((char *) FPSTR(modificatorSleepTimer30), LOGLEVEL_NOTICE);
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

        case SLEEP_TIMER_MOD_60:    // Puts/undo uC to sleep after 60 minutes
            if (sleepTimer == 60) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
                loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 60;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl((char *) FPSTR(modificatorSleepTimer60), LOGLEVEL_NOTICE);
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

        case SLEEP_TIMER_MOD_120:    // Puts/undo uC to sleep after 2 hrs
            if (sleepTimer == 120) {
                sleepTimerStartTimestamp = 0;
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
                loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
                #endif

            } else {
                sleepTimer = 120;
                sleepTimerStartTimestamp = millis();
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl((char *) FPSTR(modificatorSleepTimer120), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
                return;
            }
            if (playProperties.sleepAfterCurrentTrack) {
                loggerNl((char *) FPSTR(modificatorSleepAtEOTd), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = initialLedBrightness;
                #endif
            } else {
                loggerNl((char *) FPSTR(modificatorSleepAtEOT), LOGLEVEL_NOTICE);
                #ifdef MQTT_ENABLE
                    publishMqtt((char *) FPSTR(topicSleepTimerState), "EOT", false);
                #endif
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
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
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(modificatorSleepAtEOPd), LOGLEVEL_NOTICE);
            } else {
                #ifdef NEOPIXEL_ENABLE
                    ledBrightness = nightLedBrightness;
                    loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                #endif
                loggerNl((char *) FPSTR(modificatorSleepAtEOP), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(sleepTimerEO5), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
            } else {
                if (playProperties.repeatPlaylist) {
                    loggerNl((char *) FPSTR(modificatorPlaylistLoopDeactive), LOGLEVEL_NOTICE);
                } else {
                    loggerNl((char *) FPSTR(modificatorPlaylistLoopActive), LOGLEVEL_NOTICE);
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
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                #ifdef NEOPIXEL_ENABLE
                    showLedError = true;
                #endif
            } else {
                if (playProperties.repeatCurrentTrack) {
                    loggerNl((char *) FPSTR(modificatorTrackDeactive), LOGLEVEL_NOTICE);
                } else {
                    loggerNl((char *) FPSTR(modificatorTrackActive), LOGLEVEL_NOTICE);
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
            loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
            #ifdef NEOPIXEL_ENABLE
                ledBrightness = nightLedBrightness;
                showLedOk = true;
            #endif
            break;

        default:
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s %d !", (char *) FPSTR(modificatorDoesNotExist), mod);
            loggerNl(logBuf, LOGLEVEL_ERROR);
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
        snprintf(logBuf, sizeof(logBuf)/sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(rfidTagReceived), rfidTagId);
        currentRfidTagId = strdup(rfidTagId);
        sendWebsocketData(0, 10);       // Push new rfidTagId to all websocket-clients
        loggerNl(logBuf, LOGLEVEL_INFO);

        String s = prefsRfid.getString(rfidTagId, "-1");                 // Try to lookup rfidId in NVS
        if (!s.compareTo("-1")) {
            loggerNl((char *) FPSTR(rfidTagUnknownInNvs), LOGLEVEL_ERROR);
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
            loggerNl((char *) FPSTR(errorOccuredNvs), LOGLEVEL_ERROR);
            #ifdef NEOPIXEL_ENABLE
                showLedError = true;
            #endif
        } else {
        // Only pass file to queue if strtok revealed 3 items
            if (_playMode >= 100) {
                doRfidCardModifications(_playMode);
            } else {
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

    loggerNl((char *) FPSTR(apReady), LOGLEVEL_NOTICE);
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "IP-Adresse: %d.%d.%d.%d", apIP[0],apIP[1], apIP[2], apIP[3]);
    loggerNl(logBuf, LOGLEVEL_NOTICE);

    wServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", basicWebsite);
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
        request->send_P(200, "text/html", basicWebsite);
    });

    wServer.on("/restart", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(200, "text/html", "ESP wird neu gestartet...");
        Serial.flush();
        ESP.restart();
    });

    wServer.begin();
    loggerNl((char *) FPSTR(httpReady), LOGLEVEL_NOTICE);
    accessPointStarted = true;
}


// Provides management for WiFi
wl_status_t wifiManager(void) {
    if (wifiCheckLastTimestamp == 0) {
        // Get credentials from NVS
        String strSSID = prefsSettings.getString("SSID", "-1");
        if (!strSSID.compareTo("-1")) {
            loggerNl((char *) FPSTR(ssidNotFoundInNvs), LOGLEVEL_ERROR);
        }
        String strPassword = prefsSettings.getString("Password", "-1");
        if (!strPassword.compareTo("-1")) {
            loggerNl((char *) FPSTR(wifiPwdNotFoundInNvs), LOGLEVEL_ERROR);
        }
        const char *_ssid = strSSID.c_str();
        const char *_pwd = strPassword.c_str();

        /*
        // Get (optional) static-IP-configration from NVS
        String strStaticIp = prefsSettings.getString("staticIP", "-1");
        String strStaticIpGw = prefsSettings.getString("staticIPGw", "-1");
        String strStaticIpNetmask = prefsSettings.getString("staticIPNetmask", "-1");
        if (!strStaticIp.compareTo("-1") || !strStaticIpGw.compareTo("-1") || !strStaticIpNetmask.compareTo("-1")) {
            loggerNl((char *) FPSTR(wifiStaticIpConfigNotFoundInNvs), LOGLEVEL_INFO);
        } else {
            IPAddress staticWifiIp;
            IPAddress staticWifiIpGw;
            IPAddress staticWifiIpNetmask;

            if (strStaticIp.length() >= 7 && strStaticIpGw.length() >= 7 && strStaticIpNetmask.length() >= 7) {
                staticWifiIp.fromString(strStaticIp.c_str());
                staticWifiIpGw.fromString(strStaticIpGw.c_str());
                staticWifiIpNetmask.fromString(strStaticIpNetmask.c_str());
                WiFi.config(staticWifiIp, staticWifiIpGw, staticWifiIpNetmask);
            } else {
                Serial.println("IP-config nicht gueltig!");
            }
        }*/

        // Get (optional) hostname-configration from NVS
        String hostname = prefsSettings.getString("Hostname", "-1");
        if (hostname.compareTo("-1")) {
            WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
            WiFi.setHostname(hostname.c_str());
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(restoredHostnameFromNvs), hostname.c_str());
            loggerNl(logBuf, LOGLEVEL_INFO);
        } else {
            loggerNl((char *) FPSTR(wifiHostnameNotSet), LOGLEVEL_INFO);
        }
        // ...and create a connection with it. If not successful, an access-point is opened
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
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "Aktuelle IP: %d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
            loggerNl(logBuf, LOGLEVEL_NOTICE);
            #ifdef FTP_ENABLE
                ftpSrv.begin(ftpUser, ftpPassword);
            #endif
        } else { // Starts AP if WiFi-connect wasn't successful
            accessPointStart((char *) FPSTR(accessPointNetworkSSID), apIP, apNetmask);
        }
    }

    return WiFi.status();
}


// Used for substitution of some variables/templates of html-files. Is called by webserver's template-engine
String templateProcessor(const String& templ) {
    if (templ == "FTP_USER") {
        return prefsSettings.getString("ftpuser", "-1");
    } else if (templ == "FTP_PWD") {
        return prefsSettings.getString("ftppassword", "-1");
    } else if (templ == "INIT_LED_BRIGHTBESS") {
        return String(prefsSettings.getUChar("iLedBrightness", 0));
    } else if (templ == "NIGHT_LED_BRIGHTBESS") {
        return String(prefsSettings.getUChar("nLedBrightness", 0));
    } else if (templ == "MAX_INACTIVITY") {
        return String(prefsSettings.getUInt("mInactiviyT", 0));
    } else if (templ == "INIT_VOLUME") {
        return String(prefsSettings.getUInt("initVolume", 0));
    } else if (templ == "MAX_VOLUME") {
        return String(prefsSettings.getUInt("maxVolume", 0));
    } else if (templ == "MQTT_SERVER") {
        return prefsSettings.getString("mqttServer", "-1");
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
    } else if (templ == "IPv4") {
        myIP = WiFi.localIP();
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
        return String(logBuf);
    } else if (templ == "RFID_TAG_ID") {
        return String(currentRfidTagId);
    /*} else if (templ == "STATIC_IP") {
        return prefsSettings.getString("staticIP", "-1");
    } else if (templ == "STATIC_IP_GW") {
        return prefsSettings.getString("staticIPGw", "-1");
    } else if (templ == "STATIC_IP_NETMASK") {
        return prefsSettings.getString("staticIPNetmask", "-1");*/
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
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return false;
    }

    if (doc.containsKey("general")) {
        uint8_t iVol = doc["general"]["iVol"].as<uint8_t>();
        uint8_t mVol = doc["general"]["mVol"].as<uint8_t>();
        uint8_t iBright = doc["general"]["iBright"].as<uint8_t>();
        uint8_t nBright = doc["general"]["nBright"].as<uint8_t>();
        uint8_t iTime = doc["general"]["iTime"].as<uint8_t>();

        prefsSettings.putUInt("initVolume", iVol);
        prefsSettings.putUInt("maxVolume", mVol);
        prefsSettings.putUChar("iLedBrightness", iBright);
        prefsSettings.putUChar("nLedBrightness", nBright);
        prefsSettings.putUInt("mInactiviyT", iTime);

        // Check if settings were written successfully
        if (prefsSettings.getUInt("initVolume", 0) != iVol ||
            prefsSettings.getUInt("maxVolume", 0) != mVol ||
            prefsSettings.getUChar("iLedBrightness", 0) != iBright ||
            prefsSettings.getUChar("nLedBrightness", 0) != nBright ||
            prefsSettings.getUInt("mInactiviyT", 0) != iTime) {
            Serial.println("net gut!");
            return false;
        }

    } else if (doc.containsKey("ftp")) {
        const char *_ftpUser = doc["ftp"]["ftpUser"];
        const char *_ftpPwd = doc["ftp"]["ftpPwd"];

        prefsSettings.putString("ftpuser", (String) _ftpUser);
        prefsSettings.putString("ftppassword", (String) _ftpPwd);

        if (!(String(_ftpUser).equals(prefsSettings.getString("ftpuser", "-1")) ||
           String(_ftpPwd).equals(prefsSettings.getString("ftppassword", "-1")))) {
            Serial.println("net gut2!");
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

    /*} else if (doc.containsKey("staticIP")) {
        const char *_staticIp = object["ip"]["staticIP"];
        const char *_staticIpGW = doc["ip"]["staticIPGW"];
        const char *_staticIpNM = doc["ip"]["staticIPNM"];

        prefsSettings.putString("staticIP", (String) _staticIp);
        prefsSettings.putString("staticIPGw", (String) _staticIpGW);
        prefsSettings.putString("staticIPNetmask", (String) _staticIpNM);*/

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
        const char *_fileOrUrl = object["rfidAssign"]["fileOrUrl"];
        uint8_t _playMode = object["rfidAssign"]["playMode"];
        char rfidString[275];
        snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s%s%s0%s%u%s0", stringDelimiter, _fileOrUrl, stringDelimiter, stringDelimiter, _playMode, stringDelimiter);
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
    }

    return true;
}


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
    char jBuf[50];
    serializeJson(doc, jBuf, sizeof(jBuf) / sizeof(jBuf[0]));

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
        Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
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
            uint8_t returnCode;

            if (processJsonRequest((char*)data)) {
                returnCode = 1;
            } else {
                returnCode = 0;
            }
            sendWebsocketData(client->id(), 1);

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


// Dumps all RFID-entries from NVS into a file on SD-card
bool dumpNvsToSd(char *_namespace, char *_destFile) {
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
        Serial.printf("Partition %s not found!", partname) ;
        return NULL;
    }
    namespace_ID = FindNsID (nvs, _namespace) ;             // Find ID of our namespace in NVS

    File backupFile = SD.open(_destFile, FILE_WRITE);
    if (!backupFile) {
        return false;
    }
    while (offset < nvs->size) {
        result = esp_partition_read (nvs, offset,                // Read 1 page in nvs partition
                                    &buf,
                                    sizeof(nvs_page));
        if (result != ESP_OK) {
            Serial.println(F("Error reading NVS!"));
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
    return true;
}


// Handles uploaded backup-file and writes valid entries into NVS
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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
                snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s => %s", (char *) FPSTR(writeEntryToNvs), nvsEntry[0].nvsKey, nvsEntry[0].nvsEntry);
                loggerNl(logBuf, LOGLEVEL_NOTICE);
                prefsRfid.putString(nvsEntry[0].nvsKey, nvsEntry[0].nvsEntry);
            }
        }
    }
}


void setup() {
    Serial.begin(115200);
    srand(esp_random());
    pinMode(POWER, OUTPUT);
    digitalWrite(POWER, HIGH);
    pinMode(HEADPHONE_PLUGGED_IN, INPUT);

    prefsRfid.begin((char *) FPSTR(prefsRfidNamespace));
    prefsSettings.begin((char *) FPSTR(prefsSettingsNamespace));

    SPI_MFRC.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI_MFRC.setFrequency(1000000);

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

    // Init uSD-SPI
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    /*SPI_MFRC.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI_MFRC.setFrequency(1000000);*/
    while (!SD.begin(SD_CS, SPI_MFRC)) {
        loggerNl((char *) FPSTR(unableToMountSd), LOGLEVEL_ERROR);
        delay(500);
    }

    // Create queues
    volumeQueue = xQueueCreate(1, sizeof(int));
    if (volumeQueue == NULL) {
        loggerNl((char *) FPSTR(unableToCreateVolQ), LOGLEVEL_ERROR);
    }

    rfidCardQueue = xQueueCreate(1, (cardIdSize + 1) * sizeof(char));
    if (rfidCardQueue == NULL) {
        loggerNl((char *) FPSTR(unableToCreateRfidQ), LOGLEVEL_ERROR);
    }

    trackControlQueue = xQueueCreate(1, sizeof(uint8_t));
    if (trackControlQueue == NULL) {
        loggerNl((char *) FPSTR(unableToCreateMgmtQ), LOGLEVEL_ERROR);
    }

    char **playlistArray;
    trackQueue = xQueueCreate(1, sizeof(playlistArray));
    if (trackQueue == NULL) {
        loggerNl((char *) FPSTR(unableToCreatePlayQ), LOGLEVEL_ERROR);
    }

    // Get some stuff from NVS...
    // Get initial LED-brightness from NVS
    uint8_t nvsILedBrightness = prefsSettings.getUChar("iLedBrightness", 0);
    if (nvsILedBrightness) {
            initialLedBrightness = nvsILedBrightness;
            ledBrightness = nvsILedBrightness;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(initialBrightnessfromNvs), nvsILedBrightness);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUChar("iLedBrightness", initialLedBrightness);
        loggerNl((char *) FPSTR(wroteInitialBrightnessToNvs), LOGLEVEL_ERROR);
    }

    // Get night LED-brightness from NVS
    uint8_t nvsNLedBrightness = prefsSettings.getUChar("nLedBrightness", 0);
    if (nvsNLedBrightness) {
        nightLedBrightness = nvsNLedBrightness;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(restoredInitialBrightnessForNmFromNvs), nvsNLedBrightness);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUChar("nLedBrightness", nightLedBrightness);
        loggerNl((char *) FPSTR(wroteNmBrightnessToNvs), LOGLEVEL_ERROR);
    }

    // Get FTP-user from NVS
    String nvsFtpUser = prefsSettings.getString("ftpuser", "-1");
    if (!nvsFtpUser.compareTo("-1")) {
        prefsSettings.putString("ftpuser", (String) ftpUser);
        loggerNl((char *) FPSTR(wroteFtpUserToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(ftpUser, nvsFtpUser.c_str(), sizeof(ftpUser)/sizeof(ftpUser[0]));
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(restoredFtpUserFromNvs), nvsFtpUser.c_str());
        loggerNl(logBuf, LOGLEVEL_INFO);
    }

    // Get FTP-password from NVS
    String nvsFtpPassword = prefsSettings.getString("ftppassword", "-1");
    if (!nvsFtpPassword.compareTo("-1")) {
        prefsSettings.putString("ftppassword", (String) ftpPassword);
        loggerNl((char *) FPSTR(wroteFtpPwdToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(ftpPassword, nvsFtpPassword.c_str(), sizeof(ftpPassword)/sizeof(ftpPassword[0]));
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(restoredFtpPwdFromNvs), nvsFtpPassword.c_str());
        loggerNl(logBuf, LOGLEVEL_INFO);
    }

    // Get maximum inactivity-time from NVS
    uint32_t nvsMInactivityTime = prefsSettings.getUInt("mInactiviyT", 0);
    if (nvsMInactivityTime) {
        maxInactivityTime = nvsMInactivityTime;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(restoredMaxInactivityFromNvs), nvsMInactivityTime);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("mInactiviyT", maxInactivityTime);
        loggerNl((char *) FPSTR(wroteMaxInactivityToNvs), LOGLEVEL_ERROR);
    }

    // Get initial volume from NVS
    uint32_t nvsInitialVolume = prefsSettings.getUInt("initVolume", 0);
    if (nvsInitialVolume) {
        initVolume = nvsInitialVolume;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(restoredInitialLoudnessFromNvs), nvsInitialVolume);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("initVolume", initVolume);
        loggerNl((char *) FPSTR(wroteInitialLoudnessToNvs), LOGLEVEL_ERROR);
    }

    // Get maximum volume from NVS
    uint32_t nvsMaxVolume = prefsSettings.getUInt("maxVolume", 0);
    if (nvsMaxVolume) {
        maxVolume = nvsMaxVolume;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(restoredMaxLoudnessFromNvs), nvsMaxVolume);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("maxVolume", maxVolume);
        loggerNl((char *) FPSTR(wroteMaxLoudnessToNvs), LOGLEVEL_ERROR);
    }

    // Get MQTT-enable from NVS
    uint8_t nvsEnableMqtt = prefsSettings.getUChar("enableMQTT", 99);
    switch (nvsEnableMqtt) {
        case 99:
            prefsSettings.putUChar("enableMQTT", enableMqtt);
            loggerNl((char *) FPSTR(wroteMqttFlagToNvs), LOGLEVEL_ERROR);
            break;
        case 1:
            //prefsSettings.putUChar("enableMQTT", enableMqtt);
            enableMqtt = nvsEnableMqtt;
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(restoredMqttActiveFromNvs), nvsEnableMqtt);
            loggerNl(logBuf, LOGLEVEL_INFO);
            break;
        case 0:
            enableMqtt = nvsEnableMqtt;
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(restoredMqttDeactiveFromNvs), nvsEnableMqtt);
            loggerNl(logBuf, LOGLEVEL_INFO);
            break;
    }

    // Get MQTT-server from NVS
    String nvsMqttServer = prefsSettings.getString("mqttServer", "-1");
    if (!nvsMqttServer.compareTo("-1")) {
        prefsSettings.putString("mqttServer", (String) mqtt_server);
        loggerNl((char*) FPSTR(wroteMqttServerToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(mqtt_server, nvsMqttServer.c_str(), sizeof(mqtt_server)/sizeof(mqtt_server[0]));
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(restoredMqttServerFromNvs), nvsMqttServer.c_str());
        loggerNl(logBuf, LOGLEVEL_INFO);
    }

    // Get MQTT-user from NVS
    String nvsMqttUser = prefsSettings.getString("mqttUser", "-1");
    if (!nvsMqttUser.compareTo("-1")) {
        prefsSettings.putString("mqttUser", (String) mqttUser);
        loggerNl((char *) FPSTR(wroteMqttUserToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(mqttUser, nvsMqttUser.c_str(), sizeof(mqttUser)/sizeof(mqttUser[0]));
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(restoredMqttUserFromNvs), nvsMqttUser.c_str());
        loggerNl(logBuf, LOGLEVEL_INFO);
    }

    // Get MQTT-password from NVS
    String nvsMqttPassword = prefsSettings.getString("mqttPassword", "-1");
    if (!nvsMqttPassword.compareTo("-1")) {
        prefsSettings.putString("mqttPassword", (String) mqttPassword);
        loggerNl((char *) FPSTR(wroteMqttPwdToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(mqttPassword, nvsMqttPassword.c_str(), sizeof(mqttPassword)/sizeof(mqttPassword[0]));
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(restoredMqttPwdFromNvs), nvsMqttPassword.c_str());
        loggerNl(logBuf, LOGLEVEL_INFO);
    }

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

    xTaskCreatePinnedToCore(
        playAudio, /* Function to implement the task */
        "mp3play", /* Name of the task */
        11000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        2 | portPRIVILEGE_BIT,  /* Priority of the task */
        &mp3Play,  /* Task handle. */
        1 /* Core where the task should run */
    );


    esp_sleep_enable_ext0_wakeup((gpio_num_t) DREHENCODER_BUTTON, 0);

    // Activate internal pullups for all buttons
    pinMode(DREHENCODER_BUTTON, INPUT_PULLUP);
    /*pinMode(PAUSEPLAY_BUTTON, INPUT_PULLUP);
    pinMode(NEXT_BUTTON, INPUT_PULLUP);
    pinMode(PREVIOUS_BUTTON, INPUT_PULLUP);*/

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

    wifiManager();

    lastTimeActiveTimestamp = millis();     // initial set after boot

    if (wifiManager() == WL_CONNECTED) {
        // attach AsyncWebSocket for Mgmt-Interface
        ws.onEvent(onWebsocketEvent);
        wServer.addHandler(&ws);

        // attach AsyncEventSource
        wServer.addHandler(&events);

        wServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send_P(200, "text/html", mgtWebsite, templateProcessor);
        });

        wServer.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", backupRecoveryWebsite);
        }, handleUpload);

        wServer.on("/restart", HTTP_GET, [] (AsyncWebServerRequest *request) {
            request->send_P(200, "text/html", restartWebsite);
            Serial.flush();
            ESP.restart();
        });

        wServer.onNotFound(notFound);
        wServer.begin();
    }
    bootComplete = true;

    /*char *sdC = (char *) calloc(16384, sizeof(char));
    printSdContent(SD.open("/", FILE_READ), 16384, 1, sdC, 2);
    printSdContent(SD.open("/", FILE_READ), 16384, 1, sdC, 2);
    Serial.println(sdC);
    Serial.println(strlen(sdC));
    Serial.println(ESP.getFreeHeap());
    free (sdC);
    Serial.println(ESP.getFreeHeap());*/
}


void loop() {
    volumeHandler(minVolume, maxVolume);
    buttonHandler();
    doButtonActions();
    sleepHandler();
    deepSleepManager();
    rfidPreferenceLookupHandler();
    if (wifiManager() == WL_CONNECTED) {
        #ifdef MQTT_ENABLE
            if (enableMqtt) {
                reconnect();
                MQTTclient.loop();
                postHeartbeatViaMqtt();
            }
        #endif
        #ifdef FTP_ENABLE
            ftpSrv.handleFTP();
        #endif
    }
    #ifdef FTP_ENABLE
        if (ftpSrv.isConnected()) {
            lastTimeActiveTimestamp = millis();     // Re-adjust timer while client is connected to avoid ESP falling asleep
        }
    #endif
}


// Some mp3-lib-stuff (slightly changed from default)
void audio_info(const char *info) {
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "info        : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}
void audio_id3data(const char *info) {  //id3 metadata
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "id3data     : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}
void audio_eof_mp3(const char *info) {  //end of file
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "eof_mp3     : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
    playProperties.trackFinished = true;
}
void audio_showstation(const char *info) {
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "station     : %s", info);
    loggerNl(logBuf, LOGLEVEL_NOTICE);
    char buf[255];
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "Webradio: %s", info);
    #ifdef MQTT_ENABLE
        publishMqtt((char *) FPSTR(topicTrackState), buf, false);
    #endif
}
void audio_showstreaminfo(const char *info) {
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "streaminfo  : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}
void audio_showstreamtitle(const char *info) {
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "streamtitle : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}
void audio_bitrate(const char *info) {
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "bitrate     : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}
void audio_commercial(const char *info) {  //duration in sec
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "commercial  : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}
void audio_icyurl(const char *info) {  //homepage
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "icyurl      : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}
void audio_lasthost(const char *info) {  //stream URL played
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "lasthost    : %s", info);
    loggerNl(logBuf, LOGLEVEL_INFO);
}