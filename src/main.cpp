#include <ESP32Encoder.h>
#include "Arduino.h"
#include <WiFi.h>
#include "ESP32FtpServer.h"
#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "esp_task_wdt.h"
#include <MFRC522.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <FastLED.h>
#include "logmessages.h"
#include <ESPAsyncWebServer.h>
#include "website.h"

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
#define SPISD_CS                        15
#define SPISD_MOSI                      13
#define SPISD_MISO                      16          // 12 doesn't work with Lolin32-devBoard: uC doesn't start if put HIGH at start
#define SPISD_SCK                       14

// GPIOs (RFID-readercurrentRfidTagId)
#define RST_PIN                         22
#define RFID_CS                         21
#define RFID_MOSI                       23
#define RFID_MISO                       19
#define RFID_SCK                        18

// GPIOs (DAC)
#define I2S_DOUT                        25
#define I2S_BCLK                        27
#define I2S_LRC                         26

// GPIO used to trigger transistor-circuit / RFID-reader
#define POWER                           17

// GPIOs (Rotary encoder)
#define DREHENCODER_CLK                 34
#define DREHENCODER_DT                  35
#define DREHENCODER_BUTTON              32

// GPIOs (Control-buttons)
#define PAUSEPLAY_BUTTON                5
#define NEXT_BUTTON                     4
#define PREVIOUS_BUTTON                 33

// GPIOs (LEDs)
#define LED_PIN                         12

// Neopixel-configuration
#define NUM_LEDS                        24
#define CHIPSET                         WS2811
#define COLOR_ORDER                     GRB

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
#define REPEAT_PLAYLIST                 110         // Changes active playmode to endless-loop (for a playlist)
#define REPEAT_TRACK                    111         // Changes active playmode to endless-loop (for a single track)
#define DIMM_LEDS_NIGHTMODE             120         // Changes LED-brightness

// Repeat-Modes
#define NO_REPEAT                       0
#define TRACK                           1
#define PLAYLIST                        2
#define TRACK_N_PLAYLIST                3

typedef struct {
    uint8_t playMode;                   // playMode
    char **playlist;                    // playlist
    bool repeatCurrentTrack;            // If current track should be looped
    bool repeatPlaylist;                // If whole playlist should be looped
    uint16_t currentTrackNumber;        // Current tracknumber
    uint16_t numberOfTracks;            // Number of tracks in playlist
    unsigned long startAtFilePos;       // Offset to start play (in bytes)
    uint8_t currentRelPos;              // Current relative playPosition (in %)
    bool sleepAfterCurrentTrack;        // If uC should go to sleep after current track
    bool sleepAfterPlaylist;            // If uC should go to sleep after whole playlist
    bool saveLastPlayPosition;          // If playposition/current track should be saved (for AUDIOBOOK)
    char playRfidTag[13];               // ID of RFID-tag that started playlist
    bool pausePlay;                     // If pause is active
    bool trackFinished;                 // If current track is finished
    bool playlistFinished;              // If whole playlist is finished
} playProps;
playProps playProperties;

// Zeugs
const char* PARAM_MESSAGE = "message";

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}
AsyncWebServer wServer(81);

// Audio/mp3
SPIClass spiSD(HSPI);
TaskHandle_t mp3Play;
TaskHandle_t rfid;
TaskHandle_t LED;

// Webserver
WebServer server(80);

// LED-brightness-configuration
uint8_t initialLedBrightness = 16;
uint8_t ledBrightness = initialLedBrightness;
uint8_t nightLedBrightness = 2;

// FTP
FtpServer ftpSrv;

// WiFi-configuration
// Info: SSID / password are stored in NVS
WiFiClient wifiClient;
IPAddress myIP;

// WiFi-helper
unsigned long wifiCheckLastTimestamp = 0;

// AP-WiFi
static const char accessPointNetworkSSID[] PROGMEM = "Tonuino";     // Access-point's SSID
IPAddress apIP(192, 168, 4, 1);                     // Access-point's static IP
IPAddress apNetmask(255, 255, 255, 0);              // Access-point's netmask

// FTP
char ftpUser[10] = "esp32";                         // FTP-user
char ftpPassword[15] = "esp32";                     // FTP-password

// MQTT-configuration
#define DEVICE_HOSTNAME "ESP32-Tonuino"             // Name that that is used for MQTT
bool enableMqtt = true;
uint8_t mqttFailCount = 3;                          // Number of times mqtt-reconnect is allowed to fail. If >= mqttFailCount to further reconnects take place
char mqtt_server[16] = "192.168.2.43";              // IP-address of MQTT-server
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
unsigned long const stillOnlineInterval = 60;       // Interval 'I'm still alive' is sent via MQTT (in seconds)

// Neopixel-helper
bool showLedError = false;
bool showLedOk = false;
bool showPlaylistProgress = false;
bool showRewind = false;

// MQTT-helper
PubSubClient MQTTclient(wifiClient);
unsigned long lastOnlineTimestamp = 0;

// RFID-helper
unsigned long lastRfidCheckTimestamp = 0;
uint8_t const cardIdSize = 4;

// Loudness-configuration
uint8_t maxVolume = 21;                                 // Maximum volume that can be adjusted
uint8_t minVolume = 0;                                  // Lowest volume that can be adjusted
uint8_t initVolume = 3;                                 // 0...21

// Rotary encoder-configuration
ESP32Encoder encoder;

// Rotary encoder-helper
int32_t lastEncoderValue;
int32_t currentEncoderValue;
int32_t lastVolume = -1;                                // Don't change -1 as initial-value!
uint8_t currentVolume = initVolume;

// Sleep-configuration
uint16_t maxInactivityTime = 10;                        // Time in minutes, after uC is put to deep sleep because of inactivity
unsigned long sleepTimer = 30;                          // Sleep timer in minutes that can be optionally used (and modified later via MQTT or RFID)

