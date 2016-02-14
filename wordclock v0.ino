/* 
Adapted by C Barry 14/02/2015 23.00
Original work credit to Markus Backes - https://backes-markus.de/blog/
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//Library includes
#include <FastLED.h>
#include <Wire.h>
#include <Time.h>
#include "DS3231.h" //jarzebski/Arduino-DS3231 
#include "Timezone.h" //https://github.com/JChristensen/Timezone
#include "defaultLayout.h"

//LED defines
#define NUM_LEDS 110

//PIN defines
#define STRIP_DATA_PIN 6
#define ARDUINO_LED 13 //Default Arduino LED
#define BOTTOMRIGHTTOUCH 2
#define LDR_PIN A3

#define MAX_MODE 2  // off, clock, party, heart, fast, digi

// The RTC was synched in GMT
DS3231 RTC;
bool timeInSync = false;
TimeChangeRule myBST = {"BST", Last, Sun, Mar, 1, +60};
TimeChangeRule mySTD = {"GMT", Last, Sun, Oct, 2, 0}; //GMT == UTC
Timezone myTZ(myBST, mySTD);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
time_t utc, local;

uint8_t strip[NUM_LEDS];
uint8_t stackptr = 0;

CRGB leds[NUM_LEDS];

boolean autoBrightnessEnabled = true;
int displayMode = 0;
int cycleDisplayMode=0;

CRGB defaultColor = CRGB::White;
uint8_t colorIndex = 0;

int testHours = 0;
int testMinutes = 0;

//multitasking helper
const long oneSecondDelay = 1000;

long waitUntilParty = 0;
long waitUntilOff = 0;
long waitUntilFastTest = 0;
long waitUntilHeart = 0;
long waitUntilLDR = 0;
long waitUntilRtc = 0;
long waitUntilTemp = 0;
long waitUntilTouch = 0;
long ledBlink = 0;
long cycle_display = 0;
int state = LOW;

int digiNumber = 1;
int temp=0;
//forward declaration
void fastTest();
void clockLogic();
void doLDRLogic();
void doTouchSensorLogic();
void makeParty();
void switchoffoff();
void showHeart();
void showTemperature();
//void ThirtySecCountDown();
void DigiNumberTest();
void pushToStrip(int ledId);
void resetAndBlack();
void resetStrip();
void displayStripRandomColor();
void displayStrip();
void displayStrip(CRGB colorCode);
void timeToStrip(uint8_t hours,uint8_t minutes);


//#define DEBUG 1

#ifdef DEBUG
	#define DEBUG_PRINT(str)  Serial.println (str)
#else
	#define DEBUG_PRINT(str)
#endif

void setup() {
	
	#ifdef DEBUG
		Serial.begin(9600);
	#endif
	
	pinMode(ARDUINO_LED, OUTPUT);
	pinMode(BOTTOMRIGHTTOUCH, INPUT);
	//pinMode(TOPRIGHTTOUCH, INPUT);
	
	//setup leds incl. fastled
	for(int i = 0; i<NUM_LEDS; i++) {
		strip[i] = 0;
	}
	FastLED.addLeds<WS2812B, STRIP_DATA_PIN, GRB>(leds, NUM_LEDS);
	resetAndBlack();
	displayStrip();
	
	//setup rtc
	Wire.begin();
	RTC.begin();
	setSyncInterval(3600); //every hour
	setSyncProvider(getRTCTime);
	DEBUG_PRINT("Waiting for DS3231 time ... ");
	digitalWrite(13,HIGH);
	delay(3000);
	digitalWrite(13,LOW);
	while(timeStatus()== timeNotSet) {
		// wait until the time is set by the sync provider
		DEBUG_PRINT(".");
		delay(2000);
	}
}

void loop() {
	doLDRLogic();
	doTouchSensorLogic();
	runDisplayModeLogic(displayMode);
}

time_t getRTCTime() 
{
	RTCDateTime RTCtime = RTC.getDateTime();
	// Indicator that a time check is done
	if (RTCtime.year!=0) {
		DEBUG_PRINT("sync");
	}

	return RTCtime.unixtime+3600;
}

void runDisplayModeLogic(int disp_mode)
{
	switch(disp_mode) {
		case 0:
			clockLogic();
			break;
		case 1:
			cycleMode();
			break;
		case 2:
			showTemperature();
			break;
		case 3:
			makeParty();
			break;
		case 4:
			fastTest();
			break;
		case 5:
			DigiNumberTest();                       
		default:
			clockLogic();
			break;
	}
}

void cycleMode()
{
	if(millis()>cycle_display)
	{
        resetAndBlack();
		if(cycleDisplayMode == 5)
		{
			cycleDisplayMode = 0;
		}
		else
		{
			cycleDisplayMode++;
		}
		if(cycleDisplayMode=1)
		{
			// skip cycle display mode, as we are in that mode!
			cycleDisplayMode = 2;
		}
		runDisplayModeLogic(cycleDisplayMode);
		cycle_display+=15000;
	}
}

void doLDRLogic() {
	if(millis() >= waitUntilLDR && autoBrightnessEnabled) {
		DEBUG_PRINT("doing LDR logic");
		waitUntilLDR = millis();
		int ldrVal = map(analogRead(LDR_PIN), 300, 1023, 150, 0);
		FastLED.setBrightness(255-ldrVal);
		FastLED.show();
		DEBUG_PRINT(ldrVal);
		waitUntilLDR += oneSecondDelay;
	}
}

void doTouchSensorLogic()
{
	if(millis() >=waitUntilTouch)
	{
		// uses the TTP223B capacitive touch sensor
		int brtouch = digitalRead(BOTTOMRIGHTTOUCH);
		//int trtouch = digitalRead(TOPRIGHTTOUCH);
		
		if(brtouch==HIGH)
		{
			waitUntilTouch=+500;
			// cycle through the display modes
			if(displayMode==MAX_MODE)
			{
				displayMode = 0;
			}
			else
			{
				displayMode++;
			}
			DEBUG_PRINT("changing mode to " + displayMode);
		}
		waitUntilTouch +=250;
	}
}

///////////////////////
//DISPLAY MODES
///////////////////////
void clockLogic() {
	if(millis() >= waitUntilRtc) {
		DEBUG_PRINT("doing clock logic");
		waitUntilRtc = millis();
		utc = now();
		local = myTZ.toLocal(utc, &tcr);
		if(testMinutes != minute(local) || testHours != hour(local)) {
			DEBUG_PRINT(testHours + " " + testMinutes);
			testMinutes = minute(local);
			testHours = hour(local);
			resetAndBlack();
			timeToStrip(testHours, testMinutes);
			displayStrip(defaultColor);
		}
		waitUntilRtc += oneSecondDelay;
	}
}

void switchoffoff() {
	if(millis() >= waitUntilOff) {
		DEBUG_PRINT("switching off");
		waitUntilOff = millis();
		resetAndBlack();
		displayStrip(CRGB::Black);
		waitUntilOff += oneSecondDelay;
	}
}

void makeParty() {
	if(millis() >= waitUntilParty) {
		autoBrightnessEnabled = false;
		DEBUG_PRINT("YEAH party party");
		waitUntilParty = millis();
		resetAndBlack();
		for(int i = 0; i<NUM_LEDS;i++) {
			leds[i] = CHSV(random(0, 255), 255, 255);
		}
		FastLED.show();
		waitUntilParty += oneSecondDelay;
	}
}

void showHeart() {
	if(millis() >= waitUntilHeart) {
		autoBrightnessEnabled = false;
		DEBUG_PRINT("showing heart");
		waitUntilHeart = millis();
		resetAndBlack();
		pushToStrip(L29); pushToStrip(L30); pushToStrip(L70); pushToStrip(L89);
		pushToStrip(L11); pushToStrip(L48); pushToStrip(L68); pushToStrip(L91);
		pushToStrip(L7); pushToStrip(L52); pushToStrip(L107);
		pushToStrip(L6); pushToStrip(L106);
		pushToStrip(L5); pushToStrip(L105);
		pushToStrip(L15); pushToStrip(L95);
		pushToStrip(L23); pushToStrip(L83);
		pushToStrip(L37); pushToStrip(L77);
		pushToStrip(L41); pushToStrip(L61);
		pushToStrip(L59);
		displayStrip(CRGB::Red);
		waitUntilHeart += oneSecondDelay;
	}
}

void showTemperature()
{
	if(millis() >= waitUntilTemp) {
		autoBrightnessEnabled = false;
		DEBUG_PRINT("showing temperature");
		resetAndBlack();
		int temp=(int) (RTC.readTemperature() + 0.5);
		pushDEGREES_CENTIGRADE();
		switch(temp)
		{
			case(14):
				dispDIGI_FOURTEEN();
				break;
			case(15):
				dispDIGI_FIFTEEN();
				break;
			case(16):
				dispDIGI_SIXTEEN();
				break;
			case(17):
				dispDIGI_SEVENTEEN();
				break;
			case(18):
				dispDIGI_EIGHTEEN();
				break;
			case(19):
				dispDIGI_NINETEEN();
				break;
			case(20):
				dispDIGI_TWENTY();
				break;
			case(21):
				dispDIGI_TWENTYONE();
				break;
			case(22):
				dispDIGI_TWENTYTWO();
				break;
			case(23):
				dispDIGI_TWENTYTHREE();
				break;
			case(24):
				dispDIGI_TWENTYFOUR();
				break;
			case(25):
				dispDIGI_TWENTYFIVE();
				break;
			case(26):
				dispDIGI_TWENTYSIX();
				break;
			case(27):
				dispDIGI_TWENTYSEVEN();
				break;
			case(28):
				dispDIGI_TWENTYEIGHT();
				break;
			case(29):
				dispDIGI_TWENTYNINE();
				break;
			case(30):
				dispDIGI_THIRTY();
				break;
			case(31):
				dispDIGI_THIRTYONE();
				break;
			case(32):
				dispDIGI_THIRTYTWO();
				break;
			case(33):
				dispDIGI_THIRTYTHREE();
				break;
			case(34):
				dispDIGI_THIRTYFOUR();
				break;
			case(35):
				dispDIGI_THIRTYFIVE();
				break;
			case(36):
				dispDIGI_THIRTYSIX();
				break;
			case(37):
				dispDIGI_THIRTYSEVEN();
				break;
			case(38):
				dispDIGI_THIRTYEIGHT();
				break;
			case(39):
				dispDIGI_THIRTYNINE();
				break;
			case(40):
				dispDIGI_FORTY();
				break;			
			default:
				dispDIGI_FORTY();
				break;
		}
		displayStrip(CRGB::Red);
		waitUntilTemp += oneSecondDelay;
	}
}

void DigiNumberTest()
{
	resetAndBlack();
	switch(digiNumber)
	{
	case(1):
	pushDIGI_ONE();
	break;
	case(2):
	pushDIGI_TWO();
	break;
	case(3):
	pushDIGI_THREE();
	break;
	case(4):
	pushDIGI_FOUR();
	break;
	case(5):
	pushDIGI_FIVE();
	break;
	case(6):
	pushDIGI_SIX();
	break;
	case(7):
	pushDIGI_SEVEN();
	break;
	case(8):
	pushDIGI_EIGHT();
	break;
	case(9):
	pushDIGI_NINE();
	break;
	case(10):	
	dispDIGI_TEN();
	break;
	case(11):
	dispDIGI_ELEVEN();
	break;
	case(12):
	dispDIGI_TWELVE();
	break;
	case(13):
	dispDIGI_THIRTEEN();
	break;
	case(14):
	dispDIGI_FOURTEEN();
	break;
	case(15):
	dispDIGI_FIFTEEN();
	break;
	case(16):
	dispDIGI_SIXTEEN();
	break;
	case(17):
	dispDIGI_SEVENTEEN();
	break;
	case(18):
	dispDIGI_EIGHTEEN();
	break;
	case(19):
	dispDIGI_NINETEEN();
	break;
	case(20):
	dispDIGI_TWENTY();
	break;
	case(21):
	dispDIGI_TWENTYONE();
	break;
	case(22):
	dispDIGI_TWENTYTWO();
	break;
	case(23):
	dispDIGI_TWENTYTHREE();
	break;
	/*dispDIGI_TWENTYFOUR();

	dispDIGI_TWENTYFIVE();

	dispDIGI_TWENTYSIX();

	dispDIGI_TWENTYSEVEN();

	dispDIGI_TWENTYEIGHT();

	dispDIGI_TWENTYNINE();

	dispDIGI_THIRTY();

	dispDIGI_THIRTYONE();

	dispDIGI_THIRTYTWO();

	dispDIGI_THIRTYTHREE();

	dispDIGI_THIRTYFOUR();

	dispDIGI_THIRTYFIVE();

	dispDIGI_THIRTYSIX();

	dispDIGI_THIRTYSEVEN();

	dispDIGI_THIRTYEIGHT();

	dispDIGI_THIRTYNINE();*/
	default:
	dispDIGI_FORTY();
	break;
	}
	displayStrip();
	delay(500);
	if(digiNumber == 20)
	{
		digiNumber = 1;
	}
	else
	{
		digiNumber++;
	}
}

