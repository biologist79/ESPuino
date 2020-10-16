static const char basicWebsite[] PROGMEM = "<!DOCTYPE html>\
<html>\
    <head>\
        <title>Wifi-configuration</title>\
    </head>\
    <body>\
        <form action=\"/init\" method=\"POST\">\
            <fieldset>\
                <legend>Basic wifi-setup</legend>\
                <label for=\"ssid\">SSID:</label><br>\
                <input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"SSID\" required><br>\
                <label for=\"pwd\">Password:</label><br>\
                <input type=\"password\" id=\"pwd\" name=\"pwd\" autocomplete=\"off\" required><br>\
                <label for=\"hostname\">Tonuino-name (hostname):</label><br>\
                <input type=\"text\" id=\"hostname\" name=\"hostname\" required><br><br>\
                <input type=\"submit\" value=\"Submit\">\
            </fieldset>\
        </form>\
        <form action=\"/restart\">\
            <button type=\"submit\">Reboot</button>\
        </form>\
    </body>\
</html>\
";