// Sleep-helper
unsigned long lastTimeActiveTimestamp = 0;              // Timestamp of last user-interaction
unsigned long sleepTimerStartTimestamp = 0;             // Flag if sleep-timer is active
bool gotoSleep = false;                                 // Flag for turning uC immediately into deepsleep

// Music-player-helper
char *currentRfidTagId = NULL;

// Button-configuration
uint8_t buttonDebounceInterval = 50;                    // Interval in ms to software-debounce buttons
uint16_t intervalToLongPress = 700;                     // Interval in ms to distinguish between short and long press of previous/next-button

// HW-Timer / Control-button-helper
hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
bool lockControls;                                          // FLag if buttons and rotary encoder is locked
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

char stringDelimiter[] = "#";                                       // Character used to encapsulate data in linear NVS-strings
bool bootComplete = false;

QueueHandle_t volumeQueue;
QueueHandle_t trackQueue;
QueueHandle_t trackControlQueue;
QueueHandle_t rfidCardQueue;


// Prototypes
void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask);
static int arrSortHelper(const void* a, const void* b);
void callback(const char *topic, const byte *payload, uint32_t length);
void buttonHandler();
void doButtonActions(void);
void doRfidCardModifications(const uint32_t mod);
void deepSleepManager(void);
bool endsWith (const char *str, const char *suf);
bool fileValid(const char *_fileItem);
void freeMultiCharArray(char **arr, const uint32_t cnt);
uint8_t getRepeatMode(void);
void handleWifiSetup();
void loggerNl(const char *str, const uint8_t logLevel);
void logger(const char *str, const uint8_t logLevel);
bool publishMqtt(const char *topic, const char *payload, bool retained);
void postHeartbeatViaMqtt(void);
size_t nvsRfidWriteWrapper (const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks);
void randomizePlaylist (char *str[], const uint32_t count);
bool reconnect();
char ** returnPlaylistFromWebstream(const char *_webUrl);
char ** returnPlaylistFromSD(File _fileOrDirectory);
void rfidScanner(void *parameter);
void sleepHandler(void) ;
void sortPlaylist(const char** arr, int n);
bool startsWith(const char *str, const char *pre);
void trackControlToQueueSender(const uint8_t trackCommand);
void rfidPreferenceLookupHandler (void);
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

void printDirectory(File dir, int numTabs) {
  while (true) {

    char fileNameBuf[255];
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
        strncpy(fileNameBuf, (char *) entry.name(), sizeof(fileNameBuf) / sizeof(fileNameBuf[0]));
        if (fileValid(fileNameBuf)) {
            Serial.print("\t\t");
            Serial.println(entry.size(), DEC);
        }
    }
    entry.close();
  }
}


int countChars(const char* string, char ch)
{
    int count = 0;
    int length = strlen(string);

    for (uint8_t i = 0; i < length; i++) {
        if (string[i] == ch) {
            count++;
        }
    }

    return count;
}


void printSdContent(File dir, uint16_t allocSize, uint8_t allocCount, char *sdContent, uint8_t depth) {
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
        strcat(sdContent, "#");
        strcat(sdContent, entry.name());

        if (entry.isDirectory()) {
            printSdContent(entry, allocSize, allocCount, sdContent, depth);
        }
        entry.close();
    }
}

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

  while (!MQTTclient.connected() && mqttFailCount < maxRetries) {
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s %s", (char *) FPSTR(tryConnectMqttS), mqtt_server);
    loggerNl(logBuf, LOGLEVEL_NOTICE);

    // Try to connect to MQTT-server
    if (MQTTclient.connect(DEVICE_HOSTNAME)) {
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

/* Is called if there's a new MQTT-message
*/
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
        if (strcmp(receivedString, "EOP") == 0) {
            playProperties.sleepAfterPlaylist = true;
            loggerNl((char *) FPSTR(sleepTimerEOP), LOGLEVEL_NOTICE);
            showLedOk = true;
            return;
        } else if (strcmp(receivedString, "EOT") == 0) {
            playProperties.sleepAfterCurrentTrack = true;
            loggerNl((char *) FPSTR(sleepTimerEOT), LOGLEVEL_NOTICE);
            showLedOk = true;
            return;
        } else if (strcmp(receivedString, "0") == 0) {
            if (sleepTimerStartTimestamp) {
                sleepTimerStartTimestamp = 0;
                loggerNl((char *) FPSTR(sleepTimerStop), LOGLEVEL_NOTICE);
                showLedOk = true;
                publishMqtt((char *) FPSTR(topicSleepState), 0, false);
                return;
            } else {
                loggerNl((char *) FPSTR(sleepTimerAlreadyStopped), LOGLEVEL_INFO);
                showLedError = true;
                return;
            }
        }
        sleepTimer = strtoul(receivedString, NULL, 10);
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %lu Minute(n)", (char *) FPSTR(sleepTimerSetTo), sleepTimer);
        loggerNl(logBuf, LOGLEVEL_NOTICE);
        showLedOk = true;

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
            showLedOk = true;

        } else if (strcmp(receivedString, "ON") == 0) {
            lockControls = true;
            loggerNl((char *) FPSTR(lockButtons), LOGLEVEL_NOTICE);
            showLedOk = true;
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
                showLedError = true;
            } else {
                switch (repeatMode) {
                    case NO_REPEAT:
                        playProperties.repeatCurrentTrack = false;
                        playProperties.repeatPlaylist = false;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl((char *) FPSTR(modeRepeatNone), LOGLEVEL_INFO);
                        showLedOk = true;
                        break;

                    case TRACK:
                        playProperties.repeatCurrentTrack = true;
                        playProperties.repeatPlaylist = false;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl((char *) FPSTR(modeRepeatTrack), LOGLEVEL_INFO);
                        showLedOk = true;
                        break;

                    case PLAYLIST:
                        playProperties.repeatCurrentTrack = false;
                        playProperties.repeatPlaylist = true;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl((char *) FPSTR(modeRepeatPlaylist), LOGLEVEL_INFO);
                        showLedOk = true;
                        break;

                    case TRACK_N_PLAYLIST:
                        playProperties.repeatCurrentTrack = true;
                        playProperties.repeatPlaylist = true;
                        snprintf(rBuf, 2, "%u", getRepeatMode());
				        publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                        loggerNl((char *) FPSTR(modeRepeatTracknPlaylist), LOGLEVEL_INFO);
                        showLedOk = true;
                        break;

                    default:
                        showLedError = true;
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
        showLedError = true;
    }

    free(receivedString);
    free(mqttTopic);
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


static int arrSortHelper(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

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


// Puts webstream into playlist; same like returnPlaylistFromSD() but always only one file
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
            showLedError = true;
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
                        showLedError = true;
                        return files;
                    }
                }
                strcat(serializedPlaylist, "#");
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
        showLedError = true;
        free(serializedPlaylist);
        return NULL;
    }

    // Extract elements out of serialized playlist and copy to playlist
    char *token;
    token = strtok(serializedPlaylist, "#");
    uint32_t pos = 1;
    while( token != NULL ) {
        files[pos++] = strdup(token);
        token = strtok(NULL, "#");
    }

    free(serializedPlaylist);

    files[0] = (char *) malloc(sizeof(char) * 5);
    if (files[0] == NULL) {
        loggerNl((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
        showLedError = true;
        return NULL;
    }
    sprintf(files[0], "%u", cnt);
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(numberOfValidFiles), cnt);
    loggerNl(logBuf, LOGLEVEL_NOTICE);

    return ++files;         // return ptr+1 (starting at 1st payload-item)
}