void fastTest() {
	if(millis() >= waitUntilFastTest) {
		autoBrightnessEnabled = false;
		DEBUG_PRINT("showing heart");
		waitUntilFastTest = millis();
		if(testMinutes >= 60) {
			testMinutes = 0;
			testHours++;
		}
		if(testHours >= 24) {
			testHours = 0;
		}
		
		//Array leeren
		resetAndBlack();
		timeToStrip(testHours, testMinutes);
		displayStripRandomColor();
		testMinutes++;
		waitUntilFastTest += oneSecondDelay;
	}
}
///////////////////////

CRGB prevColor() {
	if(colorIndex > 0) {
		colorIndex--;
	}
	return getColorForIndex();
}
CRGB nextColor() {
	if(colorIndex < 9) {
		colorIndex++;
	}
	return getColorForIndex();
}

CRGB getColorForIndex() {
	switch(colorIndex) {
		case 0:
			return CRGB::White;
		case 1:
			return CRGB::Blue;
		case 2:
			return CRGB::Aqua;
		case 3:
			return CRGB::Green;
		case 4:
			return CRGB::Lime;
		case 5:
			return CRGB::Red;
		case 6:
			return CRGB::Magenta;
		case 7:
			return CRGB::Olive;
		case 8:
			return CRGB::Yellow;
		case 9:
			return CRGB::Silver;
		default:
			colorIndex = 0;
			return CRGB::White;
	}
}

