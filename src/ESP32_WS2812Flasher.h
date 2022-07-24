//ESP32_WS2812Flasher.h
/**
  This file has been pre-processed from Arduino_NeoPixel.cpp for ESP32 WS2812 ONLY
  Also methods not used  have been removed
  see Arduino_NeoPixel library for the complete code.
  Modifications
   (c)2022 Forward Computing and Control Pty. Ltd.
   NSW Australia, www.forward.com.au
   This code is not warranted to be fit for any purpose. You may only use it at your own risk.
   This code may be freely used for both private and commercial use
   Provide this copyright is maintained.
*/

// only NEO_GRB + NEO_KHZ800 defines used for ESP32 WS2812 RGB LED e.g. ESP32 C3 etc
// THIS CODE ASSUMES THERE IS ONLY ONE RGB LED IN THE STRIP
/*!
  @file Adafruit_NeoPixel.h

  This is part of Adafruit's NeoPixel library for the Arduino platform,
  allowing a broad range of microcontroller boards (most AVR boards,
  many ARM devices, ESP8266 and ESP32, among others) to control Adafruit
  NeoPixels, FLORA RGB Smart Pixels and compatible devices -- WS2811,
  WS2812, WS2812B, SK6812, etc.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  Written by Phil "Paint Your Dragon" Burgess for Adafruit Industries,
  with contributions by PJRC, Michael Miller and other members of the
  open source community.

  This file is part of the Arduino_NeoPixel library.

  Adafruit_NeoPixel is free software: you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  Adafruit_NeoPixel is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with NeoPixel.  If not, see
  <http://www.gnu.org/licenses/>.

*/

#ifndef ESP32_WS2812FLASHER_H
#define ESP32_WS2812FLASHER_H

#include <Arduino.h>
#include <PinFlasher.h>


/*!
    @brief  Class that stores state and functions for interacting with
            Adafruit NeoPixels and compatible devices.
*/
class ESP32_WS2812Flasher : public PinFlasher {

  public:
    ESP32_WS2812Flasher( int16_t pin = -1, bool invert = false);
    virtual void setColor(uint8_t r, uint8_t g, uint8_t b);

    // inherited from PinFlasher
    /**
       check if output should be changed.
       update() should be called often, atleast every loop()
    */
    //  void update();

    /**
       Set the output pin to flash.
       Call setOnOff( ) to start flashing, after calling setPin()<br>
       Multiple calls to this method with the same pinNo are ignored and do not interfere with flashing<br>
       If pinNo changes, stop any current flashing, set pin to output and OFF<br>
       else ignore this call<br>
       @param pin -- the pin number to flash
    */
    //  void setPin(int pin);

    /**
      Set the On and Off length, the period is twice this setting.
      This call does nothing if the on/off length is the same as the existing setting.<br>
      This simplifies the calling logic.<br>
      @param onOff_ms -- ms for on and also for off, i.e. half the period, duty cycle 50%<br>
      PIN_OFF (0) turns off the output<br>
      PIN_ON (-1) turns the output on <br>
      other values turn the output on for that length of time and then off for the same time
    */
    //  void setOnOff(unsigned long onOff_ms);

  private:

  protected:
    // skip this if trying to compile under ESP8266
#if defined(ESP32)    
    virtual void setOutput();
    bool canShow();
    void show();
    void setPixelColor(uint8_t r, uint8_t g, uint8_t b);
    bool is800KHz; ///< true if 800 KHz pixels
    uint16_t numBytes;  ///< Size of 'pixels' buffer below
    uint8_t *pixels;    ///< Holds LED color values (3 or 4 bytes each)
    uint8_t pixelArray[3]; // 4bytes x 1 led
    uint8_t rOffset;    ///< Red index within each 3- or 4-byte pixel
    uint8_t gOffset;    ///< Index of green byte
    uint8_t bOffset;    ///< Index of blue byte
    uint32_t endTime;   ///< Latch timing reference
    uint8_t red;
    uint8_t green;
    uint8_t blue;
#endif    
};

#endif
