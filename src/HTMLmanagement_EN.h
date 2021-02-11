static const char management_HTML[] PROGMEM = "<!DOCTYPE html>\
<html lang=\"de\">\
<head>\
    <title>ESPuino-configuration</title>\
    <meta charset=\"utf-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
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
\
    <a class=\"reboot float-right nav-link\" href=\"/restart\"><i class=\"fas fa-power-off\"></i> Neustart</a>\
    </div>\
</nav>\
<br/>\
    <nav>\
        <div class=\"container nav nav-tabs\" id=\"nav-tab\" role=\"tablist\">\
            <a class=\"nav-item nav-link active\" id=\"nav-rfid-tab\" data-toggle=\"tab\" href=\"#nav-rfid\" role=\"tab\" aria-controls=\"nav-rfid\" aria-selected=\"true\"><i class=\"fas fa-dot-circle\"></i> RFID</a>\
            <a class=\"nav-item nav-link\" id=\"nav-wifi-tab\" data-toggle=\"tab\" href=\"#nav-wifi\" role=\"tab\" aria-controls=\"nav-wifi\" aria-selected=\"false\"><i class=\"fas fa-wifi\"></i><span class=\".d-sm-none .d-md-block\"> WiFi</span></a>\
            %SHOW_MQTT_TAB%\
            %SHOW_FTP_TAB%\
            <a class=\"nav-item nav-link\" id=\"nav-general-tab\" data-toggle=\"tab\" href=\"#nav-general\" role=\"tab\" aria-controls=\"nav-general\" aria-selected=\"false\"><i class=\"fas fa-sliders-h\"></i> General</a>\
            <a class=\"nav-item nav-link\" id=\"nav-tools-tab\" data-toggle=\"tab\" href=\"#nav-tools\" role=\"tab\" aria-controls=\"nav-tools\" aria-selected=\"false\"><i class=\"fas fa-wrench\"></i> Tools</a>\
        </div>\
    </nav>\
<br>\
<div class=\"tab-content\" id=\"nav-tabContent\">\
    <div class=\"tab-pane fade\" id=\"nav-wifi\" role=\"tabpanel\" aria-labelledby=\"nav-wifi-tab\">\
        <div class=\"container\" id=\"wifiConfig\">\
            <form action=\"#wifiConfig\" method=\"POST\" onsubmit=\"wifiConfig('wifiConfig'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <label for=\"ssid\">Wifi (SSID):</label>\
                    <input type=\"text\" class=\"form-control\" id=\"ssid\" placeholder=\"SSID\" name=\"ssid\" required>\
                    <div class=\"invalid-feedback\">\
                        Please enter WiFi's SSID.\
                    </div>\
                    <label for=\"pwd\">Password:</label>\
                    <input type=\"password\" class=\"form-control\" id=\"pwd\" placeholder=\"Passwort\" name=\"pwd\" required>\
                    <label for=\"hostname\">ESPuino-name (Hostname):</label>\
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
    <div class=\"tab-pane fade show active\" id=\"nav-rfid\" role=\"tabpanel\" aria-labelledby=\"nav-rfid-tab\">\
        <div class=\"container\" id=\"rfidMusicTags\">\
            <fieldset>\
                <legend>RFID-assignments</legend>\
            <form action=\"#rfidMusicTags\" method=\"POST\" onsubmit=\"rfidAssign('rfidMusicTags'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <label for=\"rfidIdMusic\">RFID-tag-ID (12-digits)</label>\
                    <input type=\"text\" class=\"form-control\" id=\"rfidIdMusic\" maxlength=\"12\" pattern=\"[0-9]{12}\"\
                           placeholder=\"%RFID_TAG_ID%\" name=\"rfidIdMusic\" required>\
                    <label for=\"fileOrUrl\">File, directory or URL (^ and # not allowed as characters)</label>\
                    <input type=\"text\" class=\"form-control\" id=\"fileOrUrl\" maxlength=\"255\" placeholder=\"z.B. /mp3/Hoerspiele/Yakari/Yakari_und_seine_Freunde.mp3\" pattern=\"^[^\\^#]+$\" name=\"fileOrUrl\" required>\
                    <div class=\"filetree-container\">\
                        <div id=\"filebrowser\">\
                            <div class=\"filetree demo\" id=\"filetree\"></div>\
                            <div class=\"refreshAction\">\
                                <span id=\"refreshAction\" data-toggle=\"tooltip\" data-placement=\"top\" title=\"Refresh indexfile.\"><i class=\"fas fa-sync fa-1x\"></i> Refresh indexfile</span>\
                            </div>\
                        </div>\
                        <div id=\"indexing-progress\" class=\"indexing-progress overlay\">\
                            <div style=\"text-align: center; color:white; margin-top:2em;\">\
                                <div><i class=\"fas fa-sync fa-spin fa-2x\"></i> <br><br>  This could take while...</div>\
                                <div id=\"currentProcessedFile\"></div>\
                            </div>\
                        </div>\
                    </div>\
                    <label for=\"playMode\">Playmode</label>\
                    <select class=\"form-control\" id=\"playMode\" name=\"playMode\">\
                        <option class=\"option-file\" value=\"1\">Single track</option>\
                        <option class=\"option-file\" value=\"2\">Single track (loop)</option>\
                        <option class=\"option-file-and-folder\" value=\"3\">Audiobook</option>\
                        <option class=\"option-file-and-folder\" value=\"4\">Audiobook (loop)</option>\
                        <option class=\"option-folder\" value=\"5\">All tracks of directory (alph. sorted)</option>\
                        <option class=\"option-folder\" value=\"6\">All tracks of directory (random, loop)</option>\
                        <option class=\"option-folder\" value=\"7\">All tracks of directory (alph. sorted, loop)</option>\
                        <option class=\"option-folder\" value=\"9\">All tracks of directory (random)</option>\
                        <option class=\"option-stream\" value=\"8\">Webstream</option>\
                    </select>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
            </fieldset>\
        </div>\