/* Wraps putString for writing settings into NVS for RFID-cards */
size_t nvsRfidWriteWrapper (const char *_rfidCardId, const char *_track, const uint32_t _playPosition, const uint8_t _playMode, const uint16_t _trackLastPlayed, const uint16_t _numberOfTracks) {
    char prefBuf[275];
    char trackBuf[255];
    snprintf(trackBuf, sizeof(trackBuf) / sizeof(trackBuf[0]), _track);

    // If it's a directory we want to play/save we just need basename(path).
    if (_numberOfTracks > 1) {
        const char s = '/';
        char *last = strrchr(_track, s);
        char *first = strchr(_track, s);
        unsigned long substr = last-first+1;
        snprintf(trackBuf, substr, _track);     // save substring basename(_track)
    }

    snprintf(prefBuf, sizeof(prefBuf) / sizeof(prefBuf[0]), "#%s#%u#%d#%u", trackBuf, _playPosition, _playMode, _trackLastPlayed);
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "Schreibe '%s' in NVS für RFID-Card-ID %s mit playmode %d und letzter Track %u\n", prefBuf, _rfidCardId, _playMode, _trackLastPlayed);
    logger(logBuf, LOGLEVEL_INFO);
    loggerNl(prefBuf, LOGLEVEL_INFO);
    String str = String (prefBuf);
    return prefsRfid.putString(_rfidCardId, prefBuf);
}


/* Function to play music as distinct task
   As this task has to run pretty fast in order to avoid ugly sound-quality,
   it makes sense to give it a distinct core on the ESP32 (xTaskCreatePinnedToCore)
*/
void playAudio(void *parameter) {
    static Audio audio;
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(initVolume);

    uint8_t currentVolume;
    static BaseType_t trackQStatus;
    static uint8_t trackCommand = 0;

    for (;;) {
        if (xQueueReceive(volumeQueue, &currentVolume, 0) == pdPASS ) {
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(newLoudnessReceivedQueue), currentVolume);
            loggerNl(logBuf, LOGLEVEL_INFO);
            audio.setVolume(currentVolume);
            publishMqtt((char *) FPSTR(topicLoudnessState), currentVolume, false);
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
                        // Only save if there's another track, otherwise it will be saved at end of playlist
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber+1, playProperties.numberOfTracks);
                    }
                }
                if (playProperties.sleepAfterCurrentTrack) {                        // Go to sleep if "sleep after track" was requested
                    gotoSleep = true;
                }
                if (!playProperties.repeatCurrentTrack) {   // If endless-loop requested, track-number will not be incremented
                    playProperties.currentTrackNumber++;
                } else {
                    loggerNl((char *) FPSTR(repeatTrackDueToPlaymode), LOGLEVEL_INFO);
                    showRewind = true;
                }
            }

            if (playProperties.playlistFinished && trackCommand != 0) {
                loggerNl((char *) FPSTR(noPlaymodeChangeIfIdle), LOGLEVEL_NOTICE);
                trackCommand = 0;
                showLedError = true;
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
                    playProperties.pausePlay = !playProperties.pausePlay;
                    if (playProperties.saveLastPlayPosition) {
                        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "Titel wurde bei Position %u pausiert.", audio.getFilePos());
                        loggerNl(logBuf, LOGLEVEL_INFO);
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), audio.getFilePos(), playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                    }
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
						publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
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
                        showLedError = true;
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
						publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
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
                            showLedError = true;
                            trackCommand = 0;
                            continue;
                        }
                        if (playProperties.saveLastPlayPosition) {
                            nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + playProperties.currentTrackNumber), 0, playProperties.playMode, playProperties.currentTrackNumber, playProperties.numberOfTracks);
                        }
                            audio.stopSong();
                            showRewind = true;
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
                        showLedError = true;
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
                        showLedError = true;
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
                    showLedError = true;
                    continue;
            }

            if (playProperties.currentTrackNumber >= playProperties.numberOfTracks) {         // Check if last element of playlist is already reached
                loggerNl((char *) FPSTR(endOfPlaylistReached), LOGLEVEL_NOTICE);
                if (!playProperties.repeatPlaylist) {
                    if (playProperties.saveLastPlayPosition) {
                        // Set back to first track
                        nvsRfidWriteWrapper(playProperties.playRfidTag, *(playProperties.playlist + 0), 0, playProperties.playMode, 0, playProperties.numberOfTracks);
                    }
                    publishMqtt((char *) FPSTR(topicTrackState), "<Ende>", false);
                    playProperties.playlistFinished = true;
                    playProperties.playMode = NO_PLAYLIST;
                    publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
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
                    showPlaylistProgress = true;
                    if (playProperties.startAtFilePos > 0) {
                        audio.setFilePos(playProperties.startAtFilePos);
                        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s %u", (char *) FPSTR(trackStartatPos), audio.getFilePos());
                        loggerNl(logBuf, LOGLEVEL_NOTICE);
                    }
                    char buf[255];
                    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "(%d/%d) %s", (playProperties.currentTrackNumber+1), playProperties.numberOfTracks, (const char*) *(playProperties.playlist + playProperties.currentTrackNumber));
                    publishMqtt((char *) FPSTR(topicTrackState), buf, false);
                    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "'%s' wird abgespielt (%d von %d)", *(playProperties.playlist + playProperties.currentTrackNumber), (playProperties.currentTrackNumber+1) , playProperties.numberOfTracks);
                    loggerNl(logBuf, LOGLEVEL_NOTICE);
                    playProperties.playlistFinished = false;
                }
            }
        }

        // Calculate relative position in file (for neopixel) for SD-card-mode
        if (!playProperties.playlistFinished && playProperties.playMode != WEBSTREAM) {
            double fp = (double) audio.getFilePos() / (double) audio.getFileSize();
            if (millis() % 100 == 0) {
                playProperties.currentRelPos = fp * 100;
            }
        } else {
            playProperties.currentRelPos = 0;
        }

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


