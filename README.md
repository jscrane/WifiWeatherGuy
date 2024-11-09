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
- [ArduinoJson](http://arduinojson.org/) 7.2.0
- [Timezone](https://github.com/JChristensen/Timezone) 1.2.4
- [Time](https://github.com/PaulStoffregen/Time) 1.6
- [SimpleTimer](https://github.com/schinken/SimpleTimer)

## Installation
- Get an API key for your Provider (required for OpenWeatherMap, not for Open Meteo)
- Edit data/config.json with your preferences
- Configure your display in TFT_eSPI/User_Setup.h (if using the Arduino IDE), otherwise edit Makefile
- Configure your timezone in zone.h
- Upload the filesystem (Tools > ESP8266 Sketch Data Upload)
- Upload the sketch

## Note
The weather icons must be 24-bit bitmaps; convert from GIF as follows:

    % convert foo.gif -type truecolor foo.bmp 

The [Airycon](https://github.com/HaroleDev/Airycons) icons used by the Open Meteo provider were
converted using the GIMP, exporting the PNG files as 24-bit BMPs.

## Providers

### Open Weather Map
A previously supported provider was [OpenWeatherMap](https://openweathermap.org).

However as-of 2024-11-04 its version 2.5 API is deprecated in favour of version 3.0 which
requires registration of a credit card.

Limitations of this API are:
- astronomy: moon age, phase
- forecasts: forecasts in the free API are every 3 hours and you get 40 of
them, which is too big to parse on an ESP8266
- the credit-card thing

Its code remains for reference, for now.

### Open Meteo
The latest provider is [Open-Meteo](https://open-meteo.com/en/docs).

Limitations of this API are:
- astronomy: moon age, phase
- no forecast humidity

### Meterologisk
If bad things happen to Open Meteo, the next provider is
[Meterologisk](https://github.com/jscrane/WifiWeatherGuy/issues/27).

## Credits
- Javascript [transparency](https://github.com/leonidas/transparency)
- WMO [icons](https://weather-sense.leftium.com/wmo-codes)

![The Finished Article](eagle/wwg.png)
