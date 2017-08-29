# WiFiWeatherGuy
WeatherGuy for ESP8266

## Hardware
- Wemos D1 Mini
- ILI9163-based TFT display
- Connect the display to the ESP8266 [as described here](http://henrysbench.capnfatz.com/henrys-bench/arduino-displays/arduino-1-44-in-spi-tft-display-tutorial/), with CS on D6 and DC on D8.
- Connect the display's LED to D2 to automatically turn it off and a push
  switch to D3 and GND to turn it back on again.

## Software
- Arduino 1.8.4
- [ILI9163 library](https://github.com/sumotoy/TFT_ILI9163C)
- [ESP8266 for Arduino](https://github.com/esp8266/Arduino.git)
- [Arduino ESP8266 filesystem uploader](https://github.com/esp8266/arduino-esp8266fs-plugin)

## Installation
- Get a Wunderground [API key](https://www.wunderground.com/weather/api/d/docs)
- Edit data/config.txt with your preferences
- Upload the filesystem (Tools > ESP8266 Sketch Data Upload)
- Upload the sketch
