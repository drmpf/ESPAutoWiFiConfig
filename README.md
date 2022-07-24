# ESPAutoWiFiConfig
This simple code allows you to easily connect your ESP32 or ESP8266 device to any network without recoding.   
It avoids hard coded network SSID, password and IP settings and provides a simple webpage to complete the setup.   
Just turn the device on and when the led to start flashing rapidly, scan the QR code to connect to the device and open the http://192.168.1.1 web page to complete the network setup.   
This code also takes care of connecting to the local network connection with just two simple method calls, once you have set the WiFi settings.  

This library includes:-  
* **ESPAutoWiFiConfig**, Automatically opens a web page to configure your network settings  
* **ESPBufferedClient**, buffers prints to WiFi connections to prevent lots of small packets being sent   
* **ESP32_WS2812Flasher**, support for flashing the WS2812 RGB Led on the ESP32-C3/S3  
This library needs the [SafeString](https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html) library to be installed as well

# How-To
See [ESP32 / ESP8266 Auto WiFi Config](https://www.forward.com.au/pfod/ESPAutoWiFiConfig/index.html)  

# Software License
(c)2014-2022 Forward Computing and Control Pty. Ltd.  
NSW Australia, www.forward.com.au  
This code is not warranted to be fit for any purpose. You may only use it at your own risk.  
This code may be freely used for both private and commercial use  
Provide this copyright is maintained.    

# Revisions
Version 2.1 added reboot on loss of connection, clearRebootFlag while try trying to connect, added defaultNetworkSettings (see below)   
Version 2.0 supports ESP32-C3, ESP32-S2, ESP32-S3, etc that have a ws2312 RGB addressable led   
