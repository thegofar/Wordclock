# Craig's Wordclock<br />
An overview of my wordclock project.  Displaying the time in words!<br />
Mode selection via a capacitive touch sensor.  It currently does, time in words, dancing man!, ambient temperature, 'party' where all LEDs are lit with random colours, some debug modes which aren't called in the current build.

Currently this launches into clock mode displaying the time in words and every 3 minutes displays the temperature for a few seconds.  It also switches the display off as a power save during the night time.

The time is set at compile time in a seperate sketch 'setTime.ino' that writes to the RTC.  I then flash the main program 'wordclock v0.ino' onto the arduino to get the system running.<br />
Components:<br />
- Microcontroller - Arduino Pro Mini Atmega328, code has been adapted from Markus Backes - https://backes-markus.de/blog/ wordclock code.<br />
- Micro Flashing - achieved using FT232RL FTDI USB to TTL Serial Converter Adapter Module for Arduino<br />
- RTC - DS3231, implemented https://github.com/jarzebski/Arduino-DS3231. This library also facilitates reading temperature over I2C from the RTC.  Using LIR2032 backup battery (http://woodsgood.ca/projects/2014/10/21/the-right-rtc-battery/). <br />
- Auto Timezone - implemented https://github.com/JChristensen/Timezone<br />
- LEDs - WS2812B addressable RGB pixel strip ebay China, implemented FastLED lib http://fastled.io/<br />
- 10A 5v power supply, with supply current supplied at both ends of the strip via 2 capacitors.<br />
-  Capacitive touch sensor - Catalex TTP223B to cycle through the modes
