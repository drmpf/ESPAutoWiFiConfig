// ESP32_WS2812Flasher.cpp
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

/*!
   @file Arduino_NeoPixel.cpp

   @mainpage Arduino Library for driving Adafruit NeoPixel addressable LEDs,
   FLORA RGB Smart Pixels and compatible devicess -- WS2811, WS2812, WS2812B,
   SK6812, etc.

   @section intro_sec Introduction

   This is the documentation for Adafruit's NeoPixel library for the
   Arduino platform, allowing a broad range of microcontroller boards
   (most AVR boards, many ARM devices, ESP8266 and ESP32, among others)
   to control Adafruit NeoPixels, FLORA RGB Smart Pixels and compatible
   devices -- WS2811, WS2812, WS2812B, SK6812, etc.

   Adafruit invests time and resources providing this open source code,
   please support Adafruit and open-source hardware by purchasing products
   from Adafruit!

   @section author Author

   Written by Phil "Paint Your Dragon" Burgess for Adafruit Industries,
   with contributions by PJRC, Michael Miller and other members of the
   open source community.

   @section license License

   This file is part of the Arduino_NeoPixel library.

   Arduino_NeoPixel is free software: you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   Arduino_NeoPixel is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with NeoPixel. If not, see
   <http://www.gnu.org/licenses/>.
*/

#include "ESP32_WS2812Flasher.h"

#if defined(ESP8266)

// just use normal PinFlasher if a -ve pin passed in to ESPAutoWiFiConfig
ESP32_WS2812Flasher::ESP32_WS2812Flasher(int16_t p, bool invert)
  : PinFlasher(p,invert) {
}

#elif defined(ESP32)
// try to control ws2812 rgb led if a -ve pin passed in to ESPAutoWiFiConfig

// RGB NeoPixel permutations; white and red offsets are always same
// Offset:         W        R        G        B
#define NEO_GRB ((1 << 6) | (1 << 4) | (0 << 2) | (2)) ///< Transmit as G,R,B

/*!
  @param   p  Arduino pin number which will drive the NeoPixel data in.
*/
ESP32_WS2812Flasher::ESP32_WS2812Flasher(int16_t p, bool invert)
  : PinFlasher(p), is800KHz(true), endTime(0) {
  outputInverted = invert; // for ws2812 inverted output means PIN_OFF leave led on
  //updateType(NEO_GRB + NEO_KHZ800);
  rOffset = (NEO_GRB >> 4) & 0b11; // regarding R/G/B/W offsets
  gOffset = (NEO_GRB >> 2) & 0b11;
  bOffset = NEO_GRB & 0b11;
  pixels = pixelArray;
  numBytes = 3;
  // set light green default
  red = 0;
  green = 128;
  blue = 0;
}


/**
   set the output based on io_pin, io_pin_on and outputInverted.
   This is a non-public helper method
*/
void ESP32_WS2812Flasher::setOutput() { // uses class vars io_pin and io_pin_on
  if (io_pin < 0) {
    return;
  }
  if (io_pin_on) {
    if (!outputInverted) {
      setPixelColor(red, green, blue); //on
    } else {
      setPixelColor(0, 0, 0); // on inverted => off
    }
  } else { // off
    if (!outputInverted) {
      setPixelColor(0, 0, 0); // off
    } else {
      setPixelColor(red, green, blue); // off inverted => on
    }
  }
  show();
}


// ESP32 show() is external to enforce ICACHE_RAM_ATTR execution
extern "C" IRAM_ATTR void espShow(uint16_t io_pin, uint8_t *pixels, uint32_t numBytes, uint8_t type);

/*!
  @brief   Check whether a call to show() will start sending data
           immediately or will 'block' for a required interval. NeoPixels
           require a short quiet time (about 300 microseconds) after the
           last bit is received before the data 'latches' and new data can
           start being received. Usually one's sketch is implicitly using
           this time to generate a new frame of animation...but if it
           finishes very quickly, this function could be used to see if
           there's some idle time available for some low-priority
           concurrent task.
  @return  1 or true if show() will start sending immediately, 0 or false
           if show() would block (meaning some idle time is available).
*/
bool ESP32_WS2812Flasher::canShow(void) {
  // It's normal and possible for endTime to exceed micros() if the
  // 32-bit clock counter has rolled over (about every 70 minutes).
  // Since both are uint32_t, a negative delta correctly maps back to
  // positive space, and it would seem like the subtraction below would
  // suffice. But a problem arises if code invokes show() very
  // infrequently...the micros() counter may roll over MULTIPLE times in
  // that interval, the delta calculation is no longer correct and the
  // next update may stall for a very long time. The check below resets
  // the latch counter if a rollover has occurred. This can cause an
  // extra delay of up to 300 microseconds in the rare case where a
  // show() call happens precisely around the rollover, but that's
  // neither likely nor especially harmful, vs. other code that might
  // stall for 30+ minutes, or having to document and frequently remind
  // and/or provide tech support explaining an unintuitive need for
  // show() calls at least once an hour.
  uint32_t now = micros();
  if (endTime > now) {
    endTime = now;
  }
  return (now - endTime) >= 300L;
}


/*!
  @brief   Transmit pixel data in RAM to NeoPixels.
*/
void ESP32_WS2812Flasher::show(void) {
  if (io_pin < 0) {
    return;
  }
  // Data latch = 300+ microsecond pause in the output stream. Rather than
  // put a delay at the end of the function, the ending time is noted and
  // the function will simply hold off (if needed) on issuing the
  // subsequent round of data until the latch time has elapsed. This
  // allows the mainline code to start generating the next frame of data
  // rather than stalling for the latch.
  while (!canShow()) {
    ;
  }
  // ESP8266 ----------------------------------------------------------------

  // ESP8266 show() is external to enforce ICACHE_RAM_ATTR execution
  espShow(io_pin, pixels, numBytes, is800KHz);

  // END ARCHITECTURE SELECT ------------------------------------------------

  endTime = micros(); // Save EOD time for latch on next call
}


/*!
  @brief   Set a pixel's color using separate red, green and blue
           components. If using RGBW pixels, white will be set to 0.
  @param   r  Red brightness, 0 = minimum (off), 255 = maximum.
  @param   g  Green brightness, 0 = minimum (off), 255 = maximum.
  @param   b  Blue brightness, 0 = minimum (off), 255 = maximum.
*/
void ESP32_WS2812Flasher::setColor(uint8_t r, uint8_t g, uint8_t b) {
  red = r;
  green = g;
  blue = b;
  setOutput(); // update now
}

/*!
  @brief   Set a pixel's color using separate red, green and blue
           components. If using RGBW pixels, white will be set to 0.
  @param   r  Red brightness, 0 = minimum (off), 255 = maximum.
  @param   g  Green brightness, 0 = minimum (off), 255 = maximum.
  @param   b  Blue brightness, 0 = minimum (off), 255 = maximum.
*/
void ESP32_WS2812Flasher::setPixelColor(uint8_t _r, uint8_t _g,  uint8_t _b) {
  // size_t n = 0; // only handle one led
  uint8_t *p;
  p = &pixels[0 * 3];     // 3 bytes per pixel  n is always 0 here
  p[rOffset] = _r; // R,G,B always stored
  p[gOffset] = _g;
  p[bOffset] = _b;
}

#elif
#error "Only ESP8266 and ESP32 and ESP32 C3/S2/S3 etc boards supported"
#endif
