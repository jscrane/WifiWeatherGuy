View this project on [CADLAB.io](https://cadlab.io/project/1280). 

# WiFiWeatherGuy
WeatherGuy for ESP8266.

![Schematic](eagle/schematic.png)

## Hardware
- Wemos D1 Mini
- ILI9163-based TFT display, see [here](http://henrysbench.capnfatz.com/henrys-bench/arduino-displays/arduino-1-44-in-spi-tft-display-tutorial/)

## Software
- Arduino 1.8.9
- [TFT_eSPI library](https://github.com/Bodmer/TFT_eSPI) 2.5.43
- [ESP8266 for Arduino](https://github.com/esp8266/Arduino.git) 3.1.2
- [Arduino ESP8266 filesystem uploader](https://github.com/esp8266/arduino-esp8266fs-plugin)
- [ArduinoJson](http://arduinojson.org/) 6.21.5
- [Timezone](https://github.com/JChristensen/Timezone) 1.2.4
- [Time](https://github.com/PaulStoffregen/Time) 1.6
- [SimpleTimer](https://github.com/schinken/SimpleTimer)

## Installation
- Get an API key for your Provider (the default is OpenWeatherMap)
- Edit data/config.json with your preferences
- Configure your display in TFT_eSPI/User_Setup.h (if using the Arduino IDE), otherwise edit Makefile
- Configure your timezone in zone.h
- Upload the filesystem (Tools > ESP8266 Sketch Data Upload)
- Upload the sketch

## Note
The weather icons must be 24-bit bitmaps; convert from GIF as follows:

    % convert foo.gif -type truecolor foo.bmp 

## Providers
Original support was only for Wunderground. However changes to its
terms of service means it is no longer supported. (Wunderground
client code remains for reference.)

### Open Weather Map
The current supported provider is [OpenWeatherMap](https://openweathermap.org).
Limitations of this API are:
- astronomy: moon age, phase
- forecasts: forecasts in the free API are every 3 hours and you get 40 of
them, which is too big to parse on an ESP8266. (Currently we hack this by
getting 16 of them and skipping every other one.)

### Dark Sky
Limitations of the [Dark Sky API](https://darksky.net/dev/docs) are:
- no icons (but can use other iconsets)
- lat/lon only (no "by city")
- moon phase only (no moonrise/set)
- aka forecast.io

## Credits
- Javascript [transparency](https://github.com/leonidas/transparency)

![The Finished Article](eagle/wwg.png)
