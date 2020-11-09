static const char mgtWebsite[] PROGMEM = "<!DOCTYPE html>\
<html lang=\"de\">\
  <head>\
    <title>ESPuino-configuration</title>\
    <meta charset=\"utf-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
    <link rel=\"stylesheet\" href=\"https://ts-cs.de/tonuino/css/bootstrap.min.css\">\
    <script src=\"https://ts-cs.de/tonuino/js/jquery.min.js\"></script>\
    <script src=\"https://ts-cs.de/tonuino/js/popper.min.js\"></script>\
    <script src=\"https://ts-cs.de/tonuino/js/bootstrap.min.js\"></script>\
  </head>\
  <body>\
    <nav class=\"navbar navbar-expand-sm bg-primary navbar-dark\">\
      <button class=\"navbar-toggler\" type=\"button\" data-toggle=\"collapse\" data-target=\"#navbarSupportedContent\" aria-controls=\"navbarSupportedContent\" aria-expanded=\"false\" aria-label=\"Toggle navigation\">\
        <span class=\"navbar-toggler-icon\"></span>\
      </button>\
      <a class=\"navbar-brand\">\
        <img src=\"https://raw.githubusercontent.com/biologist79/Tonuino-ESP32-I2S/master/html/tonuino_logo.png\" width=\"30\" height=\"30\" class=\"d-inline-block align-top\" alt=\"\" />\
        Tonuino\
      </a>\
      <div class=\"collapse navbar-collapse\" id=\"collapsibleNavbar\">\
        <ul class=\"navbar-nav mr-auto\">\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"#wifiConfig\">Wifi</a>\
          </li>\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"#rfidMusicTags\">RFID-assignments</a>\
          </li>\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"#rfidModTags\">RFID-modifications</a>\
          </li>\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"#mqttConfig\">MQTT</a>\
          </li>\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"#ftpConfig\">FTP</a>\
          </li>\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"#generalConfig\">General</a>\
          </li>\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"#importNvs\">NVS-importer</a>\
          </li>\
          <li class=\"nav-item\">\
            <a class=\"nav-link\" href=\"/restart\" style=\"color: orange\">Reboot Tonuino</a>\
          </li>\
        </ul>\
      </div>\
    </nav>\
    <br />\
    <div class=\"container\" id=\"wifiConfig\">\
      <h2>Wifi-configuration</h2>\
      <form action=\"#wifiConfig\" method=\"POST\" onsubmit=\"wifiConfig('wifiConfig'); return false\">\
        <div class=\"form-group col-md-6\">\
          <label for=\"ssid\">Wifi-name (SSID):</label>\
          <input type=\"text\" class=\"form-control\" id=\"ssid\" placeholder=\"SSID\" name=\"ssid\" required>\
          <div class=\"invalid-feedback\">\
            Please enter name of wifi (SSID).\
          </div>\
          <label for=\"pwd\">Password:</label>\
          <input type=\"password\" class=\"form-control\" id=\"pwd\" placeholder=\"Passwort\" name=\"pwd\" required>\
          <label for=\"hostname\">Tonuino-name (hostname):</label>\
          <input type=\"text\" class=\"form-control\" id=\"hostname\" placeholder=\"tonuino\" name=\"hostname\" value=\"%HOSTNAME%\" pattern=\"^[^-\\.]{2,32}\" required>\
        </div>\
        <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
        <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
      </form>\
      <div class=\"messages col-md-6 my-2\"></div>\
    </div>\
    <div class=\"container my-5\" id=\"rfidMusicTags\">\
        <h2>RFID-assignments</h2>\
        <form action=\"#rfidMusicTags\" method=\"POST\" onsubmit=\"rfidAssign('rfidMusicTags'); return false\">\
            <div class=\"form-group col-md-6\">\
                <label for=\"rfidIdMusic\">RFID-chip-ID (12 digits)</label>\
                <input type=\"text\" class=\"form-control\" id=\"rfidIdMusic\" maxlength=\"12\" pattern=\"[0-9]{12}\" placeholder=\"%RFID_TAG_ID%\" name=\"rfidIdMusic\" required>\
                <label for=\"fileOrUrl\">File, directory or URL (^ und # are not allowed)</label>\
                <input type=\"text\" class=\"form-control\" id=\"fileOrUrl\" maxlength=\"255\" placeholder=\"z.B. /mp3/Hoerspiele/Yakari/Yakari_und_seine_Freunde.mp3\" pattern=\"^[^\\^#]+$\" name=\"fileOrUrl\" required>\
                <label for=\"playMode\">Playmode</label>\
                <select class=\"form-control\" id=\"playMode\" name=\"playMode\">\
                    <option value=\"1\">Single track</option>\
                    <option value=\"2\">Single track (infinite loop)</option>\
                    <option value=\"3\">Audiobook</option>\
                    <option value=\"4\">Audiobook (infinite loop)</option>\
                    <option value=\"5\">All tracks of directory (sorted)</option>\
                    <option value=\"6\">All tracks of directory (random)</option>\
                    <option value=\"7\">All tracks of directory (sorted, inf. loop)</option>\
                    <option value=\"9\">All tracks of directory (random, inf. loop)</option>\
                    <option value=\"8\">Webradio</option>\
                </select>\
            </div>\
          <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
          <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
        </form>\
        <div class=\"messages col-md-6 my-2\"></div>\
    </div>\
    <div class=\"container my-5\" id=\"rfidModTags\">\
        <h2>RFID-modifications</h2>\
        <form class=\"needs-validation\" action=\"#rfidModTags\" method=\"POST\" onsubmit=\"rfidMods('rfidModTags'); return false\">\
            <div class=\"form-group col-md-6\">\
                <label for=\"rfidIdMod\">RFID-chip-ID (12-digits)</label>\
                <input type=\"text\" class=\"form-control\" id=\"rfidIdMod\" maxlength=\"12\" pattern=\"[0-9]{12}\" placeholder=\"%RFID_TAG_ID%\" name=\"rfidIdMod\" required>\
                <div class=\"invalid-feedback\">\
                  Please enter a 12-digits-number.\
                </div>\
                <label for=\"modId\">Abspielmodus</label>\
                <select class=\"form-control\" id=\"modId\" name=\"modId\">\
                    <option value=\"100\">Lock keys</option>\
                    <option value=\"101\">Sleep after 15 minutes</option>\
                    <option value=\"102\">Sleep after 30 minutes</option>\
                    <option value=\"103\">Sleep after 1 hour</option>\
                    <option value=\"104\">Sleep after 1 hours</option>\
                    <option value=\"105\">Sleep after end of track</option>\
                    <option value=\"106\">Sleep after end of playlist</option>\
                    <option value=\"107\">Sleep after five tracks</option>\
                    <option value=\"110\">Repeat playlist (inf. loop)</option>\
                    <option value=\"111\">Repeat track (inf. loop)</option>\
                    <option value=\"112\">Dimm LEDs (nightmode)</option>\
                </select>\
          </div>\
          <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
          <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
        </form>\
        <div class=\"messages col-md-6 my-2\"></div>\
    </div>\
    <div class=\"container my-5\" id=\"mqttConfig\">\
        <h2>MQTT-configuration</h2>\
        <form class=\"needs-validation\" action=\"#mqttConfig\" method=\"POST\" onsubmit=\"mqttSettings('mqttConfig'); return false\">\
            <div class=\"form-check col-md-6\">\
                <input class=\"form-check-input\" type=\"checkbox\" value=\"1\" id=\"mqttEnable\" name=\"mqttEnable\" %MQTT_ENABLE%>\
                <label class=\"form-check-label\" for=\"mqttEnable\">\
                  Enable MQTT\
                </label>\
            </div>\
            <div class=\"form-group my-2 col-md-6\">\
                <label for=\"mqttServer\">MQTT-server</label>\
                <input type=\"text\" class=\"form-control\" id=\"mqttServer\" minlength=\"7\" maxlength=\"%MQTT_SERVER_LENGTH%\" placeholder=\"z.B. 192.168.2.89\" name=\"mqttServer\" value=\"%MQTT_SERVER%\">\
                <label for=\"mqttUser\">MQTT-username (optional):</label>\
                <input type=\"text\" class=\"form-control\" id=\"mqttUser\" maxlength=\"%MQTT_USER_LENGTH%\" placeholder=\"Benutzername\" name=\"mqttUser\" value=\"%MQTT_USER%\">\
                <label for=\"mqttPwd\">Password (optional):</label>\
                <input type=\"password\" class=\"form-control\" id=\"mqttPwd\" maxlength=\"%MQTT_PWD_LENGTH%\" placeholder=\"Passwort\" name=\"mqttPwd\" value=\"%MQTT_PWD%\">\
            </div>\
          <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
          <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
        </form>\
        <div class=\"messages col-md-6 my-2\"></div>\
    </div>\
    <div class=\"container\" id=\"ftpConfig\">\
        <h2>FTP-configuration</h2>\
        <form action=\"#ftpConfig\" method=\"POST\" onsubmit=\"ftpSettings('ftpConfig'); return false\">\
          <div class=\"form-group col-md-6\">\
            <label for=\"ftpUser\">FTP-username:</label>\
            <input type=\"text\" class=\"form-control\" id=\"ftpUser\" maxlength=\"%FTP_USER_LENGTH%\" placeholder=\"Benutzername\" name=\"ftpUser\" value=\"%FTP_USER%\" required>\
            <label for=\"pwd\">password:</label>\
            <input type=\"password\" class=\"form-control\" id=\"ftpPwd\" maxlength=\"%FTP_PWD_LENGTH%\" placeholder=\"Passwort\" name=\"ftpPwd\" value=\"%FTP_PWD%\" required>\
          </div>\
          <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
          <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
        </form>\
        <div class=\"messages col-md-6 my-2\"></div>\
    </div>\
    <div class=\"container my-5\" id=\"generalConfig\">\
        <h2>General configuration</h2>\
        <form action=\"#generalConfig\" method=\"POST\" onsubmit=\"genSettings('generalConfig'); return false\">\
            <div class=\"form-group col-md-6\">\
                <label for=\"initialVolume\">Volume after start</label>\
                <input type=\"number\" min=\"1\" max=\"21\" class=\"form-control\" id=\"initialVolume\" name=\"initialVolume\" value=\"%INIT_VOLUME%\" required>\
                <label for=\"maxVolume\">Maximum volume</label>\
                <input type=\"number\" min=\"1\" max=\"21\" class=\"form-control\" id=\"maxVolume\" name=\"maxVolume\" value=\"%MAX_VOLUME%\" required>\
            </div>\
            <div class=\"form-group col-md-6\">\
                <label for=\"initBrightness\">Neopixel-brightness after start</label>\
                <input type=\"number\" min=\"0\" max=\"255\" class=\"form-control\" id=\"initBrightness\" name=\"initBrightness\" value=\"%INIT_LED_BRIGHTNESS%\" required>\
                <label for=\"nightBrightness\">Neopixel-brightness in nightmode</label>\
                <input type=\"number\" min=\"0\" max=\"255\" class=\"form-control\" id=\"nightBrightness\" name=\"nightBrightness\" value=\"%NIGHT_LED_BRIGHTNESS%\" required>\
            </div>\
            <div class=\"form-group col-md-6\">\
                <label for=\"inactivityTime\">Deepsleep after inactivity (minutes)</label>\
                <input type=\"number\" min=\"1\" max=\"1440\" class=\"form-control\" id=\"inactivityTime\" name=\"inactivityTime\" value=\"%MAX_INACTIVITY%\" required>\
            </div>\
          <button type=\"reset\" class=\"btn btn-secondary\">Reset</button>\
          <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
        </form>\
        <div class=\"messages col-md-6 my-2\"></div>\
      </div>\
      <div class=\"container my-5\" id=\"importNvs\">\
        <h2>NVS-importer</h2>\
        <form action=\"/upload\" enctype=\"multipart/form-data\" method=\"POST\">\
          <div class=\"form-group\">\
            <label for=\"nvsUpload\">Backup-files can be imported here.</label>\
            <input type=\"file\" class=\"form-control-file\" id=\"nvsUpload\" name=\"nvsUpload\" accept=\".txt\">\
          </div>\
            <button type=\"submit\" class=\"btn btn-primary\">Submit</button>\
        </form>\
        <div class=\"messages col-md-6 my-2\"></div>\
      </div>\
        <script>\
          var lastIdclicked = '';\
          var errorBox = '<div class=\"alert alert-danger alert-dismissible fade show\" role=\"alert\">Error occured!<button type=\"button\" class=\"close\" data-dismiss=\"alert\" aria-label=\"Close\"><span aria-hidden=\"true\">&times;</span></button></div>';\
          var okBox = '<div class=\"alert alert-success alert-dismissible fade show\" role=\"alert\">Action successful.<button type=\"button\" class=\"close\" data-dismiss=\"alert\" aria-label=\"Close\"><span aria-hidden=\"true\">&times;</span></button></div>';\
\
          var socket = new WebSocket(\"ws://%IPv4%/ws\");\
\
          function connect() {\
            socket = new WebSocket(\"ws://%IPv4%/ws\");\
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
              alert(\"Connection to tonuino is broken!\\nPlease refresh this website.\");\
              }, 5000);\
          }\
\
          function pong() {\
            clearTimeout(tm);\
          }\
\
          socket.onopen = function () {\
            setInterval(ping, 15000);\
          };\
\
          socket.onclose = function(e) {\
            console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);\
            setTimeout(function() {\
              connect();\
            }, 5000);\
          };\
\
          socket.onerror = function(err) {\
            console.error('Socket encountered error: ', err.message, 'Closing socket');\
            socket.close();\
          };\
\
          socket.onmessage = function(event) {\
            console.log(event.data);\
            var socketMsg = JSON.parse(event.data);\
            if (socketMsg.rfidId != null) {\
              document.getElementById('rfidIdMod').value = socketMsg.rfidId;\
              document.getElementById('rfidIdMusic').value = socketMsg.rfidId;\
              $(\"#rfidIdMusic\").fadeOut(500).fadeIn(500).fadeOut(500).fadeIn(500).fadeOut(500).fadeIn(500);\
              $(\"#rfidIdMod\").fadeOut(500).fadeIn(500).fadeOut(500).fadeIn(500).fadeOut(500).fadeIn(500);\
\
            } if (socketMsg.status != null) {\
                if (socketMsg.status == 'ok') {\
                  $(\"#\" + lastIdclicked).find('.messages').html(okBox);\
                } else {\
                  $(\"#\" + lastIdclicked).find('.messages').html(errorBox);\
                }\
            } if (socketMsg.pong != null) {\
                if (socketMsg.pong == 'pong') {\
                  pong();\
                }\
            }\
          };\
\
          function genSettings(clickedId) {\
            lastIdclicked = clickedId;\
            var myObj = {\
              \"general\": {\
                iVol: document.getElementById('initialVolume').value,\
                mVol: document.getElementById('maxVolume').value,\
                iBright: document.getElementById('initBrightness').value,\
                nBright: document.getElementById('nightBrightness').value,\
                iTime: document.getElementById('inactivityTime').value\
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
            if(str.substr(-1) === '/') {\
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
        </script>\
  </body>\
</html>\
";