\
        <br>\
        <br>\
        <div class=\"container\" id=\"rfidModTags\">\
            <fieldset>\
                <legend>RFID-modifications</legend>\
            <form class=\"needs-validation\" action=\"#rfidModTags\" method=\"POST\" onsubmit=\"rfidMods('rfidModTags'); return false\">\
                <div class=\"form-group col-md-12\">\
                    <label for=\"rfidIdMod\">RFID-tag-ID (12-digits)</label>\
                    <input type=\"text\" class=\"form-control\" id=\"rfidIdMod\" maxlength=\"12\" pattern=\"[0-9]{12}\"\
                           placeholder=\"%RFID_TAG_ID%\" name=\"rfidIdMod\" required>\
                    <div class=\"invalid-feedback\">\
                        Please enter 12-digits-number.\
                    </div>\
                    <label for=\"modId\">Configuration</label>\
                    <select class=\"form-control\" id=\"modId\" name=\"modId\">\
                        <option value=\"100\">Keylock</option>\
                        <option value=\"101\">Auto-sleep after 15 minutes</option>\
                        <option value=\"102\">Auto-sleep after 30 minutes</option>\
                        <option value=\"103\">Auto-sleep after 1 hour</option>\
                        <option value=\"104\">Auto-sleep after 2 hours</option>\
                        <option value=\"105\">Auto-sleep after after end of current track</option>\
                        <option value=\"106\">Auto-sleep after after end of current playlist</option>\
                        <option value=\"107\">Auto-sleep after after five tracks</option>\
                        <option value=\"110\">Loop current playlist</option>\
                        <option value=\"111\">Loop current track</option>\
                        <option value=\"120\">Dimm LEDs (nightmode)</option>\
                        <option value=\"130\">Enable/disable (toggle) WiFi</option>\
                    </select>\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
            </fieldset>\
        </div>\
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-mqtt\" role=\"tabpanel\" aria-labelledby=\"nav-mqtt-tab\">\
        <div class=\"container\" id=\"mqttConfig\">\
\
            <form class=\"needs-validation\" action=\"#mqttConfig\" method=\"POST\"\
                  onsubmit=\"mqttSettings('mqttConfig'); return false\">\
                <div class=\"form-check col-md-12\">\
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
                    <label for=\"initialVolume\">After power on</label>\
                        <div class=\"text-center\">\
                    <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i> <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"initialVolume\" name=\"initialVolume\"\
                        data-slider-value=\"%INIT_VOLUME%\"  value=\"%INIT_VOLUME%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i></div>\
                        <br>\
                    <label for=\"maxVolumeSpeaker\">Maximum (speaker)</label>\
                        <div class=\"text-center\">\
                     <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i>   <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"maxVolumeSpeaker\" name=\"maxVolumeSpeaker\"\
                        data-slider-value=\"%MAX_VOLUME_SPEAKER%\"  value=\"%MAX_VOLUME_SPEAKER%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i>\
                        </div>\
                        <br>\
                    <label for=\"maxVolumeHeadphone\">Maximum (headphones)</label>\
                        <div class=\"text-center\">\
                    <i class=\"fas fa-volume-down fa-2x .icon-pos\"></i> <input data-provide=\"slider\" type=\"number\" data-slider-min=\"1\" data-slider-max=\"21\" min=\"1\" max=\"21\" class=\"form-control\" id=\"maxVolumeHeadphone\" name=\"maxVolumeHeadphone\"\
                         data-slider-value=\"%MAX_VOLUME_HEADPHONE%\" value=\"%MAX_VOLUME_HEADPHONE%\" required>  <i class=\"fas fa-volume-up fa-2x .icon-pos\"></i>\
                        </div>\
                </fieldset>\
                </div>\
<br>\
                <div class=\"form-group col-md-12\">\
                    <fieldset >\
                        <legend  class=\"w-auto\">Neopixel (brightness)</legend>\
                    <label for=\"initBrightness\">After power on:</label>\
                        <div class=\"text-center\">\
                            <i class=\"far fa-sun fa-2x .icon-pos\"></i>\
                        <input data-provide=\"slider\" type=\"number\" data-slider-min=\"0\" data-slider-max=\"255\" min=\"0\" max=\"255\" class=\"form-control\" id=\"initBrightness\" name=\"initBrightness\"\
                               data-slider-value=\"%INIT_LED_BRIGHTNESS%\"  value=\"%INIT_LED_BRIGHTNESS%\" required><i class=\"fas fa-sun fa-2x .icon-pos\"></i>\
                        </div>\
\
                    <label for=\"nightBrightness\">In nightmode</label>\
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
                        <label for=\"inactivityTime\">Inactivity (minutes)</label>\
                        <div class=\"text-center\"><i class=\"fas fa-hourglass-start fa-2x .icon-pos\"></i> <input type=\"number\" data-provide=\"slider\" data-slider-min=\"0\" data-slider-max=\"30\" min=\"1\" max=\"120\" class=\"form-control\" id=\"inactivityTime\" name=\"inactivityTime\"\
                         data-slider-value=\"%MAX_INACTIVITY%\"  value=\"%MAX_INACTIVITY%\" required><i class=\"fas fa-hourglass-end fa-2x .icon-pos\"></i></div>\
                    </fieldset>\
                </div>\
<br>\
\
                <div class=\"form-group col-md-12\">\
                    <fieldset>\
                        <legend>Battery</legend>\
                    <div>Status via Neopixel</div>\
                        <br>\
                    <label for=\"warningLowVoltage\">LED-warning will appear below this voltage.\
                    </label>\
                        <div class=\"text-center\">\
                        <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i> <input  data-provide=\"slider\"  data-slider-step=\"0.1\" data-slider-min=\"3.0\" data-slider-max=\"5.0\"  min=\"3.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"warningLowVoltage\" name=\"warningLowVoltage\"\
                        data-slider-value=\"%WARNING_LOW_VOLTAGE%\" value=\"%WARNING_LOW_VOLTAGE%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
