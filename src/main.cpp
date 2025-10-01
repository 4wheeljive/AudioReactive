//*********************************************************************************************************************************************
//*********************************************************************************************************************************************

//#define FASTLED_HAS_AUDIO_INPUT
#include <Arduino.h>
#include <FastLED.h>
#include "fl/xymap.h"

//#include "fl/slice.h"
//#include "fx/fx_engine.h"

// =========================================================

#include <FS.h>
#include "LittleFS.h"
#define FORMAT_LITTLEFS_IF_FAILED true 

#define BIG_BOARD
//#undef BIG_BOARD

#define DATA_PIN_1 2

//*********************************************

#ifdef BIG_BOARD 
	#include "matrixMap_32x48_3pin.h" 
	#define DATA_PIN_2 3
    #define DATA_PIN_3 4
    #define HEIGHT 32 
    #define WIDTH 48
    #define NUM_SEGMENTS 3
    #define NUM_LEDS_PER_SEGMENT 512
#else 
	#include "matrixMap_24x24.h"
	#define HEIGHT 24 
    #define WIDTH 24
    #define NUM_SEGMENTS 1
    #define NUM_LEDS_PER_SEGMENT 576
	/*
	#include "matrixMap_22x22.h"
	#define HEIGHT 22 
    #define WIDTH 22
    #define NUM_SEGMENTS 1
    #define NUM_LEDS_PER_SEGMENT 484
	*/
#endif


//*********************************************

#define NUM_LEDS ( WIDTH * HEIGHT )

CRGB leds[NUM_LEDS];
uint16_t ledNum = 0;

using namespace fl;

//bleControl variables ***********************************************************************
//elements that must be set before #include "bleControl.h" 

uint8_t PROGRAM;
uint8_t MODE;
uint8_t SPEED;
uint8_t BRIGHTNESS;

uint8_t defaultMapping = 0;
bool mappingOverride = false;

#include "bleControl.h"
//#include "audioInput.h"
#include "audioTest.hpp"


// MAPPINGS **********************************************************************************

extern const uint16_t progTopDown[NUM_LEDS] PROGMEM;
extern const uint16_t progBottomUp[NUM_LEDS] PROGMEM;
extern const uint16_t serpTopDown[NUM_LEDS] PROGMEM;
extern const uint16_t serpBottomUp[NUM_LEDS] PROGMEM;

enum Mapping {
	TopDownProgressive = 0,
	TopDownSerpentine,
	BottomUpProgressive,
	BottomUpSerpentine
}; 

// General (non-FL::XYMap) mapping 
	uint16_t myXY(uint8_t x, uint8_t y) {
			if (x >= WIDTH || y >= HEIGHT) return 0;
			uint16_t i = ( y * WIDTH ) + x;
			switch(cMapping){
				case 0:	 ledNum = progTopDown[i]; break;
				case 1:	 ledNum = progBottomUp[i]; break;
				case 2:	 ledNum = serpTopDown[i]; break;
				case 3:	 ledNum = serpBottomUp[i]; break;
				//case 4:	 ledNum = vProgTopDown[i]; break;
				//case 5:	 ledNum = vSerpTopDown[i]; break;
			}
			return ledNum;
	}

// Used only for FL::XYMap purposes
	/*
	uint16_t myXYFunction(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
			width = WIDTH;
			height = HEIGHT;
			if (x >= width || y >= height) return 0;
			uint16_t i = ( y * width ) + x;

			switch(mapping){
				case 1:	 ledNum = progTopDown[i]; break;
				case 2:	 ledNum = progBottomUp[i]; break;
				case 3:	 ledNum = serpTopDown[i]; break;
				case 4:	 ledNum = serpBottomUp[i]; break;
			}
			
			return ledNum;
	}*/

	//uint16_t myXYFunction(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

	//XYMap myXYmap = XYMap::constructWithUserFunction(WIDTH, HEIGHT, myXYFunction);
	XYMap myXYmap = XYMap::constructWithLookUpTable(WIDTH, HEIGHT, progBottomUp);
	XYMap xyRect = XYMap::constructRectangularGrid(WIDTH, HEIGHT);

//******************************************************************************************************************************

void setup() {
		
		BRIGHTNESS = 50;
		SPEED = 5;
		PROGRAM = 3;
		MODE = 2;
		
		FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds, 0, NUM_LEDS_PER_SEGMENT)
				.setCorrection(TypicalLEDStrip);
				//.setDither(BRIGHTNESS < 255);

		#ifdef DATA_PIN_2
				FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds, NUM_LEDS_PER_SEGMENT, NUM_LEDS_PER_SEGMENT)
				.setCorrection(TypicalLEDStrip);
		#endif
		
		#ifdef DATA_PIN_3
		FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds, NUM_LEDS_PER_SEGMENT * 2, NUM_LEDS_PER_SEGMENT)
				.setCorrection(TypicalLEDStrip);
		#endif
		FastLED.setBrightness(BRIGHTNESS);

		FastLED.clear();
		FastLED.show();

		if (debug) {
			Serial.begin(115200);
			delay(500);
			Serial.print("Initial program: ");
			Serial.println(PROGRAM);
			Serial.print("Initial brightness: ");
			Serial.println(BRIGHTNESS);
			Serial.print("Initial speed: ");
			Serial.println(SPEED);
		}

		bleSetup();

		if (!LittleFS.begin(true)) {
        	Serial.println("LittleFS mount failed!");
        	return;
		}
		Serial.println("LittleFS mounted successfully.");   
		
}


//*****************************************************************************************

void loop() {

		if (!displayOn){
			FastLED.clear();
		}
		
		else {
			
			mappingOverride ? cMapping = cOverrideMapping : cMapping = defaultMapping;
	
			defaultMapping = Mapping::TopDownProgressive;
			if (!audioTest::audioTestInstance) {
				audioTest::initAudioTest(myXY);
			}
			audioTest::runAudioTest();
					
			FastLED.show();
	
		}

		// upon BLE disconnect
		if (!deviceConnected && wasConnected) {
			if (debug) {Serial.println("Device disconnected.");}
			delay(500); // give the bluetooth stack the chance to get things ready
			pServer->startAdvertising();
			if (debug) {Serial.println("Start advertising");}
			wasConnected = false;
		}

} // loop()