void pushToStrip(int ledId) {
	strip[stackptr] = ledId;
	stackptr++;
}

void resetAndBlack() {
	resetStrip();
	for(int i = 0; i<NUM_LEDS; i++) {
		leds[i] = CRGB::Black;
	}
}

void resetStrip() {
	stackptr = 0;
	for(int i = 0; i<NUM_LEDS; i++) {
		strip[i] = 0;
	}
}

void displayStripRandomColor() {
	for(int i = 0; i<stackptr; i++) {
		leds[strip[i]] = CHSV(random(0, 255), 255, 255);
	}
	FastLED.show();
}

void displayStrip() {
	displayStrip(defaultColor);
}

void displayStrip(CRGB colorCode) {
	for(int i = 0; i<stackptr; i++) {
		leds[strip[i]] = colorCode;
	}
	FastLED.show();
}

void timeToStrip(uint8_t hours,uint8_t minutes)
{
	pushITS();

	//show minutes
	if(minutes >= 5-2 && minutes < 10-2) {
		pushFIVE_MINS();
		pushPAST();
	} else if(minutes >= 10-2 && minutes < 15-2) {
		pushTEN_MINS();
		pushPAST();
	} else if(minutes >= 15-2 && minutes < 20-2) {
		pushQUARTER();
		pushPAST();
	} else if(minutes >= 20-2 && minutes < 25-2) {
		pushTWENTY();
		pushPAST();
	} else if(minutes >= 25-2 && minutes < 30-2) {
		pushTWENTY();
		pushFIVE_MINS();
		pushPAST();
	} else if(minutes >= 30-2 && minutes < 35-2) {
		pushHALF();
		pushPAST();
	} else if(minutes >= 35-2 && minutes < 40-2) {
		pushTWENTY();
		pushFIVE_MINS();
		pushTO();
	} else if(minutes >= 40-2 && minutes < 45-2) {
		pushTWENTY();
		pushTO();
	} else if(minutes >= 45-2 && minutes < 50-2) {
		pushQUARTER();
		pushTO();
	} else if(minutes >= 50-2 && minutes < 55-2) {
		pushTEN_MINS();
		pushTO();
	} else if(minutes >= 55-2 && minutes < 60-2) {
		pushFIVE_MINS();
		pushTO();
	}
	

	if(hours >= 12) {
		hours -= 12;
	}

	if(minutes >= 35-2) {
		hours++;
	}

	if(hours == 12) {
		hours = 0;
	}

	//show hours
	switch(hours) {
		case 0:
			pushTWELVE();
			break;
		case 1:
			pushONE();
			break;
		case 2:
			pushTWO();
			break;
		case 3:
			pushTHREE();
			break;
		case 4:
			pushFOUR();
			break;
		case 5:
			pushFIVE_HRS();
			break;
		case 6:
			pushSIX();
			break;
		case 7:
			pushSEVEN();
			break;
		case 8:
			pushEIGHT();
			break;
		case 9:
			pushNINE();
			break;
		case 10:
			pushTEN();
			break;
		case 11:
			pushELEVEN();
			break;
	}
	
	//show o'clock
	if((minutes >= 60-2)||(minutes < 5-2)){
		pushOCLOCK();
	}
}