<br>\
                    <label for=\"voltageIndicatorLow\">One LED lights at this voltage\
                        </label>\
                        <div class=\"text-center\">\
                        <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i> <input data-provide=\"slider\" min=\"2.0\" data-slider-step=\"0.1\"  data-slider-min=\"2.0\" data-slider-max=\"5.0\" max=\"5.0\"type=\"text\" class=\"form-control\" id=\"voltageIndicatorLow\" name=\"voltageIndicatorLow\"\
                          data-slider-value=\"%VOLTAGE_INDICATOR_LOW%\"  value=\"%VOLTAGE_INDICATOR_LOW%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
                        <br>\
                        <label for=\"voltageIndicatorHigh\">All LEDs light at this voltage</label>\
\
                            <div class=\"text-center\">\
                                <i class=\"fas fa-battery-quarter fa-2x .icon-pos\"></i><input data-provide=\"slider\" data-slider-step=\"0.1\"  data-slider-min=\"2.0\" data-slider-max=\"5.0\" min=\"2.0\" max=\"5.0\" type=\"text\" class=\"form-control\" id=\"voltageIndicatorHigh\" name=\"voltageIndicatorHigh\"\
                           data-slider-value=\"%VOLTAGE_INDICATOR_HIGH%\"  value=\"%VOLTAGE_INDICATOR_HIGH%\" pattern=\"^\\d{1,2}(\\.\\d{1,3})?\" required> <i class=\"fas fa-battery-three-quarters fa-2x .icon-pos\" fa-2x .icon-pos></i>\
                        </div>\
\
                        <br>\
                    <label for=\"voltageCheckInterval\">Interval of measurement (minutes)</label>\
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
    </div>\
    <div class=\"tab-pane fade\" id=\"nav-tools\" role=\"tabpanel\" aria-labelledby=\"nav-tools-tab\">\
        <div class=\"container my-5\" id=\"importNvs\">\
            <h2>NVS-Importer</h2>\
            <form action=\"/upload\" enctype=\"multipart/form-data\" method=\"POST\">\
                <div class=\"form-group\">\
                    <label for=\"nvsUpload\">Select backup-file for import.</label>\
                    <input type=\"file\" class=\"form-control-file\" id=\"nvsUpload\" name=\"nvsUpload\" accept=\".txt\">\
                </div>\
                <br>\
                <div class=\"text-center\">\
                <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
                </div>\
            </form>\
        </div>\
    </div>\
