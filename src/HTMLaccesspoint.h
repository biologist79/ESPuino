static const char accesspoint_HTML[] PROGMEM = "<!DOCTYPE html>\
<html>\
<head>\
    <title>WLAN-Einrichtung</title>\
    <style>\
        input {\
            width: 90%%;\
            height: 44px;\
            border-radius: 4px;\
            margin: 10px auto;\
            font-size: 15px;\
            background: #f1f1f1;\
            border: 0;\
            padding: 0 15px\
        }\
        input {\
\
        }\
        body {\
            background: #007bff;\
            font-family: sans-serif;\
            font-size: 14px;\
            color: #777\
        }\
        .box {\
            background: #fff;\
            max-width: 258px;\
            margin: 75px auto;\
            padding: 30px;\
            border-radius: 5px;\
            text-align: center\
        }\
        .btn {\
            background: #3498db;\
            color: #fff;\
            cursor: pointer;\
            width: 90%%;\
            height: 44px;\
            border-radius: 4px;\
            margin: 10px auto;\
            font-size: 15px;\
        }\
        .rebootmsg {\
            display: none;\
        }\
    </style>\
</head>\
<body>\
<form id=\"settings\" action=\"/init\" class=\"box\" method=\"POST\">\
    <h1>WiFi Configuration</h1>\
    <label for=\"ssid\">SSID:</label><br>\
    <input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"SSID\" required><br>\
    <label for=\"pwd\">Passwort:</label><br>\
    <input type=\"password\" id=\"pwd\" name=\"pwd\" autocomplete=\"off\" required><br>\
    <label for=\"hostname\">Tonuino-Name (Hostname):</label><br>\
    <input type=\"text\" id=\"hostname\" name=\"hostname\" placeholder=\"Tonuino\" required><br><br>\
    <input class=\"btn\" type=\"submit\" id=\"save-button\" value=\"Save\">\
</form>\
<form action=\"/restart\" class=\"box\">\
    <h1>Ready to go?</h1>\
    <input class=\"btn\" type=\"submit\" id=\"restart-button\" value=\"Reboot\">\
</form>\
\
</body>\
</html>";