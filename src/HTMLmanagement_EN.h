static const char management_HTML[] PROGMEM = "<!DOCTYPE html>\
<html lang=\"de\">\
<head>\
    <title>ESPuino-Settings</title>\
    <meta charset=\"utf-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
    <link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"https://espuino.de/espuino/favicon.ico\">\
    <link rel=\"stylesheet\" href=\"https://espuino.de/espuino/css/bootstrap.min.css\">\
    <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css\"/>\
    <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.1/css/all.min.css\"/>\
    <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/toastr.js/latest/toastr.min.css\"/>\
    <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/bootstrap-slider/11.0.2/css/bootstrap-slider.min.css\" />\
    <script src=\"https://espuino.de/espuino/js/jquery.min.js\"></script>\
    <script src=\"https://code.jquery.com/ui/1.12.0/jquery-ui.min.js\"></script>\
    <script src=\"https://espuino.de/espuino/js/popper.min.js\"></script>\
    <script src=\"https://espuino.de/espuino/js/bootstrap.min.js\"></script>\
    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/jstree.min.js\"></script>\
    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/toastr.js/latest/toastr.min.js\"></script>\
    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/bootstrap-slider/11.0.2/bootstrap-slider.min.js\"></script>\
    <style type=\"text/css\">\
        .filetree {\
            border: 1px solid black;\
            height: 200px;\
            margin: 0em 0em 1em 0em;\
            overflow-y: scroll;\
        }\
        .slider.slider-horizontal {\
            width: 60%%;\
            margin-left: 1em;\
            margin-right: 1em;\
\
        }\
        .slider-handle{\
            height: 30px;\
            width: 30px;\
            top: -5px;\
        }\
\
        legend.scheduler-border {\
            width:inherit; /* Or auto */\
            padding:0 10px; /* To give a bit of padding on the left and right */\
            border-bottom:none;\
        }\
\
        .icon-pos{\
            top: 0.3em;\
            position: relative;\
         }\
        .reboot{\
            color:white;\
        }\
        .reboot:hover{\
            color: orange;\
        }\
\
        .fa-sync:hover {\
            color: #666666;\
        }\
\
        .clickForRefresh {\
            text-align: center;\
            color: gray;\
            cursor: pointer;\
            margin-top: 5em;\
        }\
\
        .clickForRefresh:hover {\
            color: darkgray;\
        }\
\
        .filetree-container {\
            position: relative;\
        }\
\
        .indexing-progress {\
            width: 100%%;\
            height: 100%%;\
            position: absolute;\
            top: 0;\
            left: 0;\
            opacity: 0.7;\
            display: none;\
        }\
\
        .refreshAction{\
            text-align: right;\
            font-size: 0.8em;\
        }\
\
        .refreshAction:hover{\
            cursor: pointer;\
            color: darkgray;\
        }\
\
        .overlay {\
            z-index: 9;\
            opacity: 0.8;\
            background: #1a1919;\
            height: 200px;\
            display: none;\
            width: 100%%;\
        }\
\
        #SubTabContent.tab-content {\
            display: flex;\
        }\
\
        #SubTabContent.tab-content > .tab-pane {\
            display: block; /* undo \"display: none;\" */\
            visibility: hidden;\
            margin-right: -100%%;\
            width: 100%%;\
        }\
\
        #SubTabContent.tab-content > .active {\
            visibility: visible;\
        }\
\
        /* IOS display fix */\
        select {\
            -webkit-appearance: none;\
            -moz-appearance: none;\
            appearance: none;\
            background: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABkAAAAZCAAAAADhgtq/AAAACXBIWXMAAC4jAAAuIwF4pT92AAAAR0lEQVR4nGP5z4ADsOCSGOoyC+NRabjMQqjQQrgUTCYeIrQQyEI3DSyFJIHkApAUkgSy21B0oLo6fiGSBKp/kCUGU4hSRQYAYg0Rw+gGlUQAAAAASUVORK5CYII=') no-repeat;\
            background-position: center right;\
        }\
    </style>\
</head>\
<body>\
<nav class=\"navbar navbar-expand-sm bg-primary navbar-dark\">\
    <div class=\"col-md-12\">\
    <a class=\"float-left navbar-brand\">\
        <img src=\"https://www.espuino.de/espuino/Espuino32.png\"\
             width=\"35\" height=\"35\" class=\"d-inline-block align-top\" alt=\"\"/>\
        ESPuino\
    </a>\
    <a class=\"reboot float-right nav-link\" href=\"/restart\"><i class=\"fas fa-redo\"></i> Restart</a>\
    <a class=\"reboot float-right nav-link\" href=\"/shutdown\"><i class=\"fas fa-power-off\"></i> Shutdown</a>\
    </div>\
</nav>\
<br/>\
    <nav>\
        <div class=\"container nav nav-tabs\" id=\"nav-tab\" role=\"tablist\">\
            <a class=\"nav-item nav-link\" id=\"nav-control-tab\" data-toggle=\"tab\" href=\"#nav-control\" role=\"tab\" aria-controls=\"nav-control\" aria-selected=\"false\"><i class=\"fas fa-gamepad\"></i><span class=\".d-sm-none .d-md-block\"> Control</span></a>\
            <a class=\"nav-item nav-link active\" id=\"nav-rfid-tab\" data-toggle=\"tab\" href=\"#nav-rfid\" role=\"tab\" aria-controls=\"nav-rfid\" aria-selected=\"true\"><i class=\"fas fa-dot-circle\"></i> RFID</a>\
            <a class=\"nav-item nav-link\" id=\"nav-wifi-tab\" data-toggle=\"tab\" href=\"#nav-wifi\" role=\"tab\" aria-controls=\"nav-wifi\" aria-selected=\"false\"><i class=\"fas fa-wifi\"></i><span class=\".d-sm-none .d-md-block\"> WiFi</span></a>\
            %SHOW_MQTT_TAB%\
            %SHOW_FTP_TAB%\
            <a class=\"nav-item nav-link\" id=\"nav-general-tab\" data-toggle=\"tab\" href=\"#nav-general\" role=\"tab\" aria-controls=\"nav-general\" aria-selected=\"false\"><i class=\"fas fa-sliders-h\"></i> General</a>\
            <a class=\"nav-item nav-link\" id=\"nav-tools-tab\" data-toggle=\"tab\" href=\"#nav-tools\" role=\"tab\" aria-controls=\"nav-tools\" aria-selected=\"false\"><i class=\"fas fa-wrench\"></i> Tools</a>\
            <a class=\"nav-item nav-link\" id=\"nav-forum-tab\" data-toggle=\"tab\" href=\"#nav-forum\" role=\"tab\" aria-controls=\"nav-forum\" aria-selected=\"false\"><i class=\"fas fa-comment\"></i><span class=\".d-sm-none .d-md-block\"> Forum</span></a>\
        </div>\
    </nav>\
<br>\
<div class=\"tab-content\" id=\"nav-tabContent\">\
    <div class=\"tab-pane fade\" id=\"nav-wifi\" role=\"tabpanel\" aria-labelledby=\"nav-wifi-tab\">\
        <div class=\"container\" id=\"wifiConfig\">\
            <form action=\"#wifiConfig\" method=\"POST\" onsubmit=\"wifiConfig('wifiConfig'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <legend>WiFi-settings</legend>\
                    <label for=\"ssid\">WiFi-name (SSID):</label>\
                    <input type=\"text\" class=\"form-control\" id=\"ssid\" placeholder=\"SSID\" name=\"ssid\" required>\
                    <div class=\"invalid-feedback\">\
                        Enter WiFi's SSID.\
                    </div>\
                    <label for=\"pwd\">Password:</label>\
                    <input type=\"password\" class=\"form-control\" id=\"pwd\" placeholder=\"Passwort\" name=\"pwd\" required>\
                    <label for=\"hostname\">ESPuino-Name (Hostname):</label>\
                    <input type=\"text\" class=\"form-control\" id=\"hostname\" placeholder=\"espuino\" name=\"hostname\"\
                           value=\"%HOSTNAME%\" pattern=\"^[^-\\.]{2,32}\" required>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
        </div>\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-control\" role=\"tabpanel\" aria-labelledby=\"nav-control-tab\">\
        <div class=\"container\" id=\"navControl\">\
                <div class=\"form-group col-md-12\">\
                    <legend>Control</legend>\
                    <div class=\"buttons\">\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" onclick=\"sendControl(173)\">\
                            <span class=\"fas fa-fast-backward\"></span>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" onclick=\"sendControl(171)\">\
                            <span class=\"fas fa-backward\"></span>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" onclick=\"sendControl(170)\">\
                            <i class=\"fas fa-pause\"></i>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" onclick=\"sendControl(172)\">\
                            <span class=\"fas fa-forward\"></span>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" onclick=\"sendControl(174)\">\
                            <span class=\"fas fa-fast-forward\"></span>\
                        </button>\
                    </div>\
                </div>\
                <br>\
                <div class=\"form-group col-md-12\">\
                    <legend>Volume</legend>\
                        <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i> <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"setVolume\"\
                            data-slider-value=\"%CURRENT_VOLUME%\" value=\"%CURRENT_VOLUME%\" onchange=\"sendVolume(this.value)\">  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i>\
                </div>\
                <br/>\
        </div>\
    </div>\
    <div class=\"tab-pane fade show active\" id=\"nav-rfid\" role=\"tabpanel\" aria-labelledby=\"nav-rfid-tab\">\
        <div class=\"container\" id=\"filetreeContainer\">\
            <fieldset>\
                <legend>Files</legend>\
				<div class=\"filetree-container\">\
				<div id=\"filebrowser\">\
					<div class=\"filetree demo\" id=\"explorerTree\"></div>\
				</div>\
				<div>\
					<form id=\"explorerUploadForm\" method=\"POST\" enctype=\"multipart/form-data\" action=\"/explorer\">\
                        <div class=\"input-group\">\
                            <span class=\"form-control\" id=\"uploaded_file_text\"></span>\
                            <span class=\"input-group-btn\">\
                                <span class=\"btn btn-secondary\" onclick=\"$(this).parent().find('input[type=file]').click();\">Browse</span>\
                                <span class=\"btn btn-primary\" onclick=\"$(this).parent().find('input[type=file]').submit();\">Upload</span>\
                                <input name=\"uploaded_file\" id =\"uploaded_file\" onchange=\"$(this).parent().parent().find('.form-control').html($(this).val().split(/[\\|/]/).pop());\" style=\"display: none;\" type=\"file\" multiple>\
                            </span>\
                        </div>\
                    </form>\
                    <br>\
                    <div class=\"progress\">\
                        <div id=\"explorerUploadProgress\" class=\"progress-bar\" role=\"progressbar\" ></div>\
                    </div>\
                </div>\
                <br>\
				</div>\
            </fieldset>\
        </div>\
        <div class=\"container\" id=\"rfidMusicTags\">\
            <fieldset>\
                <legend>RFID-Assignments</legend>\
            <form action=\"#rfidMusicTags\" method=\"POST\" onsubmit=\"rfidAssign('rfidMusicTags'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <label for=\"rfidIdMusic\">RFID-number (12 digits)</label>\
                    <input type=\"text\" class=\"form-control\" id=\"rfidIdMusic\" maxlength=\"12\" pattern=\"[0-9]{12}\"\
                           placeholder=\"%RFID_TAG_ID%\" name=\"rfidIdMusic\" required>\
                    <br>\
                    <ul class=\"nav nav-tabs\" id=\"SubTab\" role=\"tablist\">\
                        <li class=\"nav-item\">\
                            <a class=\"nav-link active\" id=\"rfid-music-tab\" data-toggle=\"tab\" href=\"#rfidmusic\" role=\"tab\"><i class=\"fas fa-music\"></i> Musik</a>\
                        </li>\
                        <li class=\"nav-item\">\
                            <a class=\"nav-link\" id=\"rfid-mod-tab\" data-toggle=\"tab\" href=\"#rfidmod\" role=\"tab\"><i class=\"fas fa-cog\"></i> Modifikation</a>\
                        </li>\
                    </ul>\
                    <div class=\"tab-content\" id=\"SubTabContent\">\
                        <div class=\"tab-pane show active\" id=\"rfidmusic\" role=\"tabpanel\">\
                            <br>\
                            <label for=\"fileOrUrl\">File, directory or URL (^ and # aren't allowed as chars)</label>\
                            <input type=\"text\" class=\"form-control\" id=\"fileOrUrl\" maxlength=\"255\" placeholder=\"z.B. /mp3/Hoerspiele/Yakari/Yakari_und_seine_Freunde.mp3\" pattern=\"^[^\\^#]+$\" name=\"fileOrUrl\" required>\
                            <label for=\"playMode\">Playmode</label>\
                            <select class=\"form-control\" id=\"playMode\" name=\"playMode\">\
                                <option class=\"placeholder\" disabled selected value=\"\">Select mode</option>\
                                <option class=\"option-file\" value=\"1\">Single track</option>\
                                <option class=\"option-file\" value=\"2\">Single track (loop)</option>\
                                <option class=\"option-file-and-folder\" value=\"3\">Audiobook</option>\
                                <option class=\"option-file-and-folder\" value=\"4\">Audiobook (loop)</option>\
                                <option class=\"option-folder\" value=\"5\">All tracks of a directory (sorted alph.)</option>\
                                <option class=\"option-folder\" value=\"6\">All tracks of a directory (random)</option>\
                                <option class=\"option-folder\" value=\"7\">All tracks of a directory (sorted alph., loop)</option>\
                                <option class=\"option-folder\" value=\"9\">All tracks of a directory (random, loop)</option>\
                                <option class=\"option-stream\" value=\"8\">Webradio</option>\
                            </select>\
                        </div>\
                        <div class=\"tab-pane \" id=\"rfidmod\" role=\"tabpanel\">\
                            <label for=\"modId\"></label>\
                            <select class=\"form-control\" id=\"modId\" name=\"modId\">\
                                <option class=\"placeholder\" disabled selected value=\"\">Select modification</option>\
                                <option value=\"100\">Keylock</option>\
                                <option value=\"101\">Sleep after 15 minutes</option>\
                                <option value=\"102\">Sleep after 30 minutes</option>\
                                <option value=\"103\">Sleep after 1 hour</option>\
                                <option value=\"104\">Sleep after 2 hours</option>\
                                <option value=\"105\">Sleep after end of track</option>\
                                <option value=\"106\">Sleep after end of playlist</option>\
                                <option value=\"107\">Sleep after end of five tracks</option>\
                                <option value=\"110\">Loop playlist</option>\
                                <option value=\"111\">Loop track</option>\
                                <option value=\"120\">Dimm LEDs (nightmode)</option>\
                                <option value=\"130\">Toggle WiFi</option>\
                                <option value=\"140\">Toggle Bluetooth</option>\
                                <option value=\"150\">Enable FTP</option>\
                                <option value=\"0\">Remove assignment</option>\
                            </select>\
                        </div>\
                    </div>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
            </fieldset>\
        </div>\
        <br />\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-mqtt\" role=\"tabpanel\" aria-labelledby=\"nav-mqtt-tab\">\
        <div class=\"container\" id=\"mqttConfig\">\
\
            <form class=\"needs-validation\" action=\"#mqttConfig\" method=\"POST\"\
                  onsubmit=\"mqttSettings('mqttConfig'); return false\">\
                <div class=\"form-check col-md-12\">\
                    <legend>MQTT-settings</legend>\
                    <input class=\"form-check-input\" type=\"checkbox\" value=\"1\" id=\"mqttEnable\" name=\"mqttEnable\" %MQTT_ENABLE%>\
                    <label class=\"form-check-label\" for=\"mqttEnable\">\
                        Enable MQTT\
                    </label>\
                </div>\
                <div class=\"form-group my-2 col-md-12\">\
                    <label for=\"mqttServer\">MQTT-server</label>\
                    <input type=\"text\" class=\"form-control\" id=\"mqttServer\" minlength=\"7\" maxlength=\"%MQTT_SERVER_LENGTH%\"\
                           placeholder=\"z.B. 192.168.2.89\" name=\"mqttServer\" value=\"%MQTT_SERVER%\">\
                    <label for=\"mqttUser\">MQTT-username (optional):</label>\
                    <input type=\"text\" class=\"form-control\" id=\"mqttUser\" maxlength=\"%MQTT_USER_LENGTH%\"\
                           placeholder=\"Benutzername\" name=\"mqttUser\" value=\"%MQTT_USER%\">\
                    <label for=\"mqttPwd\">MQTT-password (optional):</label>\
                    <input type=\"password\" class=\"form-control\" id=\"mqttPwd\" maxlength=\"%MQTT_PWD_LENGTH%\"\
                           placeholder=\"Passwort\" name=\"mqttPwd\" value=\"%MQTT_PWD%\">\
                    <label for=\"mqttPort\">MQTT-port:</label>\
                    <input type=\"number\" class=\"form-control\" id=\"mqttPort\" min=\"1\" max=\"65535\"\
                            placeholder=\"Port\" name=\"mqttPort\" value=\"%MQTT_PORT%\" required>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
        </div>\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-ftp\" role=\"tabpanel\" aria-labelledby=\"nav-ftp-tab\">\
        <div class=\"container\" id=\"ftpConfig\">\
\
            <form action=\"#ftpConfig\" method=\"POST\" onsubmit=\"ftpSettings('ftpConfig'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <legend>FTP-settings</legend>\
                    <label for=\"ftpUser\">FTP-username:</label>\
                    <input type=\"text\" class=\"form-control\" id=\"ftpUser\" maxlength=\"%FTP_USER_LENGTH%\"\
                           placeholder=\"Benutzername\" name=\"ftpUser\" value=\"%FTP_USER%\" required>\
                    <label for=\"pwd\">FTP-password:</label>\
                    <input type=\"password\" class=\"form-control\" id=\"ftpPwd\" maxlength=\"%FTP_PWD_LENGTH%\" placeholder=\"Passwort\"\
                           name=\"ftpPwd\" value=\"%FTP_PWD%\" required>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
        </div>\
    </div>\
