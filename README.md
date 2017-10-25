# MP3 WebServer for ESP8266

Playing mp3 on request

## Avaliable requests

- `/mp3/next`  
- `/mp3/stop`
- `/volume/up`
- `/volume/down`

also avaliable
- `favicon.ico`

you can place `favicon.ico` into `data` folder and use [ESP8266 Scatch Data Upload](https://github.com/esp8266/arduino-esp8266fs-plugin) tools

`DFPlayer` connect to `D7` (throw 1K), `D8` (throw 1K), `GND`, `3V`  
10K variable resistor connect to `A0`, `GND`, `3V`  

![NodeMcu-pinout](http://arduino-project.net/wp-content/uploads/2016/03/NodeMcu-pinout-600x531.jpg)

Use [bscheshir/volume-control](https://github.com/bscheshir/volume-control) library.

## Installation
clone 
```sh
~/Arduino$ git clone https://github.com/bscheshir/esp8266-mp3-web-server WiFiWebServerMP3
```
add to libraries https://github.com/bscheshir/volume-control
