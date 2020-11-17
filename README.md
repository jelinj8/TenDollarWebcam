# TenDollarWebcam

This little project has two goals:
1. Be a little open source webcam (with so-so quality, but hey $10 and open source)
2. Serve as an example app for the [Micro-RTSP](https://github.com/geeksville/Micro-RTSP)
library.

![sample VLC view](/docs/camview.png "Sample VLC output")

These camera are _very_ cheap (< $10) and the ESP32 has a fair amount of horsepower
left over for other work.

![devboards](/docs/devpict.jpg "Typical ESP32-CAM boards")

# Supported boards

Virtually any OV2460 + ESP32 board should work.

For boards other than this list you might (probably not) need to change esp32cam_config
to use the GPIO assignments for your hardware.

I've tested the following boards with the stock config: [ex1](https://www.banggood.com/Geekcreit-ESP32-CAM-WiFi-+-Bluetooth-Camera-Module-Development-Board-ESP32-With-Camera-Module-OV2640--p-1394679.html?rmmds=myorder&cur_warehouse=CN), [ex2](https://www.banggood.com/TTGO-T-Journal-ESP32-Camera-Development-Board-OV2640-SMA-WiFi-3dbi-Antenna-0_91-OLED-Camera-Board-p-1379925.html?rmmds=myorder&cur_warehouse=CN), [ex3](https://www.banggood.com/M5Stack-Official-ESP32-Camera-Module-Development-Board-OV2640-Camera-Type-C-Grove-Port-p-1333598.html?rmmds=myorder&cur_warehouse=CN).

## ESP32-CAM board special handling needed

This board is great in some ways: it has PSRAM (so can in theory capture might higher res images than SVGA) and it is cheap and tiny.  Two downsides:

* It has no built in USB port, so to program you'll need to use a USB serial adapter.  See docs for a photo of the proper pins.
* The GPIO assignments are different for the camera, so you'll need to define USEBOARD_AITHINKER

## TTGO-T board special handling needed

@drmocm contributed a config file for [this great board](https://www.aliexpress.com/item/TTGO-T-Camera-ESP32-WROVER-PSRAM-Camera-Module-ESP32-WROVER-B-OV2640-Camera-Module-0-96/32968683765.html).  It might be the current best choice because it
has an extra 8MB of RAM which makes it possible to use very high resolutions on the camera.  
You will need to #define USEBOARD_TTGO_T in ESP32-devcam.ino to get the proper bindings for this board.

# How to buy/install/run

This project uses the simple PlatformIO build system.  You can use the IDE, but for brevity
in these instructions I describe use of their command line tool.

1. Purchase one of the inexpensive ESP32-CAM modules from asia (see above).
2. Install [PlatformIO](https://platformio.org/).
3. Download this git repo and cd into it.
4. pio run -t upload (This command will fetch dependencies, build the project and install it on the board via USB)

~~The first time you run your device you'll need to use an Android or iOS app to give it
access to your wifi network. See [instructions here](https://github.com/geeksville/AutoWifi/blob/master/README.md).~~
To configure WiFi connection short GPIO16 (default cfg) to GND and reset to force the board into configuration mode.
It'll start an AP with captive portal. When you connect to it, you should be asked to "login a network". That would
redirect you to the config UI and allow you configure the network(s) the camera should connect to.

When rebooted, the camera autoconnects to configured networks. If it isn't succesful, it starts to flash the LED,
until it is succesfull.

At this point your device should be happily serving up frames.  Either via
a web-browser at http://yourdeviceipaddr or more interestingly via a standard
RTSP video stream.  If your device has an LCD screen it will be showing the IP address and boot messages
to that screen.

To see the RTSP stream use the client of your choice, for example you can use VLC
as follows:
```
vlc -v rtsp://yourdevipaddr:554/mjpeg/1
```

* Modified to use [AutoConnect](https://github.com/Hieromon/AutoConnect) library for WiFi management.
* Created basic root page with links to stream, single image and rtsp url + an image.

Current code supports only one client at a time. If you want to share the camera, use the /jpg url to get a single frame repeatedly.
I'm using it on a linux server (just a small WiFi router with OpenWRT + external storage, serving a static HTML page). It downloads
a frame periodically and symlinks to the last downloaded file. That link is then used in a web page to be shown on a self-refreshing
page shown on a smart TV or a phone. Another script takes care to delete old frames (e.g. 10 days and older) every night. This way
I can display a matrix (in my case 2x2) of images. It doesn't have a too big refresh rate, but the number of clients is limited only
by my bandwidth and I can combine it with cameras of other brands (they just have to be capable of providing an image to download).

# Compilation problem hints
In case of error like this (on Win/Mac):
```
.pio\libdeps\esp32\Micro-RTSP_ID6071\src\CRtspSession.cpp: In member function 'const char* CRtspSession::DateHeader()':
.pio\libdeps\esp32\Micro-RTSP_ID6071\src\CRtspSession.cpp:358:26: error: 'time' was not declared in this scope
     time_t tt = time(NULL);
                          ^
.pio\libdeps\esp32\Micro-RTSP_ID6071\src\CRtspSession.cpp:359:76: error: 'gmtime' was not declared in this scope
     strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
                                                                            ^
.pio\libdeps\esp32\Micro-RTSP_ID6071\src\CRtspSession.cpp:359:77: error: 'strftime' was not declared in this scope
     strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
```

There might be a conflict with Time lib (no case sensitivity in filenames). I've modified my PIO ESP32 platform by renaming
```c:\Users\user\.platformio\packages\toolchain-xtensa32\xtensa-esp32-elf\include\time.h``` to ```_time.h```.
Then I've changed include in ```TenDollarWebcam\.pio\libdeps\esp32\Micro-RTSP_ID6071\src\CRtspSession.cpp``` to
```
#include <_time.h>
```