\
    <div class=\"tab-pane fade\" id=\"nav-general\" role=\"tabpanel\" aria-labelledby=\"nav-general-tab\">\
        <div class=\"container\" id=\"generalConfig\">\
\
            <form action=\"#generalConfig\" method=\"POST\" onsubmit=\"genSettings('generalConfig'); return false\">\
                    <div class=\"form-group col-md-12\">\
                    <fieldset>\
                            <legend class=\"w-auto\">Volume</legend>\
                    <label for=\"initialVolume\">After restart</label>\
                        <div class=\"text-center\">\
                    <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i> <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"initialVolume\" name=\"initialVolume\"\
                        data-slider-value=\"%INIT_VOLUME%\"  value=\"%INIT_VOLUME%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i></div>\
                        <br>\
                    <label for=\"maxVolumeSpeaker\">Max. volume (speaker)</label>\
                        <div class=\"text-center\">\
                     <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i>   <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"maxVolumeSpeaker\" name=\"maxVolumeSpeaker\"\
                        data-slider-value=\"%MAX_VOLUME_SPEAKER%\"  value=\"%MAX_VOLUME_SPEAKER%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i>\
                        </div>\
                        <br>\
                    <label for=\"maxVolumeHeadphone\">Max. volume (headphones)</label>\
                        <div class=\"text-center\">\
                    <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i> <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"maxVolumeHeadphone\" name=\"maxVolumeHeadphone\"\
                         data-slider-value=\"%MAX_VOLUME_HEADPHONE%\" value=\"%MAX_VOLUME_HEADPHONE%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i>\
                        </div>\
                </fieldset>\
                </div>\
                <br>\
                <div class=\"form-group col-md-12\">\
                    <fieldset >\
                        <legend class=\"w-auto\">Neopixel (brightness)</legend>\
                    <label for=\"initBrightness\">After restart:</label>\
                        <div class=\"text-center\">\
                            <i class=\"far fa-sun fa-2x .icon-pos\"></i>\
                        <input data-provide=\"slider\" type=\"number\" data-slider-min=\"0\" data-slider-max=\"255\" min=\"0\" max=\"255\" class=\"form-control\" id=\"initBrightness\" name=\"initBrightness\"\
                               data-slider-value=\"%INIT_LED_BRIGHTNESS%\"  value=\"%INIT_LED_BRIGHTNESS%\" required><i class=\"fas fa-sun fa-2x .icon-pos\"></i>\
                        </div>\
\
                    <label for=\"nightBrightness\">For nightmode:</label>\
                        <div class=\"text-center\">\
                        <i class=\"far fa-sun fa-2x .icon-pos\"></i><input data-provide=\"slider\" type=\"number\" data-slider-min=\"0\" data-slider-max=\"255\" min=\"0\" max=\"255\" class=\"form-control\" id=\"nightBrightness\" name=\"nightBrightness\" data-slider-value=\"%NIGHT_LED_BRIGHTNESS%\" value=\"%NIGHT_LED_BRIGHTNESS%\" required><i class=\"fas fa-sun fa-2x .icon-pos\"></i>\
                        </div>\
                    </fieldset>\
                </div>\
<br>\
                <div class=\"form-group col-md-12\">\
                    <fieldset>\
                        <legend>Deep Sleep</legend>\
\
                        <label for=\"inactivityTime\">After n minutes inactivity</label>\
                        <div class=\"text-center\"><i class=\"fas fa-hourglass-start fa-2x .icon-pos\"></i> <input type=\"number\" data-provide=\"slider\" data-slider-min=\"0\" data-slider-max=\"30\" min=\"1\" max=\"120\" class=\"form-control\" id=\"inactivityTime\" name=\"inactivityTime\"\
                         data-slider-value=\"%MAX_INACTIVITY%\"  value=\"%MAX_INACTIVITY%\" required><i class=\"fas fa-hourglass-end fa-2x .icon-pos\"></i></div>\
                    </fieldset>\
                </div>\
<br>\
\
                <div class=\"form-group col-md-12\">\
                    <fieldset>\
                        <legend>Battery</legend>\
                    <div>Show voltage-status via Neopixel</div>\
                        <br>\
                    <label for=\"warningLowVoltage\">Show warning below this threshold.\
                    </label>\
                        <div class=\"text-center\">\
                        <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i> <input  data-provide=\"slider\"  data-slider-step=\"0.1\" data-slider-min=\"3.0\" data-slider-max=\"5.0\"  min=\"3.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"warningLowVoltage\" name=\"warningLowVoltage\"\
                        data-slider-value=\"%WARNING_LOW_VOLTAGE%\" value=\"%WARNING_LOW_VOLTAGE%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
