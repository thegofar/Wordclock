# Craig's Wordclock<br />
An overview of my wordclock project.  Displaying the time in words!<br />

Components:<br />
*Microcontroller - Arduino Pro Mini Atmega328, code has been adapted from Markus Backes - https://backes-markus.de/blog/ wordclock code.<br />
*Flashing - achieved using FT232RL FTDI USB to TTL Serial Converter Adapter Module for Arduino<br />
*RTC - DS3231, implemented https://github.com/jarzebski/Arduino-DS3231. This library also facilitates reading temperature over I2C from the RTC.  Using LIR2032 backup battery (http://woodsgood.ca/projects/2014/10/21/the-right-rtc-battery/). <br />
*Auto Timezone - implemented https://github.com/JChristensen/Timezone<br />
*LEDs - WS2812B addressable RGB pixel strip ebay China, implemented FastLED lib http://fastled.io/<br />
*10A 5v power supply, with supply current supplied at both ends of the strip via a capacitor.<br />