///////////////////////
//PUSH WORD HELPER
///////////////////////
void pushITS()  {
	pushToStrip(L105);
	pushToStrip(L104);
	pushToStrip(L103);
}

void pushFIVE_MINS() {
	pushToStrip(L78);
	pushToStrip(L79);
	pushToStrip(L80);
	pushToStrip(L81);
}

void pushTEN_MINS() {
	pushToStrip(L71);
	pushToStrip(L72);
	pushToStrip(L73);
}

void pushQUARTER() {
	pushToStrip(L90);
	pushToStrip(L91);
	pushToStrip(L92);
	pushToStrip(L93);
	pushToStrip(L94);
	pushToStrip(L95);
	pushToStrip(L96);
}

void pushTWENTY() {
	pushToStrip(L87);
	pushToStrip(L86);
	pushToStrip(L85);
	pushToStrip(L84);
	pushToStrip(L83);
	pushToStrip(L82);
}

void pushHALF() {
	pushToStrip(L66);
	pushToStrip(L67);
	pushToStrip(L68);
	pushToStrip(L69);
}

void pushPAST() {
	pushToStrip(L62);
	pushToStrip(L63);
	pushToStrip(L64);
	pushToStrip(L65);
}

void pushTO() {
	pushToStrip(L75);
	pushToStrip(L76);
}