<br>\
                    <label for=\"voltageIndicatorLow\">Lowest voltage, that is indicated by one LED\
                        </label>\
                        <div class=\"text-center\">\
                        <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i> <input data-provide=\"slider\" min=\"2.0\" data-slider-step=\"0.1\" data-slider-min=\"2.0\" data-slider-max=\"5.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"voltageIndicatorLow\" name=\"voltageIndicatorLow\"\
                          data-slider-value=\"%VOLTAGE_INDICATOR_LOW%\"  value=\"%VOLTAGE_INDICATOR_LOW%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
                        <br>\
                        <label for=\"voltageIndicatorHigh\">Voltage that is indicated by all LEDs</label>\
\
                            <div class=\"text-center\">\
                                <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i><input data-provide=\"slider\" data-slider-step=\"0.1\"  data-slider-min=\"2.0\" data-slider-max=\"5.0\" min=\"2.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"voltageIndicatorHigh\" name=\"voltageIndicatorHigh\"\
                           data-slider-value=\"%VOLTAGE_INDICATOR_HIGH%\"  value=\"%VOLTAGE_INDICATOR_HIGH%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
\
                        <br>\
                    <label for=\"voltageCheckInterval\">Interval between measurements (in minutes)</label>\
                        <div class=\"text-center\"><i class=\"fas fa-hourglass-start fa-2x .icon-pos\"></i>\
                            <input data-provide=\"slider\" data-slider-min=\"1\" data-slider-max=\"60\" type=\"number\" min=\"1\" max=\"60\" class=\"form-control\" id=\"voltageCheckInterval\"\
                                   data-slider-value=\"%VOLTAGE_CHECK_INTERVAL%\"  name=\"voltageCheckInterval\" value=\"%VOLTAGE_CHECK_INTERVAL%\" required><i class=\"fas fa-hourglass-end fa-2x .icon-pos\"></i>\
                    </div>\
\
                    </fieldset>\
                </div>\
<br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
        </div>\
        <br />\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-tools\" role=\"tabpanel\" aria-labelledby=\"nav-tools-tab\">\
        <div class=\"container\" id=\"importNvs\">\
            <legend>NVS-Importer</legend>\
            <form action=\"/upload\" enctype=\"multipart/form-data\" method=\"POST\">\
                <div class=\"form-group\">\
                    <label for=\"nvsUpload\">Backupfile can be imported right here.</label>\
                    <input type=\"file\" class=\"form-control-file\" id=\"nvsUpload\" name=\"nvsUpload\" accept=\".txt\">\
                </div>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
            </form>\
        </div>\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-forum\" role=\"tabpanel\" aria-labelledby=\"nav-forum-tab\">\
        <div class=\"container\" id=\"forum\">\
            <legend>Forum</legend>\
            <p>Having problems or aim to discuss about ESPuino?<br />\
                Join us at <a href=\"https://forum.espuino.de\" target=\"_blank\">ESPuino-Forum</a>! Especially there's a lot of (german)<br />\
                <a href=\"https://forum.espuino.de/c/dokumentation/anleitungen/10\" target=\"_blank\">documentation</a> online!\
            </p>\
        </div>\
    </div>\
</div>\
<script type=\"text/javascript\">\
    var DEBUG = false;\
    var lastIdclicked = '';\
    var ActiveSubTab = 'rfid-music-tab';\
    var host = $(location).attr('hostname');\
\
    /* show active / selected tab */\
    $('#SubTab.nav-tabs a').on('shown.bs.tab', function (e) {\
        ActiveSubTab = $(e.target).attr('id');\
        console.log ( ActiveSubTab);\
        if (ActiveSubTab == 'rfid-music-tab') {\
            document.getElementById(\"fileOrUrl\").required = true;\
        } else {\
            document.getElementById(\"fileOrUrl\").required = false;\
        }\
});\
\
    if (DEBUG) {\
        host = \"192.168.178.114\";\
    }\
\
    toastr.options = {\
        \"closeButton\": false,\
        \"debug\": false,\
        \"newestOnTop\": false,\
        \"progressBar\": false,\
        \"positionClass\": \"toast-top-right\",\
        \"preventDuplicates\": false,\
        \"onclick\": null,\
        \"showDuration\": \"300\",\
        \"hideDuration\": \"1000\",\
        \"timeOut\": \"5000\",\
        \"extendedTimeOut\": \"1000\",\
        \"showEasing\": \"swing\",\
        \"hideEasing\": \"linear\",\
        \"showMethod\": \"fadeIn\",\
        \"hideMethod\": \"fadeOut\"\
    };\
\
    function postRendering(event, data) {\
        Object.keys(data.instance._model.data).forEach(function (key, index) {\
\
            var cur = data.instance.get_node(data.instance._model.data[key]);\
            var lastFolder = cur['id'].split('/').filter(function (el) {\
                return el.trim().length > 0;\
            }).pop();\
            if ((/\\.(mp3|MP3|ogg|wav|WAV|OGG|wma|WMA|acc|ACC|flac|FLAC|m4a|M4A)$/i).test(lastFolder)) {\
                data.instance.set_type(data.instance._model.data[key], 'audio');\
            } else {\
                if (data.instance._model.data[key]['type'] == \"file\") {\
                    data.instance.disable_node(data.instance._model.data[key]);\
                }\
            }\
            data.instance.rename_node(data.instance._model.data[key], lastFolder);\
        });\
    }\
\
    /* File Explorer functions begin*/\
    var lastSelectedNodePath = \"\";\
\
    $('#explorerTree').on('select_node.jstree', function (e, data) {\
\
        $('input[name=fileOrUrl]').val(data.node.data.path);\
\
        if (ActiveSubTab !== 'rfid-music-tab') {\
            $('#SubTab.nav-tabs a[id=\"rfid-music-tab\"]').tab('show');\
        }\
\
        if (data.node.type == \"folder\") {\
            $('.option-folder').show();\
            $('.option-file').hide();\
            $('#playMode option').removeAttr('selected').filter('[value=3]').attr('selected', true);\
        }\
\
        if (data.node.type == \"audio\") {\
            $('.option-file').show();\
            $('.option-folder').hide();\
            $('#playMode option').removeAttr('selected').filter('[value=1]').attr('selected', true);\
        }\
\
        if(lastSelectedNodePath != data.node.data.path) {\
            if (data.node.data.directory) {\
\
                var ref = $('#explorerTree').jstree(true),\
                    sel = ref.get_selected();\
                if(!sel.length) { return false; }\
                sel = sel[0];\
                var children = $(\"#explorerTree\").jstree(\"get_children_dom\",sel);\
                /* refresh only, when there is no child -> possible not yet updated */\
                if(children.length < 1){\
                    refreshNode(sel);\
                }\
\
            }\
         lastSelectedNodePath = data.node.data.path;\
        }\
\
\
    });\
\
	function doRest(path, callback, obj) {\
		obj.url      = path;\
		obj.dataType = \"json\";\
		obj.contentType= \"application/json;charset=IBM437\",\
        obj.scriptCharset= \"IBM437\",\
        obj.success  = function(data, textStatus, jqXHR) {\
			if (callback) {\
				callback(data);\
			}\
		};\
		obj.error    = function(jqXHR, textStatus, errorThrown) {\
			console.log(\"AJAX error\");\
			/*debugger; */\
		};\
		jQuery.ajax(obj);\
	} /* doRest */\
\
	function getData(path, callback) {\
		doRest(path, callback, {\
			method   : \"GET\"\
		});\
	} /* getData */\
\
	function deleteData(path, callback, _data) {\
		doRest(path, callback, {\
			method   : \"DELETE\",\
			data: _data\
		});\
	} /* deleteData */\
\
	function patchData(path, callback, _data) {\
		doRest(path, callback, {\
			method   : \"PATCH\",\
			data: _data\
		});\
	} /* patchData */\
\
	function postData(path, callback, _data) {\
		doRest(path, callback, {\
			method   : \"POST\",\
			data: _data\
		});\
	} /* postData */\
\
\
	function putData(path, callback, _data) {\
		doRest(path, callback, {\
			method   : \"PUT\",\
			data: _data\
		});\
	} /* putData */\
\
\
	/* File Upload */\
	$('#explorerUploadForm').submit(function(e){\
		e.preventDefault();\
		console.log(\"Upload!\");\
		var data = new FormData(this);\
\
		var ref = $('#explorerTree').jstree(true),\
			sel = ref.get_selected(),\
			path = \"/\";\
        if(!sel.length) { alert(\"Please select the upload location!\");return false; }\
        if(!document.getElementById('uploaded_file').files.length > 0) { alert(\"Please select files to upload!\");return false; }\
		sel = sel[0];\
		selectedNode = ref.get_node(sel);\
		if(selectedNode.data.directory){\
			path = selectedNode.data.path\
		} else {\
			/* remap sel to parent folder */\
			sel = ref.get_node(ref.get_parent(sel));\
			path = parentNode.data.path;\
			console.log(\"Parent path: \" + path);\
		}\
\
		$.ajax({\
			url: '/explorer?path=' + path,\
            type: 'POST',\
            data: data,\
            contentType: false,\
            processData:false,\
			xhr: function() {\
				var xhr = new window.XMLHttpRequest();\
\
				xhr.upload.addEventListener(\"progress\", function(evt) {\
				  if (evt.lengthComputable) {\
					var percentComplete = evt.loaded / evt.total;\
					percentComplete = parseInt(percentComplete * 100);\
                    console.log(percentComplete);\
                    var percent = percentComplete + '%';\
                    $(\"#explorerUploadProgress\").css('width', percent).text(percent);\
				  }\
				}, false);\
\
				return xhr;\
			},\
			success: function(data, textStatus, jqXHR) {\
                console.log(\"Upload success!\");\
                $(\"#explorerUploadProgress\").text(\"Upload success!\");\
                document.getElementById('uploaded_file').value = '';\
                document.getElementById('uploaded_file_text').innerHTML = '';\
\
				getData(\"/explorer?path=\" + path, function(data) {\
					/* We now have data! */\
					deleteChildrenNodes(sel);\
					addFileDirectory(sel, data);\
					ref.open_node(sel);\
\
\
				});\
\
			}\
        });\
	});\
\
	/* File Delete */\
	function handleDeleteData(nodeId) {\
		var ref = $('#explorerTree').jstree(true);\
		var node = ref.get_node(nodeId);\
		console.log(\"call delete request: \" + node.data.path);\
		deleteData(\"/explorer?path=\" + node.data.path);\
	}\
\
	function fileNameSort( a, b ) {\
		if ( a.dir && !b.dir ) {\
			return -1\
		}\
		if ( !a.dir && b.dir ) {\
			return 1\
		}\
		if ( a.name < b.name ){\
			return -1;\
		}\
		if ( a.name > b.name ){\
			return 1;\
		}\
		return 0;\
	}\
\
	function createChild(nodeId, data) {\
		var ref = $('#explorerTree').jstree(true);\
		var node = ref.get_node(nodeId);\
		var parentNodePath = node.data.path;\
		/* In case of root node remove leading '/' to avoid '//' */\
		if(parentNodePath == \"/\"){\
			parentNodePath = \"\";\
		}\
		var child = {\
			text: data.name,\
			type: getType(data),\
			data: {\
				path: parentNodePath + \"/\" + data.name,\
				directory: data.dir\
			}\
		};\
\
		return child;\
\
	}\
\
	function deleteChildrenNodes(nodeId) {\
		var ref = $('#explorerTree').jstree(true);\
		var children = $(\"#explorerTree\").jstree(\"get_children_dom\",nodeId);\
		for(var i=0;i<children.length;i++)\
		{\
			ref.delete_node(children[i].id);\
		}\
\
	}\
\
	function refreshNode(nodeId) {\
\
		var ref = $('#explorerTree').jstree(true);\
\
		var node = ref.get_node(nodeId);\
\
		getData(\"/explorer?path=\" + node.data.path, function(data) {\
			/* We now have data! */\
\
			deleteChildrenNodes(nodeId);\
			addFileDirectory(nodeId, data);\
			ref.open_node(nodeId);\
\
		});\
\
\
	}\
\
	function getType(data) {\
		var type = \"\";\
\
		if(data.dir) {\
			type = \"folder\";\
		} else if ((/\\.(mp3|MP3|ogg|wav|WAV|OGG|wma|WMA|acc|ACC|flac|FLAC)$/i).test(data.name)) {\
			type = \"audio\";\
		} else {\
			type = \"file\";\
		}\
\
		return type;\
	}\
\
\
	function addFileDirectory(parent, data) {\
\
		data.sort( fileNameSort );\
		var ref = $('#explorerTree').jstree(true);\
\
		for (var i=0; i<data.length; i++) {\
			console.log(\"Create Node\");\
			ref.create_node(parent, createChild(parent, data[i]));\
		}\
	} /* addFileDirectory */\
\
	function buildFileSystemTree(path) {\
\
		$('#explorerTree').jstree({\
				\"core\" : {\
						\"check_callback\" : true,\
						'force_text' : true,\
						\"themes\" : { \"stripes\" : true },\
						'data' : {	text: '/',\
									state: {\
										opened: true\
									},\
									type: 'folder',\
									children: [],\
									data: {\
										path: '/',\
										directory: true\
								}}\
					},\
					'types': {\
						'folder': {\
							'icon': \"fa fa-folder\"\
						},\
						'file': {\
							'icon': \"fa fa-file\"\
						},\
						'audio': {\
							'icon': \"fa fa-file-audio\"\
						},\
						'default': {\
							'icon': \"fa fa-folder\"\
						}\
					},\
				plugins: [\"contextmenu\", \"themes\", \"types\"],\
				contextmenu: {\
					items: function(nodeId) {\
						var ref = $('#explorerTree').jstree(true);\
						var node = ref.get_node(nodeId);\
						var items = {};\
\
\
						if (node.data.directory) {\
							items.createDir = {\
								label: \"Neuer Ordner\",\
								action: function(x) {\
									var childNode = ref.create_node(nodeId, {text: \"Neuer Ordner\", type: \"folder\"});\
									if(childNode) {\
										ref.edit(childNode, null, function(childNode, status){\
											putData(\"/explorer?path=\" + node.data.path + \"/\" + childNode.text);\
											refreshNode(nodeId);\
										});\
									}\
								}\
							};\
						}\
\
						/* Play */\
						items.play = {\
							label: \"Abspielen\",\
							action: function(x) {\
								var playMode = node.data.directory?\"5\":\"1\";\
								postData(\"/exploreraudio?path=\" + node.data.path + \"&playmode=\" + playMode);\
							}\
						};\
\
						/* Refresh */\
						items.refresh = {\
							label: \"Aktualisieren\",\
							action: function(x) {\
								refreshNode(nodeId);\
							}\
						};\
\
						/* Delete */\
						items.delete = {\
							label: \"Löschen\",\
							action: function(x) {\
								handleDeleteData(nodeId);\
								refreshNode(ref.get_parent(nodeId));\
							}\
						};\
\
						/* Rename */\
						items.rename = {\
							label: \"Umbenennen\",\
							action: function(x) {\
								var srcPath = node.data.path;\
								ref.edit(nodeId, null, function(node, status){\
									node.data.path = node.data.path.substring(0,node.data.path.lastIndexOf(\"/\")+1) + node.text;\
									patchData(\"/explorer?srcpath=\" + srcPath + \"&dstpath=\" + node.data.path);\
									refreshNode(ref.get_parent(nodeId));\
								});\
							}\
						};\
\
					   return items;\
					}\
				}\
			});\
\
		if (path.length == 0) {\
			return;\
		}\
		getData(\"/explorer?path=/\", function(data) {\
			/* We now have data! */\
			$('#explorerTree').jstree(true).settings.core.data.children = [];\
\
			data.sort( fileNameSort );\
\
\
			for (var i=0; i<data.length; i++) {\
				var newChild = {\
					text: data[i].name,\
					type: getType(data[i]),\
					data: {\
						path: \"/\" + data[i].name,\
						directory: data[i].dir\
					},\
					children: []\
				};\
				$('#explorerTree').jstree(true).settings.core.data.children.push(newChild);\
			}\
\
			$(\"#explorerTree\").jstree(true).refresh();\
\
\
		});\
	} /* buildFileSystemTree */\
\
    /* File Explorer functions end */\
\
    var socket = undefined;\
    var tm;\
\
    function connect() {\
        socket = new WebSocket(\"ws://\" + host + \"/ws\");\
\
        socket.onopen = function () {\
            setInterval(ping, 15000);\
        };\
\
        socket.onclose = function (e) {\
            console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);\
            socket = null;\
            setTimeout(function () {\
                connect();\
            }, 5000);\
        };\
\
        socket.onerror = function (err) {\
            console.error('Socket encountered error: ', err.message, 'Closing socket');\
        };\
\
        socket.onmessage = function(event) {\
          console.log(event.data);\
          var socketMsg = JSON.parse(event.data);\
          if (socketMsg.rfidId != null) {\
              document.getElementById('rfidIdMusic').value = socketMsg.rfidId;\
              toastr.info(\"RFID Tag mit \"+ socketMsg.rfidId + \" erkannt.\" );\
              $(\"#rfidIdMusic\").effect(\"highlight\", {color:\"#abf5af\"}, 3000);\
          } if (\"status\" in socketMsg) {\
              if (socketMsg.status == \"ok\") {\
                  toastr.success(\"Aktion erfolgreich ausgeführt.\" );\
              }\
          } if (\"pong\" in socketMsg) {\
              if (socketMsg.pong == 'pong') {\
                  pong();\
              }\
          }\
      };\
    }\
\
    function ping() {\
        var myObj = {\
            \"ping\": {\
                ping: 'ping'\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
        tm = setTimeout(function () {\
            toastr.warning('Die Verbindung zum ESPuino ist unterbrochen! Bitte Seite neu laden.');\
        }, 5000);\
    }\
