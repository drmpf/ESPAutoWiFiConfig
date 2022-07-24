#ifndef ESP_AUTO_WIFI_CONFIG_H
#define ESP_AUTO_WIFI_CONFIG_H
// for Stream
#include <Arduino.h>  

/**
   ESPAutoWiFiConfig.h
   by Matthew Ford,  2022/04/07
   See the tutorial at https://www.forward.com.au/pfod/ESPAutoWiFiCofig/index.html

   This file needs the SafeString library.  Install from the Arduino library manager.
   
   (c)2022 Forward Computing and Control Pty. Ltd.
   NSW, Australia  www.forward.com.au
   This code may be freely used for both private and commerical use.
   Provide this copyright is maintained.
*/
// 

#if defined(ESP8266) || defined(ESP32)
// ok
#else 
#error "Only ESP8266 and ESP32 and ESP32 C3/S2/S3 etc boards supported"
#endif

// NOTE: ESPAutoWiFiConfig uses 148 bytes of EEPROM so if you are using your own make sure you allow enough if you call EEPROM.begin( ) in your code.
/* 
  For ESP8266 use 
  int ledPin = 0; for Adafruit ESP8266 HUZZAH - onboard Led connected to GPIO0
  int ledPin = 2 for ESP-01S (Blue led on GPIO2)
  int ledPin = 1 for ESP-01 (Blue led on TX (GPIO1), ESP-01 also has a Red power led)
  bool highForLedOn = false; // need to make output low to turn Adafruit ESP8266 HUZZAH's and ESP-01s and ESP-01 Led ON
  // normally led turns off when WiFi connected
  // set to true if you want the led to stay ON when connected
  
  For ESP32 use,
  int ledPin = 5; // for Sparkfun ESP32 Thing - onboard Led connected to GPIO5
  bool highForLedOn = true; // need to make output high to turn Sparkfun ESP32 Thing Led ON
  // normally led turns off when WiFi connected
  // set to false if you want the led to stay ON when connected

  For ESP32 C3/S2/S3  which pin drives the ws2812 addressable RGB led
  be careful not to use -ve pin numbers for boards that do not have ws2812 addressable RGB led
  NOTE use -ve pin value => use ws2812 driver
  For ESP32C3_RGB_LED use int pinNo = -8;  // not -ve to enable ws2818 driver
  For ESP32-S2-DevKitM-1 	use  int pinNo = -18; // not -ve to enable ws2818 driver
  For ESP32-S2-Saola-1 	use pinNo = -18;  // not -ve to enable ws2818 driver
  For ESP32-S3-DevKitC-1 	use pinNo = -48; // not -ve to enable ws2818 driver
  bool highForLedOn = true; // set to false if you wan the ws2812 to stay on when connected
  // normally led turns off when WiFi connected
*/

/**
    call this from near the top of setup() with
  if (ESPAutoWiFiConfigSetup(13,true,0)) {
      return;
   }
   // rest of setup
*/
// if ESPAutoWiFiConfigSetup() returns true the in AP mode waiting for connection to set WiFi SSID/pw/ip settings
// ledPin is the output that drives the indicator led, highForLedOn is true if +volts turns led on, else false if 0V out turns led on
// EEPROM_offset 0 unless you are using EEPROM in your code in which case pass in the size of the EEPROM your code uses
// and ESPAutoWiFiConfig will EEPROM addresses after that.
// AutoConfig uses 148 bytes of EEPROM
bool ESPAutoWiFiConfigSetup(int ledPin, bool highForLedOn, size_t EEPROM_offset);

/**
    call this from near the top of loop() with
  void loop() {
   if (ESPAutoWiFiConfigLooptoConfigLoop()) {
      return;
   }
   // rest of loop
*/
// if ESPAutoWiFiConfigLoop() returns true the in AP mode processing setting WiFi SSID/pw/ip settings
bool ESPAutoWiFiConfigLoop();

// set the RGB led colour on ESP32C OR other ESP32 board using ws2812 addressable RGB led on a GPIO pin
// ESP32C uses GPIO8 see ESP32C_RGB_LED above
/** Set RGB value for the WS2812 LED
 *
 * @param[in] red Intensity of Red color 0 to 255 (0-100%)
 * @param[in] green Intensity of Green color 0 to 255 (0-100%)
 * @param[in] blue Intensity of Green color 0 to 255 (0-100%)
 */
void ESPAutoWiFiConfig_setColor(uint8_t _red, uint8_t _green, uint8_t _blue);

// call this to enable debug out for the ESPAutoWiFiConfig code
void setESPAutoWiFiConfigDebugOut(Stream &out);

size_t getESPAutoWiFiConfigEEPROM_Size();
#endif