</div>\
<script type=\"text/javascript\">\
    var DEBUG = false;\
    var lastIdclicked = '';\
    var host = $(location).attr('hostname');\
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
            if ((/\\.(mp3|MP3|ogg|wav|WAV|OGG|wma|WMA|acc|ACC|flac|FLAC)$/i).test(lastFolder)) {\
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
    function renderFileTree() {\
\
        var filesURI = \"/files\";\
        if (DEBUG) {\
            filesURI = \"http://\" + host + \"/files\";\
        }\
        $('#filetree').jstree({\
            'core': {\
                'check_callback': true,\
                'data': {\
                    url: filesURI,\
                    error: function (XMLHttpRequest, textStatus, errorThrown) {\
                        $('#j1_loading').hide();\
                        $(\"#refreshAction\").hide();\
                        $('#filetree').html(\"<div class='clickForRefresh' ><i class='fas fa-sync fa-1x'><span id='#clickForRefresh' > Refresh fileindex.</span></div>\");\
                        $('#filetree').on(\"click\", function () {\
                            refreshFileList();\
                        });\
                        toastr.error(\"Unable to fetch directory-list.\");\
                    }\
                },\
\
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
            'plugins': [\"themes\", \"types\"]\
        }).bind('loaded.jstree', function (event, data) {\
            postRendering(event, data);\
            if ((data.instance._model.data['#']['children'].length == 0)) {\
                toastr.info(\"SD-card-index needs to be refreshed.\");\
            }\
        }).bind('refresh.jstree', function (event, data) {\
            postRendering(event, data);\
        });\
    }\
\
    $('#filetree').on('select_node.jstree', function (e, data) {\
        $('input[name=fileOrUrl]').val(data.node.id);\
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
    });\
\
    $('#refreshAction').on(\"click\", function () {\
        refreshFileList();\
        $(\"#indexing-progress\").show();\
        $(\"#refreshAction\").hide();\
    });\
\
    $('#playMode').on(\"change\", function () {\
        if (this.value == 8) {\
            $('#filebrowser').slideUp();\
        } else {\
            $('#filebrowser').slideDown();\
        }\
    });\
\
    function showFileIndexingState() {\
        $(\"#indexing-progress\").show();\
        $(\"#refreshAction\").hide();\
    }\
\
    function hideFileIndexingState() {\
        $(\"#indexing-progress\").hide();\
        $(\"#refreshAction\").show();\
    }\
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
              document.getElementById('rfidIdMod').value = socketMsg.rfidId;\
              document.getElementById('rfidIdMusic').value = socketMsg.rfidId;\
              toastr.info(\"RFID Tag mit \"+ socketMsg.rfidId + \" erkannt.\" );\
\
              $(\"#rfidIdMusic\").effect(\"highlight\", {color:\"#abf5af\"}, 3000);\
              $(\"#rfidIdMod\").effect(\"highlight\", {color:\"#abf5af\"}, 3000);\
\
          } if (\"status\" in socketMsg) {\
              if (socketMsg.status == \"ok\") {\
                  toastr.success(\"Action completed successfully.\" );\
              }\
          } if (\"pong\" in socketMsg) {\
              if (socketMsg.pong == 'pong') {\
                  pong();\
              }\
          } if (\"refreshFileList\" in socketMsg) {\
              hideFileIndexingState();\
              toastr.info(\"SD-card-index has been refreshed!\");\
              $('#filetree').jstree(true).refresh();\
\
          }\
          if (\"indexingState\" in socketMsg) {\
              if(socketMsg.indexingState != null) {\
                  $(\"#currentProcessedFile\").text(socketMsg.indexingState);\
                  console.log(socketMsg.indexingState);\
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
            toastr.warning('Connection to ESPuino is broken. Please refresh.');\
        }, 5000);\
    }\