void rfidScanner(void *parameter) {
    static MFRC522 mfrc522(RFID_CS, RST_PIN);
    SPI.begin();
    mfrc522.PCD_Init();
    mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader detail
    delay(4);
    loggerNl((char *) FPSTR(rfidScannerReady), LOGLEVEL_DEBUG);
    byte cardId[cardIdSize];
    char *cardIdString;

    for (;;) {
        esp_task_wdt_reset();
        vTaskDelay(10);
        if ((millis() - lastRfidCheckTimestamp) >= 300) {
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
                showLedError = true;
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
void showLed(void *parameter) {
    static uint8_t hlastVolume = currentVolume;
    static uint8_t lastPos = playProperties.currentRelPos;
    static bool lastPlayState = false;
    static bool lastLockState = false;
    static bool ledBusyShown = false;
    static bool notificationShown = false;
    static bool volumeChangeShown = false;
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
        if (!bootComplete) {
            FastLED.clear();
            FastLED.show();
            vTaskDelay(portTICK_RATE_MS*10);
            esp_task_wdt_reset();
            continue;
        }
        if (lastLedBrightness != ledBrightness) {
            FastLED.setBrightness(ledBrightness);
            lastLedBrightness = ledBrightness;
        }

        if (!buttons[3].currentState) {
            FastLED.clear();
            for(uint8_t led = 0; led < NUM_LEDS; led++) {
                leds[led] = CRGB::Red;
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

            for(uint8_t led = 0; led < NUM_LEDS; led++) {
                leds[led] = CRGB::Red;
            }
            FastLED.show();
            vTaskDelay(portTICK_RATE_MS * 200);
        }

        if (showLedOk) {             // If action was accepted
            showLedOk = false;
            notificationShown = true;
            FastLED.clear();

            for(uint8_t led = 0; led < NUM_LEDS; led++) {
                leds[led] = CRGB::Green;
            }
            FastLED.show();
            vTaskDelay(portTICK_RATE_MS * 400);
        }

        if (hlastVolume != currentVolume) {         // If volume has been changed
            uint8_t numLedsToLight = map(currentVolume, 0, maxVolume, 0, NUM_LEDS);
            hlastVolume = currentVolume;
            volumeChangeShown = true;
            FastLED.clear();

            for(int led = 0; led < numLedsToLight; led++) {     // (Inverse) color-gradient from green (85) back to (still) red (245) using unsigned-cast
                leds[led].setHue((uint8_t) (85 - ((double) 95 / NUM_LEDS) * led));
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
                leds[i] = CRGB::Black;
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
                    leds[i] = CRGB::Blue;
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
                    leds[i-1] = CRGB::Black;
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
                    for (uint8_t i=0; i < NUM_LEDS; i++) {
                        FastLED.clear();
                        if (i == 0) {
                            leds[0] = CRGB::White;
                            leds[NUM_LEDS/4] = CRGB::White;
                            leds[NUM_LEDS/2] = CRGB::White;
                            leds[NUM_LEDS/4*3] = CRGB::White;
                        } else {
                            leds[i % NUM_LEDS] = CRGB::White;
                            leds[(i+NUM_LEDS/4) % NUM_LEDS] = CRGB::White;
                            leds[(i+NUM_LEDS/2) % NUM_LEDS] = CRGB::White;
                            leds[(i+NUM_LEDS/4*3) % NUM_LEDS] = CRGB::White;
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
                    if (i == 0) {
                        leds[0] = CRGB::BlueViolet;
                        leds[NUM_LEDS/4] = CRGB::BlueViolet;
                        leds[NUM_LEDS/2] = CRGB::BlueViolet;
                        leds[NUM_LEDS/4*3] = CRGB::BlueViolet;
                    } else {
                        leds[i % NUM_LEDS] = CRGB::BlueViolet;
                        leds[(i+NUM_LEDS/4) % NUM_LEDS] = CRGB::BlueViolet;
                        leds[(i+NUM_LEDS/2) % NUM_LEDS] = CRGB::BlueViolet;
                        leds[(i+NUM_LEDS/4*3) % NUM_LEDS] = CRGB::BlueViolet;
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
                            for(uint8_t led = 0; led < numLedsToLight; led++) {
                                if (lockControls) {
                                    leds[led] = CRGB::Red;
                                } else if (!playProperties.pausePlay) { // Hue-rainbow
                                    leds[led].setHue((uint8_t) (((double) 255 / NUM_LEDS) * led));
                                } else if (playProperties.pausePlay) {
                                    leds[led] = CRGB::Orange;
                                }
                            }
                        }
                    } else { // ... but to things a little bit different for Webstream as there's no progress available
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
                                leds[ledPosWebstream] = CRGB::Red;
                                leds[(ledPosWebstream+NUM_LEDS/2) % NUM_LEDS] = CRGB::Red;
                            } else if (!playProperties.pausePlay) {
                                leds[ledPosWebstream].setHue(webstreamColor);
                                leds[(ledPosWebstream+NUM_LEDS/2) % NUM_LEDS].setHue(webstreamColor++);
                            } else if (playProperties.pausePlay) {
                                leds[ledPosWebstream] = CRGB::Orange;
                                leds[(ledPosWebstream+NUM_LEDS/2) % NUM_LEDS] = CRGB::Orange;
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

// Sets deep-sleep-flag if necessary
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
        publishMqtt((char *) FPSTR(topicState), "Offline", false);
        publishMqtt((char *) FPSTR(topicTrackState), "---", false);
        MQTTclient.disconnect();
        FastLED.clear();
        FastLED.show();
        MQTTclient.disconnect();
        SPI.end();
        spiSD.end();
        delay(200);
        esp_deep_sleep_start();
    }
}


// Puts new volume to volume-queue
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


// Puts new control-command to control-queue
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

    publishMqtt((char *) FPSTR(topicLedBrightnessState), 0, false);

    playProperties.playMode = BUSY;     // Show @Neopixel, if uC is busy with creating playlist
    publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
    if (_playMode != 8) {
        musicFiles = returnPlaylistFromSD(SD.open(filename));
    } else {
        musicFiles = returnPlaylistFromWebstream(filename);
    }
    publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);

    if (musicFiles == NULL) {
        loggerNl((char *) FPSTR(errorOccured), LOGLEVEL_ERROR);
        showLedError = true;
        playProperties.playMode = NO_PLAYLIST;
        return;
    } else if (!strcmp(*(musicFiles-1), "0")) {
        loggerNl((char *) FPSTR(noMp3FilesInDir), LOGLEVEL_NOTICE);
        showLedError = true;
        playProperties.playMode = NO_PLAYLIST;
        free (filename);
        return;
    }

    playProperties.playMode = _playMode;
    playProperties.numberOfTracks = strtoul(*(musicFiles-1), NULL, 10);
    // Setting default-values
    playProperties.repeatCurrentTrack = false;
    playProperties.repeatPlaylist = false;
    playProperties.sleepAfterCurrentTrack = false;
    playProperties.sleepAfterPlaylist = false;
    playProperties.saveLastPlayPosition = false;

    switch(playProperties.playMode) {
        case SINGLE_TRACK: {
            loggerNl((char *) FPSTR(modeSingleTrack), LOGLEVEL_NOTICE);
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case SINGLE_TRACK_LOOP: {
            playProperties.repeatCurrentTrack = true;
            loggerNl((char *) FPSTR(modeSingleTrackLoop), LOGLEVEL_NOTICE);
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), TRACK, false);
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case AUDIOBOOK: {   // Tracks need to be alph. sorted!
            playProperties.saveLastPlayPosition = true;
            loggerNl((char *) FPSTR(modeSingleAudiobook), LOGLEVEL_NOTICE);
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case AUDIOBOOK_LOOP: {  // Tracks need to be alph. sorted!
            playProperties.repeatPlaylist = true;
            playProperties.saveLastPlayPosition = true;
            loggerNl((char *) FPSTR(modeSingleAudiobookLoop), LOGLEVEL_NOTICE);
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_SORTED: {
            snprintf(logBuf, sizeof(logBuf)/sizeof(logBuf[0]), "%s '%s' ", (char *) FPSTR(modeAllTrackAlphSorted), filename);
            loggerNl(logBuf, LOGLEVEL_NOTICE);
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_RANDOM: {
            loggerNl((char *) FPSTR(modeAllTrackRandom), LOGLEVEL_NOTICE);
            randomizePlaylist(musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_SORTED_LOOP: {
            playProperties.repeatPlaylist = true;
            loggerNl((char *) FPSTR(modeAllTrackAlphSortedLoop), LOGLEVEL_NOTICE);
            sortPlaylist((const char**) musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case ALL_TRACKS_OF_DIR_RANDOM_LOOP: {
            playProperties.repeatPlaylist = true;
            loggerNl((char *) FPSTR(modeAllTrackRandomLoop), LOGLEVEL_NOTICE);
            randomizePlaylist(musicFiles, strtoul(*(musicFiles-1), NULL, 10));
            publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
            publishMqtt((char *) FPSTR(topicRepeatModeState), PLAYLIST, false);
            xQueueSend(trackQueue, &(musicFiles), 0);
            break;
        }

        case WEBSTREAM: {   // This is always just one "track"
            loggerNl((char *) FPSTR(modeWebstream), LOGLEVEL_NOTICE);
            if (wifiManager() == WL_CONNECTED) {
                xQueueSend(trackQueue, &(musicFiles), 0);
                publishMqtt((char *) FPSTR(topicPlaymodeState), playProperties.playMode, false);
                publishMqtt((char *) FPSTR(topicRepeatModeState), NO_REPEAT, false);
            } else {
                loggerNl((char *) FPSTR(webstreamNotAvailable), LOGLEVEL_ERROR);
            }
            break;
        }

        default:
            loggerNl((char *) FPSTR(modeDoesNotExist), LOGLEVEL_ERROR);
            showLedError = true;
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
                publishMqtt((char *) FPSTR(topicLockControlsState), "ON", false);
                showLedOk = true;
            } else {
                loggerNl((char *) FPSTR(modificatorAllButtonsUnlocked), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicLockControlsState), "OFF", false);
                showLedOk = true;
            }
            break;

        case SLEEP_TIMER_MOD_15:    // Puts/undo uC to sleep after 15 minutes
            if (sleepTimer == 15) {
                sleepTimerStartTimestamp = 0;
                ledBrightness = initialLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);

            } else {
                sleepTimer = 15;
                sleepTimerStartTimestamp = millis();
                ledBrightness = nightLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepTimer15), LOGLEVEL_NOTICE);
                loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            showLedOk = true;
            break;

        case SLEEP_TIMER_MOD_30:    // Puts/undo uC to sleep after 30 minutes
            if (sleepTimer == 30) {
                sleepTimerStartTimestamp = 0;
                ledBrightness = initialLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);

            } else {
                sleepTimer = 30;
                sleepTimerStartTimestamp = millis();
                ledBrightness = nightLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepTimer30), LOGLEVEL_NOTICE);
                loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            showLedOk = true;
            break;

        case SLEEP_TIMER_MOD_60:    // Puts/undo uC to sleep after 60 minutes
            if (sleepTimer == 60) {
                sleepTimerStartTimestamp = 0;
                ledBrightness = initialLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);

            } else {
                sleepTimer = 60;
                sleepTimerStartTimestamp = millis();
                ledBrightness = nightLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepTimer60), LOGLEVEL_NOTICE);
                loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            showLedOk = true;
            break;

        case SLEEP_TIMER_MOD_120:    // Puts/undo uC to sleep after 2 hrs
            if (sleepTimer == 120) {
                sleepTimerStartTimestamp = 0;
                ledBrightness = initialLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepd), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);

            } else {
                sleepTimer = 120;
                sleepTimerStartTimestamp = millis();
                ledBrightness = nightLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepTimer120), LOGLEVEL_NOTICE);
                loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                publishMqtt((char *) FPSTR(topicSleepTimerState), sleepTimer, false);
                publishMqtt((char *) FPSTR(topicLedBrightnessState), nightLedBrightness, false);
            }

            playProperties.sleepAfterCurrentTrack = false;      // deactivate/overwrite if already active
            playProperties.sleepAfterPlaylist = false;          // deactivate/overwrite if already active
            showLedOk = true;
            break;

        case SLEEP_AFTER_END_OF_TRACK:  // Puts uC to sleep after end of current track
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                showLedError = true;
                return;
            }
            if (playProperties.sleepAfterCurrentTrack) {
                loggerNl((char *) FPSTR(modificatorSleepAtEOTd), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
                ledBrightness = initialLedBrightness;
            } else {
                loggerNl((char *) FPSTR(modificatorSleepAtEOT), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicSleepTimerState), "EOT", false);
                ledBrightness = nightLedBrightness;
                loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
            }
            playProperties.sleepAfterCurrentTrack = !playProperties.sleepAfterCurrentTrack;
            playProperties.sleepAfterPlaylist = false;
            sleepTimerStartTimestamp = 0;

            publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
            showLedOk = true;
            break;

        case SLEEP_AFTER_END_OF_PLAYLIST:   // Puts uC to sleep after end of whole playlist (can take a while :->)
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                showLedError = true;
                return;
            }
            if (playProperties.sleepAfterCurrentTrack) {
                publishMqtt((char *) FPSTR(topicSleepTimerState), "0", false);
                ledBrightness = initialLedBrightness;
                loggerNl((char *) FPSTR(modificatorSleepAtEOPd), LOGLEVEL_NOTICE);
            } else {
                ledBrightness = nightLedBrightness;
                loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
                loggerNl((char *) FPSTR(modificatorSleepAtEOP), LOGLEVEL_NOTICE);
                publishMqtt((char *) FPSTR(topicSleepTimerState), "EOP", false);
            }

            playProperties.sleepAfterCurrentTrack = false;
            playProperties.sleepAfterPlaylist = !playProperties.sleepAfterPlaylist;
            sleepTimerStartTimestamp = 0;
            publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
            showLedOk = true;
            break;

        case REPEAT_PLAYLIST:
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                showLedError = true;
            } else {
                if (playProperties.repeatPlaylist) {
                    loggerNl((char *) FPSTR(modificatorPlaylistLoopDeactive), LOGLEVEL_NOTICE);
                } else {
                    loggerNl((char *) FPSTR(modificatorPlaylistLoopActive), LOGLEVEL_NOTICE);
                }
                playProperties.repeatPlaylist = !playProperties.repeatPlaylist;
                char rBuf[2];
                snprintf(rBuf, 2, "%u", getRepeatMode());
                publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                showLedOk = true;
            }
            break;

        case REPEAT_TRACK:      // Introduces looping for track-mode
            if (playProperties.playMode == NO_PLAYLIST) {
                loggerNl((char *) FPSTR(modificatorNotallowedWhenIdle), LOGLEVEL_NOTICE);
                showLedError = true;
            } else {
                if (playProperties.repeatCurrentTrack) {
                    loggerNl((char *) FPSTR(modificatorTrackDeactive), LOGLEVEL_NOTICE);
                } else {
                    loggerNl((char *) FPSTR(modificatorTrackActive), LOGLEVEL_NOTICE);
                }
                playProperties.repeatCurrentTrack = !playProperties.repeatCurrentTrack;
                char rBuf[2];
                snprintf(rBuf, 2, "%u", getRepeatMode());
                publishMqtt((char *) FPSTR(topicRepeatModeState), rBuf, false);
                showLedOk = true;
            }
            break;

        case DIMM_LEDS_NIGHTMODE:
            publishMqtt((char *) FPSTR(topicLedBrightnessState), ledBrightness, false);
            loggerNl((char *) FPSTR(ledsDimmedToNightmode), LOGLEVEL_INFO);
            ledBrightness = nightLedBrightness;
            showLedOk = true;
            break;

        default:
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s %d !", (char *) FPSTR(modificatorDoesNotExist), mod);
            loggerNl(logBuf, LOGLEVEL_ERROR);
            showLedError = true;
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
        loggerNl(logBuf, LOGLEVEL_INFO);

        String s = prefsRfid.getString(rfidTagId, "-1");                 // Try to lookup rfidId in NVS
        if (!s.compareTo("-1")) {
            loggerNl((char *) FPSTR(rfidTagUnknownInNvs), LOGLEVEL_ERROR);
            showLedError = true;
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
            showLedError = true;
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


void handleWifiSetup() {
    String setupPage = "<!DOCTYPE html>";
    setupPage += "<html>";
    setupPage += "<head>";
    setupPage += "<title>WLAN-Einrichtung</title>";
    setupPage += "<h1>Initiale WLAN-Einrichtung</h1>";
    setupPage += "<form action='/'>";
    setupPage += "    SSID: <input type='text' name='ssid'><br>";
    setupPage += "<br>";
    setupPage += "    Passwort: <input type='text' name='pw'><br>";
    setupPage += "<br>";
    setupPage += "   <input type='submit' value='Absenden'>";
    setupPage += " </form>";
    setupPage += "<br><br>";
    setupPage += "<form action='/restart'>";
    setupPage += "   <input type='submit' value='Neustart'>";
    setupPage += " </form>";
    setupPage += "</head>";
    setupPage += "</html>";

    server.send ( 200, "text/html", setupPage);
    if (server.args() > 0) { // Arguments were received
        for (uint8_t i=0; i < server.args(); i++) {
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s=%s", (char *) FPSTR(statementsReceivedByServer),  server.argName(i).c_str(), server.arg(i).c_str());
            loggerNl(logBuf, LOGLEVEL_INFO);

            if (server.argName(i) == "ssid") {
                snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(savedSsidInNvs), server.arg(i).c_str());
                loggerNl(logBuf, LOGLEVEL_NOTICE);
                prefsSettings.putString("SSID", server.arg(i));
            } else if (server.argName(i) == "pw") {
                snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(savedWifiPwdInNvs), server.arg(i).c_str());
                loggerNl(logBuf, LOGLEVEL_NOTICE);
                prefsSettings.putString("Password", server.arg(i));
            }
        }
    }
}


