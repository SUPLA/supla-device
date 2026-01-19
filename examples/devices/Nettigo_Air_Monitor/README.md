Supla version of firware for Nettigo Air Monitor 0.3.3 from Nettigo:
https://nettigo.pl/products/tagged/NAM-033

With standard Wemos D1 ESP8266 module works all sensors and additional bh1750 light sensor, but there is not enough RAM to send results to https://aqi.eco
![NAM 0.3.3](https://docs.nettigo.pl/i-en/nam/033/nam-033-std-fa/nam-0.3.3-std-29.jpg)

After replacing the standard board with the ESP32 module, everything works. Make sure the module pinouts match the main NAM board.

You can use for example: https://nettigo.pl/products/modul-wifi-bluetooth-esp32-d1-mini-usb-c
![ESP32 module with pinout compaibile with original Wemos D1](https://nettigo.pl/system/images/4286/original.jpg?1727209615)

It uses Arduino libraries:
* Adafruit BME280 Library (https://github.com/adafruit/Adafruit_BME280_Library)
* DallasTemperature (https://github.com/milesburton/Arduino-Temperature-Control-Library)
* BH1750 (https://github.com/claws/BH1750)
* GuL_NovaFitness (https://github.com/boeserfrosch/GuL_NovaFitness)
* ClosedCube SHT31D fork (https://github.com/malarz-supla/ClosedCube_SHT31D_Arduino)

and only on ESP32:
* Arduino JSON (https://github.com/arduino-libraries/Arduino_JSON)
