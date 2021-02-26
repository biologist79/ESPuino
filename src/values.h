#ifndef __ESPUINO_VALUES_H__
#define __ESPUINO_VALUES_H__

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
    #define CMD_NOTHING                       0         // Do Nothing
    #define LOCK_BUTTONS_MOD                100         // Locks all buttons and rotary encoder
    #define SLEEP_TIMER_MOD_15              101         // Puts uC into deepsleep after 15 minutes + LED-DIMM
    #define SLEEP_TIMER_MOD_30              102         // Puts uC into deepsleep after 30 minutes + LED-DIMM
    #define SLEEP_TIMER_MOD_60              103         // Puts uC into deepsleep after 60 minutes + LED-DIMM
    #define SLEEP_TIMER_MOD_120             104         // Puts uC into deepsleep after 120 minutes + LED-DIMM
    #define SLEEP_AFTER_END_OF_TRACK        105         // Puts uC into deepsleep after track is finished + LED-DIMM
    #define SLEEP_AFTER_END_OF_PLAYLIST     106         // Puts uC into deepsleep after playlist is finished + LED-DIMM
    #define SLEEP_AFTER_5_TRACKS            107         // Puts uC into deepsleep after five tracks + LED-DIMM
    #define REPEAT_PLAYLIST                 110         // Changes active playmode to endless-loop (for a playlist)
    #define REPEAT_TRACK                    111         // Changes active playmode to endless-loop (for a single track)
    #define DIMM_LEDS_NIGHTMODE             120         // Changes LED-brightness
    #define TOGGLE_WIFI_STATUS              130         // Toggles WiFi-status
    #define TOGGLE_BLUETOOTH_MODE           140         // Toggles Normal/Bluetooth Mode
    #define ENABLE_FTP_SERVER               150         // Enables FTP-server

    #define CMD_PLAYPAUSE                   170         // Command: play/pause
    #define CMD_PREVTRACK                   171         // Command: previous track
    #define CMD_NEXTTRACK                   172         // Command: next track
    #define CMD_FIRSTTRACK                  173         // Command: first track
    #define CMD_LASTTRACK                   174         // Command: last track
    #define CMD_VOLUMEINIT                  175         // Command: set volume to initial value
    #define CMD_VOLUMEUP                    176         // Command: increase volume by 1
    #define CMD_VOLUMEDOWN                  177         // Command: lower volume by 1
    #define CMD_MEASUREBATTERY              178         // Command: Measure battery-voltage
    #define CMD_SLEEPMODE                   179         // Command: Go to deepsleep
    #define CMD_SEEK_FORWARDS               180         // Command: fast forward => don't use, has no effect currently!
    #define CMD_SEEK_BACKWARDS              181         // Command: wind back => don't use, has no effect currently!

    // Repeat-Modes
    #define NO_REPEAT                       0           // No repeat
    #define TRACK                           1           // Repeat current track (infinite loop)
    #define PLAYLIST                        2           // Repeat whole playlist (infinite loop)
    #define TRACK_N_PLAYLIST                3           // Repeat both (infinite loop)

    // Seek-modes
    #define SEEK_NORMAL                     0           // Normal play
    #define SEEK_FORWARDS                   1           // Seek forwards
    #define SEEK_BACKWARDS                  2           // Seek backwards

#endif