// HOURS
void pushONE() {
	pushToStrip(L44);
	pushToStrip(L45);
	pushToStrip(L46);
}

void pushTWO() {
	pushToStrip(L35);
	pushToStrip(L34);
	pushToStrip(L33);
}

void pushTHREE() {
	pushToStrip(L50);
	pushToStrip(L51);
	pushToStrip(L52);
	pushToStrip(L53);
	pushToStrip(L54);
}

void pushFOUR() {
	pushToStrip(L43);
	pushToStrip(L42);
	pushToStrip(L41);
	pushToStrip(L40);
}

void pushFIVE_HRS() {
	pushToStrip(L36);
	pushToStrip(L37);
	pushToStrip(L38);
	pushToStrip(L39);
}

void pushSIX() {
	pushToStrip(L47);
	pushToStrip(L48);
	pushToStrip(L49);
}

void pushSEVEN() {
	pushToStrip(L21);
	pushToStrip(L20);
	pushToStrip(L19);
	pushToStrip(L18);
	pushToStrip(L17);
}

void pushEIGHT() {
	pushToStrip(L22);
	pushToStrip(L23);
	pushToStrip(L24);
	pushToStrip(L25);
	pushToStrip(L26);
}

void pushNINE() {
	pushToStrip(L58);
	pushToStrip(L57);
	pushToStrip(L56);
	pushToStrip(L55);
}

void pushTEN() {
	pushToStrip(L0);
	pushToStrip(L1);
	pushToStrip(L2);
}

void pushTEN_HRS() {
	pushToStrip(L0);
	pushToStrip(L1);
	pushToStrip(L2);
}

void pushELEVEN() {
	pushToStrip(L27);
	pushToStrip(L28);
	pushToStrip(L29);
	pushToStrip(L30);
	pushToStrip(L31);
	pushToStrip(L32);
}

void pushTWELVE() {
	pushToStrip(L16);
	pushToStrip(L15);
	pushToStrip(L14);
	pushToStrip(L13);
	pushToStrip(L12);
	pushToStrip(L11);
}

void pushOCLOCK() {
	pushToStrip(L5);
	pushToStrip(L6);
	pushToStrip(L7);
	pushToStrip(L8);
	pushToStrip(L9);
	pushToStrip(L10);
}
///////////////////////