// Initialize Soft Access Point with ESP32
// ESP32 establishes its own WiFi network, one can choose the SSID
void accessPointStart(const char *SSID, IPAddress ip, IPAddress netmask) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, ip, netmask);
    WiFi.softAP(SSID);
    delay(500);

    loggerNl((char *) FPSTR(apReady), LOGLEVEL_NOTICE);
    snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "IP-Adresse: %d.%d.%d.%d", apIP[0],apIP[1], apIP[2], apIP[3]);
    loggerNl(logBuf, LOGLEVEL_NOTICE);

    server.on("/", handleWifiSetup);                // Show setup-page
    server.on("/restart", []() {                    // Show restart-page
        server.send(200, "text/plain", "ESP-Reset wird durchgeführt...");
        ESP.restart();
    });

    server.begin();

    loggerNl((char *) FPSTR(httpReady), LOGLEVEL_NOTICE);
    while (true) {
        server.handleClient();                      // Wird endlos ausgeführt damit das WLAN Setup erfolgen kann
    }
}


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

        // ...and create a connection with it. If not successful, an access-point will is opened
        WiFi.begin(_ssid, _pwd);

        uint8_t tryCount=0;
        while (WiFi.status() != WL_CONNECTED && tryCount <= 4) {
            delay(500);
            Serial.print(F("."));
            tryCount++;
            wifiCheckLastTimestamp = millis();
            if (tryCount >= 4 && WiFi.status() == WL_CONNECT_FAILED) {
                WiFi.begin(_ssid, _pwd);        // ESP32-workaround
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
            myIP = WiFi.localIP();
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "Aktuelle IP: %d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
            loggerNl(logBuf, LOGLEVEL_NOTICE);
            ftpSrv.begin(ftpUser, ftpPassword);
        } else { // Starts AP if WiFi-connect wasn't successful
            accessPointStart((char *) FPSTR(accessPointNetworkSSID), apIP, apNetmask);
        }
    }

    return WiFi.status();
}


