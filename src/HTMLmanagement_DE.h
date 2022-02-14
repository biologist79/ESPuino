static const char management_HTML[] PROGMEM = "<!DOCTYPE html>\
<html lang=\"de\">\
<head>\
    <title>ESPuino-Konfiguration</title>\
    <meta charset=\"utf-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
    <link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon\">\
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
        .coverimage-container {\
            width: 80%%;\
            height:15%%;\
            margin-left: 1em;\
            margin-right: 1em;\
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
        <img src=\"/logo\"\
             width=\"35\" height=\"35\" class=\"d-inline-block align-top\" alt=\"\"/>\
        ESPuino\
    </a>\
    <a class=\"reboot float-right nav-link\" href=\"/restart\"><i class=\"fas fa-redo\"></i> Neustart</a>\
    <a class=\"reboot float-right nav-link\" href=\"/shutdown\"><i class=\"fas fa-power-off\"></i> Ausschalten</a>\
    <a class=\"reboot float-right nav-link\" href=\"/log\"><i class=\"fas fa-book\"></i> Log</a>\
    <a class=\"reboot float-right nav-link\" href=\"/info\"><i class=\"fas fa-info\"></i> Infos</a>\
    </div>\
</nav>\
<br/>\
    <nav>\
        <div class=\"container nav nav-tabs\" id=\"nav-tab\" role=\"tablist\">\
            <a class=\"nav-item nav-link\" id=\"nav-control-tab\" data-toggle=\"tab\" href=\"#nav-control\" role=\"tab\" aria-controls=\"nav-control\" aria-selected=\"false\"><i class=\"fas fa-gamepad\"></i><span class=\".d-sm-none .d-md-block\"> Steuerung</span></a>\
            <a class=\"nav-item nav-link active\" id=\"nav-rfid-tab\" data-toggle=\"tab\" href=\"#nav-rfid\" role=\"tab\" aria-controls=\"nav-rfid\" aria-selected=\"true\"><i class=\"fas fa-dot-circle\"></i> RFID</a>\
            <a class=\"nav-item nav-link\" id=\"nav-wifi-tab\" data-toggle=\"tab\" href=\"#nav-wifi\" role=\"tab\" aria-controls=\"nav-wifi\" aria-selected=\"false\"><i class=\"fas fa-wifi\"></i><span class=\".d-sm-none .d-md-block\"> WLAN</span></a>\
            %SHOW_MQTT_TAB%\
            %SHOW_FTP_TAB%\
            <a class=\"nav-item nav-link\" id=\"nav-general-tab\" data-toggle=\"tab\" href=\"#nav-general\" role=\"tab\" aria-controls=\"nav-general\" aria-selected=\"false\"><i class=\"fas fa-sliders-h\"></i> Allgemein</a>\
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
                    <legend>WLAN-Einstellungen</legend>\
                    <label for=\"ssid\">WLAN-Name (SSID):</label>\
                    <input type=\"text\" class=\"form-control\" id=\"ssid\" placeholder=\"SSID\" name=\"ssid\" required>\
                    <div class=\"invalid-feedback\">\
                        Bitte SSID des WLANs eintragen.\
                    </div>\
                    <label for=\"pwd\">Passwort:</label>\
                    <input type=\"password\" class=\"form-control\" id=\"pwd\" placeholder=\"Passwort\" name=\"pwd\" required>\
                    <label for=\"hostname\">ESPuino-Name (Hostname):</label>\
                    <input type=\"text\" class=\"form-control\" id=\"hostname\" placeholder=\"espuino\" name=\"hostname\"\
                           value=\"%HOSTNAME%\" pattern=\"^[^-\\.]{2,32}\" required>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Absenden</button>\
                </div>\
            </form>\
        </div>\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-control\" role=\"tabpanel\" aria-labelledby=\"nav-control-tab\">\
        <div class=\"container\" id=\"navControl\">\
                <div class=\"form-group col-md-12\">\
                    <legend>Steuerung</legend>\
                    <div class=\"form-group col-md-12\">\
                        <img src=\"/cover\" class=\"coverimage-container\" id=\"coverimg\" alt=\"\">\
                    </div>\
                    <div class=\"buttons\">\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" id=\"nav-btn-first\" onclick=\"sendControl(173)\">\
                            <span class=\"fas fa-fast-backward\"></span>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" id=\"nav-btn-prev\" onclick=\"sendControl(171)\">\
                            <span class=\"fas fa-backward\"></span>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" id=\"nav-btn-play\" onclick=\"sendControl(170)\">\
                            <i id=\"ico-play-pause\" class=\"fas fa-lg fa-play\"></i>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" id=\"nav-btn-next\" onclick=\"sendControl(172)\">\
                            <span class=\"fas fa-forward\"></span>\
                        </button>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" id=\"nav-btn-last\" onclick=\"sendControl(174)\">\
                            <span class=\"fas fa-fast-forward\"></span>\
                        </button>\
                    </div>\
                </div>\
                <br>\
                <div class=\"form-group col-md-12\">\
                    <legend>Lautst&auml;rke</legend>\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" onclick=\"sendControl(177)\">\
                            <span class=\"fas fa-volume-down\"></span>\
                        </button>\
                        </i><input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"setVolume\"\
                            data-slider-value=\"%CURRENT_VOLUME%\" value=\"%CURRENT_VOLUME%\" onchange=\"sendVolume(this.value)\">\
                        <button type=\"button\" class=\"btn btn-default btn-lg\" onclick=\"sendControl(176)\">\
                            <span class=\"fas fa-volume-up\"></span>\
                        </button>\
                </div>\
                <br/>\
                <div class=\"form-group col-md-12\">\
                    <legend>Aktueller Titel</legend>\
                    <div id=\"track\"></div>\
                </div>\
        </div>\
    </div>\
    <div class=\"tab-pane fade show active\" id=\"nav-rfid\" role=\"tabpanel\" aria-labelledby=\"nav-rfid-tab\">\
        <div class=\"container\" id=\"filetreeContainer\">\
            <fieldset>\
                <legend>Dateien</legend>\
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
                <legend>RFID-Zuweisungen</legend>\
            <form action=\"#rfidMusicTags\" method=\"POST\" onsubmit=\"rfidAssign('rfidMusicTags'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <label for=\"rfidIdMusic\">RFID-Chip-Nummer (12-stellig)</label>\
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
                            <label for=\"fileOrUrl\">Datei, Verzeichnis oder URL (^ und # als Zeichen nicht erlaubt)</label>\
                            <input type=\"text\" class=\"form-control\" id=\"fileOrUrl\" maxlength=\"255\" placeholder=\"z.B. /mp3/Hoerspiele/Yakari/Yakari_und_seine_Freunde.mp3\" pattern=\"^[^\\^#]+$\" name=\"fileOrUrl\" required>\
                            <label for=\"playMode\">Abspielmodus</label>\
                            <select class=\"form-control\" id=\"playMode\" name=\"playMode\">\
                                <option class=\"placeholder\" disabled selected value=\"\">Modus auswählen</option>\
                                <option class=\"option-file\" value=\"1\">Einzelner Titel</option>\
                                <option class=\"option-file\" value=\"2\">Einzelner Titel (Endlosschleife)</option>\
                                <option class=\"option-file-and-folder\" value=\"3\">Hörbuch</option>\
                                <option class=\"option-file-and-folder\" value=\"4\">Hörbuch (Endlosschleife)</option>\
                                <option class=\"option-folder\" value=\"5\">Alle Titel eines Verzeichnis (sortiert)</option>\
                                <option class=\"option-folder\" value=\"6\">Alle Titel eines Verzeichnis (zufällig)</option>\
                                <option class=\"option-folder\" value=\"7\">Alle Titel eines Verzeichnis (sortiert, Endlosschleife)</option>\
                                <option class=\"option-folder\" value=\"9\">Alle Titel eines Verzeichnis (zufällig, Endlosschleife)</option>\
                                <option class=\"option-stream\" value=\"8\">Webradio</option>\
                                <option class=\"option-stream\" value=\"11\">Liste (Dateien von SD und/oder Webstreams) aus lokaler .m3u-Datei</option>\
                            </select>\
                        </div>\
                        <div class=\"tab-pane \" id=\"rfidmod\" role=\"tabpanel\">\
                            <label for=\"modId\"></label>\
                            <select class=\"form-control\" id=\"modId\" name=\"modId\">\
                                <option class=\"placeholder\" disabled selected value=\"\">Modifikation auswählen</option>\
                                <option value=\"100\">Tastensperre</option>\
                                <option value=\"179\">Schlafe sofort</option>\
                                <option value=\"101\">Schlafen nach 15 Minuten</option>\
                                <option value=\"102\">Schlafen nach 30 Minuten</option>\
                                <option value=\"103\">Schlafen nach 1 Stunde</option>\
                                <option value=\"104\">Schlafen nach 2 Stunden</option>\
                                <option value=\"105\">Schlafen nach Ende des Titels</option>\
                                <option value=\"106\">Schlafen nach Ende der Playlist</option>\
                                <option value=\"107\">Schlafen nach fünf Titeln</option>\
                                <option value=\"110\">Wiederhole Playlist (endlos)</option>\
                                <option value=\"111\">Wiederhole Titel (endlos)</option>\
                                <option value=\"120\">Dimme LEDs (Nachtmodus)</option>\
                                <option value=\"130\">Aktiviere/deaktive WLAN</option>\
                                <option value=\"140\">Aktiviere/deaktiviere Bluetooth</option>\
                                <option value=\"150\">Aktiviere FTP</option>\
                                <option value=\"0\">Lösche Zuordnung</option>\
                                <option value=\"170\">Play/Pause</option>\
                                <option value=\"171\">Vorheriger Titel</option>\
                                <option value=\"172\">Nächster Titel</option>\
                                <option value=\"173\">Erster Titel</option>\
                                <option value=\"174\">Letzter Titel</option>\
                                <option value=\"180\">Springe vorwärts (n Sekunden)</option>\
                                <option value=\"181\">Springe rückwärts (n Sekunden)</option>\
                            </select>\
                        </div>\
                    </div>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Absenden</button>\
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
                    <legend>MQTT-Einstellungen</legend>\
                    <input class=\"form-check-input\" type=\"checkbox\" value=\"1\" id=\"mqttEnable\" name=\"mqttEnable\" %MQTT_ENABLE%>\
                    <label class=\"form-check-label\" for=\"mqttEnable\">\
                        MQTT aktivieren\
                    </label>\
                </div>\
                <div class=\"form-group my-2 col-md-12\">\
                    <label for=\"mqttServer\">MQTT-Server</label>\
                    <input type=\"text\" class=\"form-control\" id=\"mqttServer\" minlength=\"7\" maxlength=\"%MQTT_SERVER_LENGTH%\"\
                           placeholder=\"z.B. 192.168.2.89\" name=\"mqttServer\" value=\"%MQTT_SERVER%\">\
                    <label for=\"mqttUser\">MQTT-Benutzername (optional):</label>\
                    <input type=\"text\" class=\"form-control\" id=\"mqttUser\" maxlength=\"%MQTT_USER_LENGTH%\"\
                           placeholder=\"Benutzername\" name=\"mqttUser\" value=\"%MQTT_USER%\">\
                    <label for=\"mqttPwd\">MQTT-Passwort (optional):</label>\
                    <input type=\"password\" class=\"form-control\" id=\"mqttPwd\" maxlength=\"%MQTT_PWD_LENGTH%\"\
                           placeholder=\"Passwort\" name=\"mqttPwd\" value=\"%MQTT_PWD%\">\
                    <label for=\"mqttPort\">MQTT-Port:</label>\
                    <input type=\"number\" class=\"form-control\" id=\"mqttPort\" min=\"1\" max=\"65535\"\
                            placeholder=\"Port\" name=\"mqttPort\" value=\"%MQTT_PORT%\" required>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Absenden</button>\
                </div>\
            </form>\
        </div>\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-ftp\" role=\"tabpanel\" aria-labelledby=\"nav-ftp-tab\">\
        <div class=\"container\" id=\"ftpConfig\">\
\
            <form action=\"#ftpConfig\" method=\"POST\" onsubmit=\"ftpSettings('ftpConfig'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <legend>FTP-Einstellungen</legend>\
                    <label for=\"ftpUser\">FTP-Benutzername:</label>\
                    <input type=\"text\" class=\"form-control\" id=\"ftpUser\" maxlength=\"%FTP_USER_LENGTH%\"\
                           placeholder=\"Benutzername\" name=\"ftpUser\" value=\"%FTP_USER%\" required>\
                    <label for=\"pwd\">FTP-Passwort:</label>\
                    <input type=\"password\" class=\"form-control\" id=\"ftpPwd\" maxlength=\"%FTP_PWD_LENGTH%\" placeholder=\"Passwort\"\
                           name=\"ftpPwd\" value=\"%FTP_PWD%\" required>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Absenden</button>\
                </div>\
            </form>\
        </div>\
        <div class=\"container\" id=\"ftpStart\">\
            <div class=\"form-group col-md-12\">\
                <legend>FTP-Server starten</legend>\
                <div>\
                    Aktiviert den FTP-Server bis zum Neustart des Geräts.\
                </div>\
                <br>\
                <div class=\"text-center\">\
                    <button type=\"button\" class=\"btn btn-primary\" onclick=\"ftpStart()\">FTP-Server starten</button>\
                </div>\
            </div>\
        </div>\
    </div>\
\
    <div class=\"tab-pane fade\" id=\"nav-general\" role=\"tabpanel\" aria-labelledby=\"nav-general-tab\">\
        <div class=\"container\" id=\"generalConfig\">\
\
            <form action=\"#generalConfig\" method=\"POST\" onsubmit=\"genSettings('generalConfig'); return false\">\
                    <div class=\"form-group col-md-12\">\
                    <fieldset>\
                            <legend class=\"w-auto\">Lautstärke</legend>\
                    <label for=\"initialVolume\">Nach dem Einschalten</label>\
                        <div class=\"text-center\">\
                    <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i> <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"initialVolume\" name=\"initialVolume\"\
                        data-slider-value=\"%INIT_VOLUME%\"  value=\"%INIT_VOLUME%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i></div>\
                        <br>\
                    <label for=\"maxVolumeSpeaker\">Maximal (Lautsprecher)</label>\
                        <div class=\"text-center\">\
                     <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i>   <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"maxVolumeSpeaker\" name=\"maxVolumeSpeaker\"\
                        data-slider-value=\"%MAX_VOLUME_SPEAKER%\"  value=\"%MAX_VOLUME_SPEAKER%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i>\
                        </div>\
                        <br>\
                    <label for=\"maxVolumeHeadphone\">Maximal (Kopfhörer)</label>\
                        <div class=\"text-center\">\
                    <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i> <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"maxVolumeHeadphone\" name=\"maxVolumeHeadphone\"\
                         data-slider-value=\"%MAX_VOLUME_HEADPHONE%\" value=\"%MAX_VOLUME_HEADPHONE%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i>\
                        </div>\
                </fieldset>\
                </div>\
                <br>\
                <div class=\"form-group col-md-12\">\
                    <fieldset >\
                        <legend class=\"w-auto\">Neopixel (Helligkeit)</legend>\
                    <label for=\"initBrightness\">Nach dem Einschalten:</label>\
                        <div class=\"text-center\">\
                            <i class=\"far fa-sun fa-2x .icon-pos\"></i>\
                        <input data-provide=\"slider\" type=\"number\" data-slider-min=\"0\" data-slider-max=\"255\" min=\"0\" max=\"255\" class=\"form-control\" id=\"initBrightness\" name=\"initBrightness\"\
                               data-slider-value=\"%INIT_LED_BRIGHTNESS%\"  value=\"%INIT_LED_BRIGHTNESS%\" required><i class=\"fas fa-sun fa-2x .icon-pos\"></i>\
                        </div>\
\
                    <label for=\"nightBrightness\">Im Nachtmodus</label>\
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
                        <label for=\"inactivityTime\">Inaktivität nach (in Minuten)</label>\
                        <div class=\"text-center\"><i class=\"fas fa-hourglass-start fa-2x .icon-pos\"></i> <input type=\"number\" data-provide=\"slider\" data-slider-min=\"0\" data-slider-max=\"30\" min=\"1\" max=\"120\" class=\"form-control\" id=\"inactivityTime\" name=\"inactivityTime\"\
                         data-slider-value=\"%MAX_INACTIVITY%\"  value=\"%MAX_INACTIVITY%\" required><i class=\"fas fa-hourglass-end fa-2x .icon-pos\"></i></div>\
                    </fieldset>\
                </div>\
<br>\
\
                <div class=\"form-group col-md-12\">\
                    <fieldset>\
                        <legend>Batterie</legend>\
                    <div>Status über Neopixel anzeigen</div>\
                        <br>\
                    <label for=\"warningLowVoltage\">Unter dieser Spannung wird eine Warnung angezeigt.\
                    </label>\
                        <div class=\"text-center\">\
                        <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i> <input  data-provide=\"slider\"  data-slider-step=\"0.1\" data-slider-min=\"3.0\" data-slider-max=\"5.0\"  min=\"3.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"warningLowVoltage\" name=\"warningLowVoltage\"\
                        data-slider-value=\"%WARNING_LOW_VOLTAGE%\" value=\"%WARNING_LOW_VOLTAGE%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
<br>\
                    <label for=\"voltageIndicatorLow\">Eine LED leuchtet bei dieser Spannung\
                        </label>\
                        <div class=\"text-center\">\
                        <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i> <input data-provide=\"slider\" min=\"2.0\" data-slider-step=\"0.1\" data-slider-min=\"2.0\" data-slider-max=\"5.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"voltageIndicatorLow\" name=\"voltageIndicatorLow\"\
                          data-slider-value=\"%VOLTAGE_INDICATOR_LOW%\"  value=\"%VOLTAGE_INDICATOR_LOW%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
                        <br>\
                        <label for=\"voltageIndicatorHigh\">Alle LEDs leuchten bei dieser Spannung</label>\
\
                            <div class=\"text-center\">\
                                <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i><input data-provide=\"slider\" data-slider-step=\"0.1\"  data-slider-min=\"2.0\" data-slider-max=\"5.0\" min=\"2.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"voltageIndicatorHigh\" name=\"voltageIndicatorHigh\"\
                           data-slider-value=\"%VOLTAGE_INDICATOR_HIGH%\"  value=\"%VOLTAGE_INDICATOR_HIGH%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
\
                        <br>\
                    <label for=\"voltageCheckInterval\">Zeitabstand der Messung (in Minuten)</label>\
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
                <button type=\"submit\" class=\"btn btn-primary\">Absenden</button>\
                </div>\
            </form>\
        </div>\
        <br />\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-tools\" role=\"tabpanel\" aria-labelledby=\"nav-tools-tab\">\
        <div class=\"container\" id=\"eraseNvs\">\
            <legend>NVS RFID-Zuweisungen löschen</legend>\
            <form action=\"/rfidnvserase\" enctype=\"multipart/form-data\" method=\"GET\">\
                <div class=\"form-group\">\
                    <p>Über den Importer werden lediglich neue Einträge importiert, jedoch keine bestehenden Einträge aktiv gelöscht. Im Falle einer doppelten Zuweisung wird ein Eintrag allenfalls überschrieben. Mit dieser Funktion können alle bestehenden NVS-RFID-Zuweisungen gelöscht werden, so dass der ESPuino im Anschluss keinerlei Karten mehr kennt. Wird im Anschluss der Importer gestartet, befinden sich im Speicher des ESPuino anschließend exakt nur solche Zuweisungen, die Teil der Backup-Datei sind. Weitere Infos gibt es <a href=\"https://forum.espuino.de/t/die-backupfunktion-des-espuino/508\" target=\"_blank\">hier</a>.</p>\
                </div>\
                <button type=\"submit\" class=\"btn btn-primary\">Zuweisungen löschen</button>\
            </form>\
        </div>\
        <br />\
        <br />\
        <div class=\"container\" id=\"importNvs\">\
            <legend>NVS RFID-Importer</legend>\
            <form action=\"/upload\" enctype=\"multipart/form-data\" method=\"POST\">\
                <div class=\"form-group\">\
                    <label for=\"nvsUpload\">Hier kann eine Backup-Datei hochgeladen werden, um NVS-RFID-Zuweisungen zu importieren.</label>\
                    <input type=\"file\" class=\"form-control-file\" id=\"nvsUpload\" name=\"nvsUpload\" accept=\".txt\">\
                </div>\
                <button type=\"submit\" class=\"btn btn-primary\">Absenden</button>\
            </form>\
        </div>\
        <br />\
        <br />\
        <div class=\"container\" id=\"httpUpdate\">\
            <legend>Firmware-Update</legend>\
            <form action=\"/update\" enctype=\"multipart/form-data\" method=\"POST\">\
                <div class=\"form-group\">\
                    <label for=\"firmwareUpload\">Hier kann ein Firmware-Update durchgeführt werden.</label>\
                    <input type=\"file\" class=\"form-control-file\" id=\"firmwareUpload\" name=\"firmwareUpload\" accept=\".bin\">\
                </div>\
                <button type=\"submit\" class=\"btn btn-primary\">Absenden</button>\
            </form>\
        </div>\
    </div>\
    <br />\
    <br />\
    <div class=\"tab-pane fade\" id=\"nav-forum\" role=\"tabpanel\" aria-labelledby=\"nav-forum-tab\">\
        <div class=\"container\" id=\"forum\">\
            <legend>Forum</legend>\
            <p>Du hast Probleme mit ESPuino oder bist an einem Erfahrungsaustausch interessiert?<br />\
                Dann schaue doch mal im <a href=\"https://forum.espuino.de\" target=\"_blank\">ESPuino-Forum</a> vorbei! Insbesondere gibt es dort auch einen<br />\
                <a href=\"https://forum.espuino.de/c/dokumentation/anleitungen/10\" target=\"_blank\">Bereich</a>, in dem reichlich Dokumentation hinterlegt ist. Wir freuen uns auf deinen Besuch!\
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
        \"onclick\": null,\
        \"showDuration\": \"300\",\
        \"hideDuration\": \"1000\",\
        \"timeOut\": \"5000\",\
        \"extendedTimeOut\": \"1000\",\
        \"showEasing\": \"swing\",\
        \"hideEasing\": \"linear\",\
        \"showMethod\": \"fadeIn\",\
        \"hideMethod\": \"fadeOut\",\
        \"preventDuplicates\": true,\
        \"preventOpenDuplicates\": true\
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
            } else \
            if ((/\\.(png|PNG|jpg|JPG|jpeg|JPEG|bmp|BMP|gif|GIF)$/i).test(lastFolder)) {\
                data.instance.set_type(data.instance._model.data[key], 'image');\
                data.instance.disable_node(data.instance._model.data[key]);\
            } else {\
             if (data.instance._model.data[key]['type'] == \"file\") {\
                    data.instance.disable_node(data.instance._model.data[key]);\
                }\
            }\
            data.instance.rename_node(data.instance._model.data[key], lastFolder);\
        });\
    }\
\
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
                var lastNow = new Date().getTime();\
                startTime = new Date().getTime();\
                var lastKBytes = 0;\
                bytesTotal = 0;\
				var xhr = new window.XMLHttpRequest();\
\
				xhr.upload.addEventListener(\"progress\", function(evt) {\
				  if (evt.lengthComputable) {\
                    var now = new Date().getTime();  \
                    bytesTotal = evt.total;\
					var percentComplete = evt.loaded / evt.total;\
					percentComplete = parseInt(percentComplete * 100);\
                    kbytes = evt.loaded / 1024;\
                    var uploadedkBytes = kbytes - lastKBytes;\
                    var elapsed = (now - lastNow) / 1000;\
                    elapsed_total = (now - startTime) / 1000;\
                    var kbps = elapsed ? uploadedkBytes / elapsed : 0;\
                    lastKBytes = kbytes;\
                    lastNow = now;\
\
                    var seconds_elapsed =  (now - startTime) / 1000;\
                    var bytes_per_second =  seconds_elapsed ? evt.loaded / seconds_elapsed : 0;\
                    var Kbytes_per_second = bytes_per_second / 1024;\
                    var remaining_bytes =   evt.total - evt.loaded;                    \
                    var seconds_remaining = seconds_elapsed ? (remaining_bytes / bytes_per_second) : 'wird berechnet..' ;\
\
                    var percent = percentComplete + '%';\
                    console.log(\"Percent: \" + percent + \", \" + Kbytes_per_second.toFixed(2) + \" KB/s\");\
                    var progressText = percent;\
                    if (seconds_remaining > 60) {\
                        progressText = progressText + \", \" + (seconds_remaining / 60).toFixed(0) + \" Min. verbleibend..\";\
                    } else\
                    if (seconds_remaining <= 0) {\
                        progressText = progressText + \" (\" + Kbytes_per_second.toFixed(2) + \" KB/s)\";\
                    } else\
                     if (seconds_remaining < 2) {\
                        progressText = progressText + \", wenige Sek. verbleibend..\";\
                    } else {\
                        progressText = progressText + \", \" + seconds_remaining.toFixed(0) + \" Sek. verbleibend..\";\
                    }\
                    $(\"#explorerUploadProgress\").css('width', percent).text(progressText);\
				  }\
				}, false);\
\
				return xhr;\
			},\
			success: function(data, textStatus, jqXHR) {\
                var now = new Date().getTime();  \
                var elapsed_time = (now - startTime);\
                var seconds_elapsed =  elapsed_time / 1000;\
                var bytes_per_second =  seconds_elapsed ? bytesTotal / seconds_elapsed : 0;\
                var Kbytes_per_second = bytes_per_second / 1024 ;\
                var minutes = Math.floor(seconds_elapsed / 60);\
                var seconds = seconds_elapsed - (minutes * 60);\
                var timeText = minutes.toString().padStart(2, '0') + ':' + seconds.toFixed(0).toString().padStart(2, '0');\
                console.log(\"Upload success (\" + timeText + \", \" + Kbytes_per_second.toFixed(2) + \" KB/s): \" + textStatus);\
                var progressText = \"Upload abgeschlossen (\" + timeText + \", \" + Kbytes_per_second.toFixed(2) + \"  KB/s).\";\
                $(\"#explorerUploadProgress\").text(progressText);\
                document.getElementById('uploaded_file').value = '';\
                document.getElementById('uploaded_file_text').innerHTML = '';\
\
				getData(\"/explorer?path=\" + path, function(data) {\
					/* We now have data! */\
					deleteChildrenNodes(sel);\
					addFileDirectory(sel, data);\
					ref.open_node(sel);\
				});\
\
			},\
            error: function(request, status, error) {\
                console.log(\"Upload ERROR!\");\
                $(\"#explorerUploadProgress\").text(\"Upload-Fehler: \" + status);\
                toastr.error(\"Upload-Fehler: \" + status);\
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
		} else if ((/\\.(mp3|MP3|ogg|wav|WAV|OGG|wma|WMA|acc|ACC|m4a|M4A|flac|FLAC)$/i).test(data.name)) {\
			type = \"audio\";\
		} else if ((/\\.(png|PNG|jpg|JPG|jpeg|JPEG|bmp|BMP|gif|GIF)$/i).test(data.name)) {\
			type = \"image\";\
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
                        'strings' : { \"Loading ...\" : \"wird geladen...\" },\
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
						'image': {\
							'icon': \"fa fa-file-image\"\
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
								var playMode = 1;\
                                if (node.data.directory) {\
                                    playMode = 5;\
                                } else {\
                                    if ((/\\.(m3u|M3U)$/i).test(node.data.path)) {\
                                        playMode = 11;\
                                    }\
                                }\
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
						/* Download */\
                        if (!node.data.directory) {\
						    items.download = {\
							    label: \"Download\",\
							    action: function(x) {\
								    uri = \"/explorerdownload?path=\" + encodeURIComponent(node.data.path);\
                                    console.log(\"download file: \" + node.data.path);\
                                    var anchor = document.createElement('a');\
                                    anchor.href = uri;\
                                    anchor.target = '_blank';\
                                    anchor.download = node.data.path;\
                                    anchor.click();\
                                    document.body.removeChild(document.body.lastElementChild);\
							    }\
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
    var volumeSlider = new Slider(\"#setVolume\");\
    document.getElementById('setVolume').remove();\
\
    function connect() {\
        socket = new WebSocket(\"ws://\" + host + \"/ws\");\
\
        socket.onopen = function () {\
            setInterval(ping, 15000);\
            getTrack();\
            getCoverimage();\
        };\
\
        socket.onclose = function (e) {\
            console.log('Socket is geschlossen. Neuer Versuch in fuenf Sekunden.', e.reason);\
            socket = null;\
            setTimeout(function () {\
                connect();\
            }, 5000);\
        };\
\
        socket.onerror = function (err) {\
            console.error('Socket-Fehler: ', err.message, 'Socket wird geschlossen');\
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
            } if (\"volume\" in socketMsg) {\
                volumeSlider.setValue(parseInt(socketMsg.volume));\
            } if (\"trackinfo\" in socketMsg) {\
                document.getElementById('track').innerHTML = socketMsg.trackinfo.name;\
\
                var btnTrackPlayPause = document.getElementById('nav-btn-play');\
                if (socketMsg.trackinfo.pausePlay) {\
                    btnTrackPlayPause.innerHTML = '<i id=\"ico-play-pause\" class=\"fas fa-lg fa-play\"></i>';\
                } else {\
                    btnTrackPlayPause.innerHTML = '<i id=\"ico-play-pause\" class=\"fas fa-lg fa-pause\"></i>';\
                }\
\
                var btnTrackFirst = document.getElementById('nav-btn-first');\
                var btnTrackPrev = document.getElementById('nav-btn-prev');\
                if (socketMsg.trackinfo.currentTrackNumber <= 1) {\
                    btnTrackFirst.classList.add(\"disabled\");\
                    btnTrackPrev.classList.add(\"disabled\");\
                } else {\
                    btnTrackFirst.classList.remove(\"disabled\");\
                    btnTrackPrev.classList.remove(\"disabled\");\
                }\
                var btnTrackLast = document.getElementById('nav-btn-last');\
                var btnTrackNext = document.getElementById('nav-btn-next');\
                if (socketMsg.trackinfo.currentTrackNumber >= socketMsg.trackinfo.numberOfTracks) {\
                    btnTrackLast.classList.add(\"disabled\");\
                    btnTrackNext.classList.add(\"disabled\");\
                } else {\
                    btnTrackLast.classList.remove(\"disabled\");\
                    btnTrackNext.classList.remove(\"disabled\");\
                }\
            } if (\"coverimg\" in socketMsg) {\
                document.getElementById('coverimg').src = \"/cover?\" + new Date().getTime();\
          }\
      };\
    }\
\
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
    function getTrack() {\
        var myObj = {\
            \"trackinfo\": {\
                trackinfo: 'trackinfo'\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
    function getCoverimage() {\
        var myObj = {\
            \"coverimg\": {\
                coverimg: 'coverimg'\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
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
    function ftpStart() {\
        var myObj = {\
            \"ftpStatus\": {\
                start: 1\
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
    });\
</script>\
</body>\
</html>";