\
    function pong() {\
        clearTimeout(tm);\
    }\
\
    function refreshFileList(clickedId) {\
        lastIdclicked = clickedId;\
        var myObj = {\
            \"refreshFileList\": true\
        };\
        var myJSON = JSON.stringify(myObj);\
        $(\"#refreshAction\").hide();\
        socket.send(myJSON);\
        showFileIndexingState();\
    };\
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
                mqttPwd: document.getElementById('mqttPwd').value\
            }\
        };\
        var myJSON = JSON.stringify(myObj);\
        socket.send(myJSON);\
    }\
\
    function rfidMods(clickedId) {\
        lastIdclicked = clickedId;\
        var myObj = {\
            \"rfidMod\": {\
                rfidIdMod: document.getElementById('rfidIdMod').value,\
                modId: document.getElementById('modId').value\
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
        var myObj = {\
            \"rfidAssign\": {\
                rfidIdMusic: document.getElementById('rfidIdMusic').value,\
                fileOrUrl: removeTrSlash(document.getElementById('fileOrUrl').value),\
                playMode: document.getElementById('playMode').value\
            }\
        };\
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
\
    $(document).ready(function () {\
        connect();\
        renderFileTree();\
\
        console.log(parseInt(document.getElementById('warningLowVoltage').value));\
        $(function () {\
            $('[data-toggle=\"tooltip\"]').tooltip();\
        });\
\
    });\
</script>\
</body>\
</html>\
";