void setup() {
    Serial.begin(115200);
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
    // Don't forget to comment this section out for regular usage as it probably overwrites saved states in mode audiobook
    prefsRfid.putString("215123125075", "#/mp3/Kinderlieder#0#6#0");
    prefsRfid.putString("009236075184", "#/Aura - Avoure.mp3#0#3#0");
    prefsRfid.putString("073022077184", "#/kurz#0#7#0");
    prefsRfid.putString("169239075184", "#http://radio.koennmer.net/evosonic.mp3#0#8#0");
    prefsRfid.putString("244105171042", "#0#0#111#0"); // modification-card (repeat track)
    prefsRfid.putString("075081176028", "#0#0#106#0"); // modification-card (sleep at end of playlist)
    prefsRfid.putString("212216120042", "#0#0#105#0"); // modification-card (sleep at end of track)
    prefsRfid.putString("020059140043", "#0#0#111#0"); // modification-card (repeat current track)
    prefsRfid.putString("228064156042", "#0#0#110#0"); // modification-card (repeat playlist)
    prefsRfid.putString("018030087052", "#http://shouthost.com.19.streams.bassdrive.com:8200#0#8#0");
    prefsRfid.putString("182146124043", "#http://ibizaglobalradio.streaming-pro.com:8024#0#8#0");
    prefsRfid.putString("018162219052", "#http://stream2.friskyradio.com:8000/frisky_mp3_hi#0#8#0");
    prefsRfid.putString("160243107050", "#/mp3/Hoerspiele/Sonstige/Dingi und der Containerdieb.mp3#0#3#0");
    prefsRfid.putString("244189084042", "#/mp3/Hoerspiele/Yakari/Yakari und die Pferdediebe#0#3#0");
    prefsRfid.putString("244042007042", "#/mp3/Hoerspiele/Yakari/Der Gesang des Raben#0#3#0");
    prefsRfid.putString("176063100050", "#/mp3/Hoerspiele/Yakari/Best of Lagerfeuergeschichten#0#3#0");
    prefsRfid.putString("004134024043", "#/mp3/Hoerspiele/Yakari/Schneeball in Gefahr#0#3#0");
    prefsRfid.putString("242216118051", "#/mp3/Hoerspiele/Weihnachten mit Astrid Lindgren#0#3#0");
    prefsRfid.putString("176008145050", "#/mp3/Hoerspiele/Janosch/Oh wie schoen ist Panama#0#3#0");
    prefsRfid.putString("036073235043", "#/mp3/Hoerspiele/Paw Patrol/Rettet Weihnachten#0#3#0");


    // Init uSD-SPI
    pinMode(SPISD_CS, OUTPUT);
    digitalWrite(SPISD_CS, HIGH);
    spiSD.begin(SPISD_SCK, SPISD_MISO, SPISD_MOSI, SPISD_CS);
    spiSD.setFrequency(1000000);
    while (!SD.begin(SPISD_CS, spiSD)) {
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
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %d", (char *) FPSTR(loadedInitialBrightnessForNmFromNvs), nvsNLedBrightness);
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
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(loadedFtpUserFromNvs), nvsFtpUser.c_str());
        loggerNl(logBuf, LOGLEVEL_INFO);
    }

    // Get FTP-password from NVS
    String nvsFtpPassword = prefsSettings.getString("ftppassword", "-1");
    if (!nvsFtpPassword.compareTo("-1")) {
        prefsSettings.putString("ftppassword", (String) ftpPassword);
        loggerNl((char *) FPSTR(wroteFtpPwdToNvs), LOGLEVEL_ERROR);
    } else {
        strncpy(ftpPassword, nvsFtpPassword.c_str(), sizeof(ftpPassword)/sizeof(ftpPassword[0]));
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(loadedFtpPwdFromNvs), nvsFtpPassword.c_str());
        loggerNl(logBuf, LOGLEVEL_INFO);
    }

    // Get maximum inactivity-time from NVS
    uint32_t nvsMInactivityTime = prefsSettings.getUInt("mInactiviyT", 0);
    if (nvsMInactivityTime) {
        maxInactivityTime = nvsMInactivityTime;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(loadedMaxInactivityFromNvs), nvsMInactivityTime);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("mInactiviyT", maxInactivityTime);
        loggerNl((char *) FPSTR(wroteMaxInactivityToNvs), LOGLEVEL_ERROR);
    }

    // Get initial volume from NVS
    uint32_t nvsInitialVolume = prefsSettings.getUInt("initVolume", 0);
    if (nvsInitialVolume) {
        initVolume = nvsInitialVolume;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(loadedInitialLoudnessFromNvs), nvsInitialVolume);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("initVolume", initVolume);
        loggerNl((char *) FPSTR(wroteInitialLoudnessToNvs), LOGLEVEL_ERROR);
    }

    // Get maximum volume from NVS
    uint32_t nvsMaxVolume = prefsSettings.getUInt("maxVolume", 0);
    if (nvsMaxVolume) {
        maxVolume = nvsMaxVolume;
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(loadedMaxLoudnessFromNvs), nvsMaxVolume);
        loggerNl(logBuf, LOGLEVEL_INFO);
    } else {
        prefsSettings.putUInt("maxVolume", maxVolume);
        loggerNl((char *) FPSTR(wroteMaxLoudnessToNvs), LOGLEVEL_ERROR);
    }

    // Get MQTT-enable from NVS
    uint8_t nvsEnableMqtt = prefsSettings.getUChar("enableMQTT", 99);
    switch (nvsEnableMqtt) {
        case 99:
            prefsSettings.putUChar("enableMQTT", 0);
            loggerNl((char *) FPSTR(wroteMqttFlagToNvs), LOGLEVEL_ERROR);
            break;
        case 1:
            prefsSettings.putUChar("enableMQTT", enableMqtt);
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(loadedMqttActiveFromNvs), nvsEnableMqtt);
            loggerNl(logBuf, LOGLEVEL_INFO);
            break;
        case 0:
            enableMqtt = nvsEnableMqtt;
            snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %u", (char *) FPSTR(loadedMqttDeactiveFromNvs), nvsEnableMqtt);
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
        snprintf(logBuf, sizeof(logBuf) / sizeof(logBuf[0]), "%s: %s", (char *) FPSTR(loadedMqttServerFromNvs), nvsMqttServer.c_str());
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

    xTaskCreatePinnedToCore(
        showLed, /* Function to implement the task */
        "LED", /* Name of the task */
        2000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        1 | portPRIVILEGE_BIT,  /* Priority of the task */
        &LED,  /* Task handle. */
        0 /* Core where the task should run */
    );

    esp_sleep_enable_ext0_wakeup((gpio_num_t) DREHENCODER_BUTTON, 0);

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
    if (enableMqtt) {
        MQTTclient.setServer(mqtt_server, 1883);
        MQTTclient.setCallback(callback);
    }

    wifiManager();

    lastTimeActiveTimestamp = millis();     // initial set after boot

    wServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Hello, world");
    });

    // Send a GET request to <IP>/get?message=<message>
    wServer.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String message;
        if (request->hasParam(PARAM_MESSAGE)) {
            message = request->getParam(PARAM_MESSAGE)->value();
        } else {
            message = "No message sent";
        }
        request->send(200, "text/plain", "Hello, GET: " + String(playProperties.playMode) + String(currentRfidTagId));
    });

    // Send a POST request to <IP>/post with a form field message set to <message>
    wServer.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
        String message;
        if (request->hasParam(PARAM_MESSAGE, true)) {
            message = request->getParam(PARAM_MESSAGE, true)->value();
        } else {
            message = "No message sent";
        }
        request->send(200, "text/plain", "Hello, POST: " + message);
    });

    wServer.onNotFound(notFound);

    wServer.begin();
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
        if (enableMqtt) {
            reconnect();
            MQTTclient.loop();
            postHeartbeatViaMqtt();
        }
        ftpSrv.handleFTP();
    }
    if (ftpSrv.isConnected()) {
        lastTimeActiveTimestamp = millis();     // Re-adjust timer while client is connected to avoid ESP falling asleep
    }
    server.handleClient();
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
    publishMqtt((char *) FPSTR(topicTrackState), buf, false);
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