void pushDEGREES_CENTIGRADE()
{
	pushToStrip(L99);
}
///////////////////////
/* Digital Numerics */
///////////////////////
void pushDIGI_ONE()
{
	pushToStrip(L17);pushToStrip(L16);pushToStrip(L15);
	pushToStrip(L27);
	pushToStrip(L38);
	pushToStrip(L49);
	pushToStrip(L60);
	pushToStrip(L70);pushToStrip(L71);
	pushToStrip(L82);
}
void pushDIGI_TWO()
{
	pushToStrip(L18);pushToStrip(L17);pushToStrip(L16);pushToStrip(L15);pushToStrip(L14);
	pushToStrip(L25);
	pushToStrip(L39);
	pushToStrip(L49);pushToStrip(L50);
	pushToStrip(L58);
	pushToStrip(L69);pushToStrip(L73);
	pushToStrip(L83);pushToStrip(L82);pushToStrip(L81);
}
void pushDIGI_THREE()
{
	pushToStrip(L17);pushToStrip(L16);pushToStrip(L15);
	pushToStrip(L25);pushToStrip(L29);
	pushToStrip(L36);
	pushToStrip(L49);pushToStrip(L50);pushToStrip(L48);
	pushToStrip(L58);
	pushToStrip(L69);pushToStrip(L73);
	pushToStrip(L83);pushToStrip(L82);pushToStrip(L81);
}
void pushDIGI_FOUR()
{
	pushToStrip(L15);
	pushToStrip(L28);
	pushToStrip(L36);pushToStrip(L37);pushToStrip(L38);pushToStrip(L39);pushToStrip(L40);
	pushToStrip(L47);pushToStrip(L50);
	pushToStrip(L61);pushToStrip(L59);
	pushToStrip(L71);pushToStrip(L72);
	pushToStrip(L81);
}
void pushDIGI_FIVE()
{
	pushToStrip(L17);pushToStrip(L16);pushToStrip(L15);
	pushToStrip(L25);pushToStrip(L29);
	pushToStrip(L36);
	pushToStrip(L49);pushToStrip(L50);pushToStrip(L48);
	pushToStrip(L58);
	pushToStrip(L69);
	pushToStrip(L84);pushToStrip(L83);pushToStrip(L82);pushToStrip(L81);pushToStrip(L80);
}
void pushDIGI_SIX()
{
	pushToStrip(L17);pushToStrip(L16);pushToStrip(L15);
	pushToStrip(L25);pushToStrip(L29);
	pushToStrip(L36);pushToStrip(L40);
	pushToStrip(L47);pushToStrip(L48);pushToStrip(L49);pushToStrip(L50);
	pushToStrip(L62);
	pushToStrip(L70);
	pushToStrip(L81);pushToStrip(L82);
}
void pushDIGI_SEVEN()
{
	pushToStrip(L17);
	pushToStrip(L26);
	pushToStrip(L39);
	pushToStrip(L49);
	pushToStrip(L59);
	pushToStrip(L73);
	pushToStrip(L84);pushToStrip(L83);pushToStrip(L82);pushToStrip(L81);pushToStrip(L80);
}
void pushDIGI_EIGHT()
{
	pushToStrip(L81);pushToStrip(L82);pushToStrip(L83);	
	pushToStrip(L69);pushToStrip(L73);
	pushToStrip(L62);pushToStrip(L58);
	pushToStrip(L48);pushToStrip(L49);pushToStrip(L50);
	pushToStrip(L40);pushToStrip(L36);
	pushToStrip(L25);pushToStrip(L29);
	pushToStrip(L17);pushToStrip(L16);pushToStrip(L15);
}
void pushDIGI_NINE()
{
	pushToStrip(L81);pushToStrip(L82);pushToStrip(L83);	
	pushToStrip(L69);pushToStrip(L73);
	pushToStrip(L62);pushToStrip(L58);
	pushToStrip(L48);pushToStrip(L49);pushToStrip(L50);pushToStrip(L51);
	pushToStrip(L36);
	pushToStrip(L28);
	pushToStrip(L17);pushToStrip(L16);
}
void dispDIGI_TEN()
{
	pushTENS_ONE();
	pushUNITS_ZERO();
}
void dispDIGI_ELEVEN()
{
	pushTENS_ONE();
	pushUNITS_ONE();
}
void dispDIGI_TWELVE()
{
	pushTENS_ONE();
	pushUNITS_TWO();
}
void dispDIGI_THIRTEEN()
{
	pushTENS_ONE();
	pushUNITS_THREE();
}
void dispDIGI_FOURTEEN()
{
	pushTENS_ONE();
	pushUNITS_FOUR();
}
void dispDIGI_FIFTEEN()
{
	pushTENS_ONE();
	pushUNITS_FIVE();
}
void dispDIGI_SIXTEEN()
{
	pushTENS_ONE();
	pushUNITS_SIX();
}
void dispDIGI_SEVENTEEN()
{
	pushTENS_ONE();
	pushUNITS_SEVEN();
}
void dispDIGI_EIGHTEEN()
{
	pushTENS_ONE();
	pushUNITS_EIGHT();
}
void dispDIGI_NINETEEN()
{
	pushTENS_ONE();
	pushUNITS_NINE();
}
void dispDIGI_TWENTY()
{
	pushTENS_TWO();
	pushUNITS_ZERO();
}
void dispDIGI_TWENTYONE()
{
	pushTENS_TWO();
	pushUNITS_ONE();
}
void dispDIGI_TWENTYTWO()
{
	pushTENS_TWO();
	pushUNITS_TWO();
}
void dispDIGI_TWENTYTHREE()
{
	pushTENS_TWO();
	pushUNITS_THREE();
}
void dispDIGI_TWENTYFOUR()
{
	pushTENS_TWO();
	pushUNITS_FOUR();
}
void dispDIGI_TWENTYFIVE()
{
	pushTENS_TWO();
	pushUNITS_FIVE();
}
void dispDIGI_TWENTYSIX()
{
	pushTENS_TWO();
	pushUNITS_SIX();
}
void dispDIGI_TWENTYSEVEN()
{
	pushTENS_TWO();
	pushUNITS_SEVEN();
}
void dispDIGI_TWENTYEIGHT()
{
	pushTENS_TWO();
	pushUNITS_EIGHT();
}
void dispDIGI_TWENTYNINE()
{
	pushTENS_TWO();
	pushUNITS_NINE();
}
void dispDIGI_THIRTY()
{
	pushTENS_THREE();
    pushUNITS_ZERO();
}
void dispDIGI_THIRTYONE()
{
	pushTENS_THREE();
    pushUNITS_ONE();
}
void dispDIGI_THIRTYTWO()
{
	pushTENS_THREE();
    pushUNITS_TWO();
}
void dispDIGI_THIRTYTHREE()
{
	pushTENS_THREE();
    pushUNITS_THREE();
}
void dispDIGI_THIRTYFOUR()
{
	pushTENS_THREE();
    pushUNITS_FOUR();
}
void dispDIGI_THIRTYFIVE()
{
	pushTENS_THREE();
    pushUNITS_FIVE();
}
void dispDIGI_THIRTYSIX()
{
	pushTENS_THREE();
    pushUNITS_SIX();
}
void dispDIGI_THIRTYSEVEN()
{
	pushTENS_THREE();
    pushUNITS_SEVEN();
}
void dispDIGI_THIRTYEIGHT()
{
	pushTENS_THREE();
    pushUNITS_EIGHT();
}
void dispDIGI_THIRTYNINE()
{
	pushTENS_THREE();
    pushUNITS_NINE();
}
void dispDIGI_FORTY()
{
	pushTENS_FOUR();
    pushUNITS_ZERO();
}
void pushUNITS_ZERO()
{
	pushToStrip(L12);pushToStrip(L13);pushToStrip(L14);
	pushToStrip(L32);pushToStrip(L28);
	pushToStrip(L33);pushToStrip(L36);pushToStrip(L37);
	pushToStrip(L50);pushToStrip(L52);pushToStrip(L54);
	pushToStrip(L55);pushToStrip(L56);pushToStrip(L59);
	pushToStrip(L72);pushToStrip(L76);
	pushToStrip(L80);pushToStrip(L79);pushToStrip(L78);
}
void pushUNITS_ONE()
{
	pushToStrip(L11);pushToStrip(L12);pushToStrip(L13);pushToStrip(L14);pushToStrip(L15);
	pushToStrip(L30);
	pushToStrip(L35);
	pushToStrip(L52);
	pushToStrip(L57);pushToStrip(L59);
	pushToStrip(L74); pushToStrip(L73);
	pushToStrip(L79);
}
void pushUNITS_TWO()
{
	pushToStrip(L11);pushToStrip(L12);pushToStrip(L13);pushToStrip(L14);pushToStrip(L15);
	pushToStrip(L29);
	pushToStrip(L35);
	pushToStrip(L53);
	pushToStrip(L55);
	pushToStrip(L76); pushToStrip(L72);
	pushToStrip(L80); pushToStrip(L79);	pushToStrip(L78);
}
void pushUNITS_THREE()
{
	pushToStrip(L12);pushToStrip(L13);pushToStrip(L14);
	pushToStrip(L32);pushToStrip(L28);
	pushToStrip(L33);
	pushToStrip(L53);pushToStrip(L52);
	pushToStrip(L55);
	pushToStrip(L76); pushToStrip(L72);
	pushToStrip(L80); pushToStrip(L79);	pushToStrip(L78);
}
void pushUNITS_FOUR()
{
	pushToStrip(L12);
	pushToStrip(L31);
	pushToStrip(L33);pushToStrip(L34);pushToStrip(L35);pushToStrip(L36);pushToStrip(L37);
	pushToStrip(L50);pushToStrip(L53);
	pushToStrip(L58);pushToStrip(L56);
	pushToStrip(L75);pushToStrip(L74);
	pushToStrip(L78);
}
void pushUNITS_FIVE()
{
	pushToStrip(L77);pushToStrip(L78);pushToStrip(L79);pushToStrip(L80);pushToStrip(L81);
	pushToStrip(L72);
	pushToStrip(L59);pushToStrip(L58);pushToStrip(L57);pushToStrip(L56);
	pushToStrip(L54);
	pushToStrip(L33);
	pushToStrip(L28);pushToStrip(L32);
	pushToStrip(L14);pushToStrip(L13);pushToStrip(L12);
}
void pushUNITS_SIX()
{
	pushToStrip(L80);pushToStrip(L79);pushToStrip(L78);
	pushToStrip(L72);pushToStrip(L76);
	pushToStrip(L59);
	pushToStrip(L53);pushToStrip(L52);pushToStrip(L51);pushToStrip(L50);
	pushToStrip(L33);pushToStrip(L37);
	pushToStrip(L28);pushToStrip(L32);
	pushToStrip(L14);pushToStrip(L13);pushToStrip(L12);
}
void pushUNITS_SEVEN()
{
	pushToStrip(L77);pushToStrip(L78);pushToStrip(L79);pushToStrip(L80);pushToStrip(L81);
	pushToStrip(L76);
	pushToStrip(L56);
	pushToStrip(L52);
	pushToStrip(L36);
	pushToStrip(L29);
	pushToStrip(L14);
}
void pushUNITS_EIGHT()
{
	pushToStrip(L14);pushToStrip(L13);pushToStrip(L12);
	pushToStrip(L28);pushToStrip(L32);
	pushToStrip(L37);pushToStrip(L33);
	pushToStrip(L51);pushToStrip(L52);pushToStrip(L53);
	pushToStrip(L55);pushToStrip(L59);
	pushToStrip(L72);pushToStrip(L76);
	pushToStrip(L80);pushToStrip(L79);pushToStrip(L78);
}
void pushUNITS_NINE()
{
	pushToStrip(L78);pushToStrip(L79);pushToStrip(L80);
	pushToStrip(L76);pushToStrip(L72);
	pushToStrip(L55);pushToStrip(L59);
	pushToStrip(L51);pushToStrip(L52);pushToStrip(L53);pushToStrip(L54);
	pushToStrip(L33);
	pushToStrip(L31);
	pushToStrip(L13);pushToStrip(L14);
}
void pushTENS_ONE()
{
	pushToStrip(L17);pushToStrip(L18);pushToStrip(L19);pushToStrip(L20);pushToStrip(L21);
	pushToStrip(L24);
	pushToStrip(L41);
	pushToStrip(L46);
	pushToStrip(L63);pushToStrip(L65);
	pushToStrip(L68); pushToStrip(L67);
	pushToStrip(L85);
}
void pushTENS_TWO()
{
	pushToStrip(L17);pushToStrip(L18);pushToStrip(L19);pushToStrip(L20);pushToStrip(L21);
	pushToStrip(L23);
	pushToStrip(L41);
	pushToStrip(L47);
	pushToStrip(L61);
	pushToStrip(L70); pushToStrip(L66);
	pushToStrip(L86); pushToStrip(L85);	pushToStrip(L84);
}
void pushTENS_THREE()
{
	pushToStrip(L18);pushToStrip(L19);pushToStrip(L20);
	pushToStrip(L26);pushToStrip(L22);
	pushToStrip(L39);
	pushToStrip(L47);pushToStrip(L46);
	pushToStrip(L61);
	pushToStrip(L70); pushToStrip(L66);
	pushToStrip(L86); pushToStrip(L85);	pushToStrip(L84);
}
void pushTENS_FOUR()
{
	pushToStrip(L18);
	pushToStrip(L25);
	pushToStrip(L39);pushToStrip(L40);pushToStrip(L41);pushToStrip(L42);pushToStrip(L43);
	pushToStrip(L44);pushToStrip(L47);
	pushToStrip(L62);pushToStrip(L64);
	pushToStrip(L69); pushToStrip(L68);
	pushToStrip(L84);
}