\
    function pong() {\
        clearTimeout(tm);\
    }\
\
    function genSettings(clickedId) {\
        lastIdclicked = clickedId;\
        var myObj = {\
            \"general\": {\
                iVol: document.getElementById('initialVolume').value,\
                mVolSpeaker: document.getElementById('maxVolumeSpeaker').value,\
                mVolHeadphone: document.getElementById('maxVolumeHeadphone').value,\
                iBright: document.getElementById('initBrightness').value,\
                nBright: document.getElementById('nightBrightness').value,\
                iTime: document.getElementById('inactivityTime').value,\
                vWarning: document.getElementById('warningLowVoltage').value,\
                vIndLow: document.getElementById('voltageIndicatorLow').value,\
                vIndHi: document.getElementById('voltageIndicatorHigh').value,\
                vInt: document.getElementById('voltageCheckInterval').value\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
    function ftpSettings(clickedId) {\
        lastIdclicked = clickedId;\
        var myObj = {\
            \"ftp\": {\
                ftpUser: document.getElementById('ftpUser').value,\
                ftpPwd: document.getElementById('ftpPwd').value\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
    function mqttSettings(clickedId) {\
        lastIdclicked = clickedId;\
        var val;\
        if (document.getElementById('mqttEnable').checked) {\
            val = document.getElementById('mqttEnable').value;\
        } else {\
            val = 0;\
        }\
        var myObj = {\
            \"mqtt\": {\
                mqttEnable: val,\
                mqttServer: document.getElementById('mqttServer').value,\
                mqttUser: document.getElementById('mqttUser').value,\
                mqttPwd: document.getElementById('mqttPwd').value,\
                mqttPort: document.getElementById('mqttPort').value\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
    function removeTrSlash(str) {\
        if (str.substr(-1) === '/') {\
            return str.substr(0, str.length - 1);\
        }\
        return str;\
    }\
\
    function rfidAssign(clickedId) {\
        lastIdclicked = clickedId;\
        if (ActiveSubTab == 'rfid-music-tab') {\
            var myObj = {\
                \"rfidAssign\": {\
                    rfidIdMusic: document.getElementById('rfidIdMusic').value,\
                    fileOrUrl: removeTrSlash(document.getElementById('fileOrUrl').value),\
                    playMode: document.getElementById('playMode').value\
                }\
            };\
        } else {\
            var myObj = {\
                \"rfidMod\": {\
                    rfidIdMod: document.getElementById('rfidIdMusic').value,\
                    modId: document.getElementById('modId').value\
                }\
            };\
        }\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
    function wifiConfig(clickedId) {\
        lastIdclicked = clickedId;\
        var myObj = {\
            \"wifiConfig\": {\
                ssid: document.getElementById('ssid').value,\
                pwd: document.getElementById('pwd').value,\
                hostname: document.getElementById('hostname').value\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
    function sendControl(cmd) {\
        var myObj = {\
            \"controls\": {\
                action: cmd\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
    function sendVolume(vol) {\
        var myObj = {\
            \"controls\": {\
                set_volume: vol\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
    $(document).ready(function () {\
        connect();\
		buildFileSystemTree(\"/\");\
\
        console.log(parseInt(document.getElementById('warningLowVoltage').value));\
        $(function () {\
            $('[data-toggle=\"tooltip\"]').tooltip();\
        });\
\
    });\
</script>\
</body>\
</html>";