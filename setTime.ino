/*
  DS3231: Real-Time Clock. Simple example
  Read more: www.jarzebski.pl/arduino/komponenty/zegar-czasu-rzeczywistego-rtc-ds3231.html
  GIT: https://github.com/jarzebski/Arduino-DS3231
  Web: http://www.jarzebski.pl
  (c) 2014 by Korneliusz Jarzebski
*/

#include <Wire.h>
#include <DS3231.h>

DS3231 clock;

void setup()
{
  // Initialize DS3231
  Serial.println("Initialize DS3231");
  clock.begin();

  // Set sketch compiling time
  clock.setDateTime(__DATE__, __TIME__);
}

void loop()
{
  delay(1000);
}