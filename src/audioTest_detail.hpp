#pragma once

#include "bleControl.h"
#include "audioProcessing.h"
#include "fl/xymap.h"

// Access to LED array from main.cpp
extern CRGB leds[];

namespace audioTest {

	using namespace myAudio;

	bool audioTestInstance = false;

    uint16_t (*xyFunc)(uint8_t x, uint8_t y);

	uint8_t hue = 0;
	uint8_t visualizationMode = 0;  // 0=spectrum, 1=VU meter, 2=beat pulse, 3=bass ripple
	const uint8_t NUM_VIS_MODES = 4;

    void initAudioTest(uint16_t (*xy_func)(uint8_t, uint8_t)) {
        audioTestInstance = true;
        xyFunc = xy_func;
        
        // Initialize audio input system
        myAudio::initAudioInput();
		// Initialize audio processing system
		myAudio::initAudioProcessing();
	}

	// Get current color palette
	CRGBPalette16 getCurrentPalette() {

		switch(cColorPalette) {
			case 0: return CRGBPalette16(RainbowColors_p);
			case 1: return CRGBPalette16(HeatColors_p);
			case 2: return CRGBPalette16(OceanColors_p);
			case 3: return CRGBPalette16(ForestColors_p);
			case 4: return CRGBPalette16(PartyColors_p);
			case 5: return CRGBPalette16(LavaColors_p);
			case 6: return CRGBPalette16(CloudColors_p);
			default: return CRGBPalette16(RainbowColors_p);
		}
	}

	//===============================================================================================
	// VISUALIZATION MODE 0: Spectrum Analyzer
	// Shows 16 FFT frequency bins as vertical bars across the matrix
	//===============================================================================================
	void drawSpectrum() {
		CRGBPalette16 palette = getCurrentPalette();

		// Clear the display
		fill_solid(leds, WIDTH * HEIGHT, CRGB::Black);

		// DEBUG: Check where crash occurs
		Serial.println("drawSpectrum: before getFFT");

		const fl::FFTBins* fft = myAudio::getFFT();

		Serial.println("drawSpectrum: after getFFT");

		if (!fft || fft->bins_raw.size() == 0) {
			Serial.println("drawSpectrum: no FFT data");
			return;
		}

		Serial.print("drawSpectrum: got ");
		Serial.print(fft->bins_raw.size());
		Serial.println(" bins");

		// Calculate bar width - spread 16 bins across WIDTH
		uint8_t barWidth = WIDTH / 16;
		if (barWidth < 1) barWidth = 1;

		for (uint8_t bin = 0; bin < 16 && bin < fft->bins_raw.size(); bin++) {
			// Get bin value and scale it (bins can be 0-500+ based on our observations)
			float rawValue = fft->bins_raw[bin];
			// Scale: assume max around 300 for good visual range
			uint8_t barHeight = constrain(map((int)rawValue, 0, 300, 0, HEIGHT), 0, HEIGHT);

			// Calculate x position for this bar
			uint8_t xStart = bin * barWidth;

			// Draw the bar from bottom up
			for (uint8_t y = 0; y < barHeight; y++) {
				// Color based on height (low=green, mid=yellow, high=red style via palette)
				uint8_t colorIndex = map(y, 0, HEIGHT - 1, 0, 255);
				CRGB color = ColorFromPalette(palette, colorIndex);

				// Draw bar width
				for (uint8_t xOff = 0; xOff < barWidth; xOff++) {
					uint8_t x = xStart + xOff;
					if (x < WIDTH) {
						// y=0 is top, so draw from bottom: HEIGHT-1-y
						uint16_t idx = xyFunc(x, HEIGHT - 1 - y);
						leds[idx] = color;
					}
				}
			}
		}
	}

	//===============================================================================================
	// VISUALIZATION MODE 1: VU Meter
	// Shows overall volume as horizontal bars filling from left to right
	//===============================================================================================
	void drawVUMeter() {
		CRGBPalette16 palette = getCurrentPalette();

		// Clear the display
		fill_solid(leds, WIDTH * HEIGHT, CRGB::Black);

		// Get RMS (with spike filtering and DC correction)
		float rms = myAudio::getRMS();

		// Observed ranges from INMP441 with spike filtering:
		//   Quiet room: 10-50
		//   Speech/claps: 100-400
		//   Loud music nearby: 400-800+
		// We use a noise floor cutoff and scale the rest

		constexpr float NOISE_FLOOR = 35.0f;    // Below this = silence; was 40
		constexpr float MAX_SIGNAL = 550.0f;    // Full scale signal;  was 600

		// Subtract noise floor and scale
		float signal = rms - NOISE_FLOOR;
		if (signal < 0) signal = 0;

		// Map to display width
		uint8_t level = constrain((int)((signal / (MAX_SIGNAL - NOISE_FLOOR)) * WIDTH), 0, WIDTH);

		// Smooth the level to reduce jitter from occasional spikes
		static uint8_t smoothedLevel = 0;
		// Fast attack, slower decay
		if (level > smoothedLevel) {
			smoothedLevel = level;  // Instant attack
		} else {
			// Decay: blend toward new level
			smoothedLevel = (smoothedLevel * 3 + level) / 4;
		}

		// Draw horizontal bars across the full height
		for (uint8_t x = 0; x < smoothedLevel; x++) {
			// Color based on position (left=green, right=red via palette)
			uint8_t colorIndex = map(x, 0, WIDTH - 1, 0, 255);
			CRGB color = ColorFromPalette(palette, colorIndex);

			for (uint8_t y = 0; y < HEIGHT; y++) {
				uint16_t idx = xyFunc(x, y);
				leds[idx] = color;
			}
		}
	}

	//===============================================================================================
	// VISUALIZATION MODE 2: Beat Pulse
	// Flashes/pulses the entire display on detected beats with decay
	//===============================================================================================
	uint8_t beatBrightness = 0;  // Decaying brightness for beat pulse

	void drawBeatPulse() {
		CRGBPalette16 palette = getCurrentPalette();

		// On beat detection, set brightness to max
		if (myAudio::beatDetected) {
			beatBrightness = 255;
			hue += 32;  // Shift color on each beat
		}

		// Fill with current color at current brightness
		CRGB color = ColorFromPalette(palette, hue);
		color.nscale8(beatBrightness);

		fill_solid(leds, WIDTH * HEIGHT, color);

		// Decay the brightness
		if (beatBrightness > 10) {
			beatBrightness = beatBrightness * 0.85;  // Exponential decay
		} else {
			beatBrightness = 0;
		}
	}

	//===============================================================================================
	// VISUALIZATION MODE 3: Bass Ripple
	// Creates expanding rings from center based on bass energy
	//===============================================================================================
	uint8_t rippleRadius = 0;
	uint8_t rippleHue = 0;

	void drawBassRipple() {
		CRGBPalette16 palette = getCurrentPalette();

		// Fade existing content
		fadeToBlackBy(leds, WIDTH * HEIGHT, 30);

		// Get bass level and check for bass hit
		float bass = myAudio::bassLevel;

		// Trigger new ripple on strong bass (threshold ~100 based on observed data)
		if (bass > 100 && rippleRadius == 0) {
			rippleRadius = 1;
			rippleHue = hue;
			hue += 40;
		}

		// Draw expanding ripple ring
		if (rippleRadius > 0) {
			uint8_t centerX = WIDTH / 2;
			uint8_t centerY = HEIGHT / 2;

			// Draw a ring at current radius
			for (uint16_t angle = 0; angle < 360; angle += 5) {
				float rad = angle * 0.01745329;  // degrees to radians
				int8_t x = centerX + (int8_t)(cos(rad) * rippleRadius);
				int8_t y = centerY + (int8_t)(sin(rad) * rippleRadius);

				if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
					uint16_t idx = xyFunc(x, y);
					leds[idx] = ColorFromPalette(palette, rippleHue);
				}
			}

			rippleRadius += 2;  // Expand the ring

			// Reset when ripple goes off screen
			uint8_t maxRadius = (WIDTH > HEIGHT ? WIDTH : HEIGHT) / 2 + 5;
			if (rippleRadius > maxRadius) {
				rippleRadius = 0;
			}
		}
	}

	//===============================================================================================
	// Cycle through visualization modes (can be triggered by MODE button via BLE)
	//===============================================================================================
	void nextVisualizationMode() {
		visualizationMode = (visualizationMode + 1) % NUM_VIS_MODES;
		Serial.print("Visualization mode: ");
		Serial.println(visualizationMode);
	}

	//===============================================================================================

	// Set to true to run audio diagnostics instead of visualizations
	// Use this to calibrate and verify audio input is working correctly
	constexpr bool DIAGNOSTIC_MODE = false;

	void testFunction() {
		// Minimal diagnostic - just show mode and occasional RMS
		EVERY_N_MILLISECONDS(2000){
			Serial.print("Mode: ");
			Serial.print(visualizationMode);
			Serial.print(" | RMS: ");
			Serial.print(myAudio::getRMS());
			Serial.print(" | Bass: ");
			Serial.println(myAudio::bassLevel);
		}
	}

	//===============================================================================================

	void runAudioTest() {

		myAudio::sampleAudio();

		// Run diagnostic mode for calibration testing
		if (DIAGNOSTIC_MODE) {
			myAudio::runAudioDiagnostic();
			// Still run VU meter visualization so you can see audio response on LEDs
			drawVUMeter();
			return;
		}

		testFunction();

		// Use cMode from BLE to select visualization (or cycle with visualizationMode)
		// For now, use the local visualizationMode variable
		// You can map cMode to visualizationMode if desired: visualizationMode = cMode % NUM_VIS_MODES;

		switch(visualizationMode=1) {
			case 0:
				drawSpectrum();
				break;
			case 1:
				drawVUMeter();
				break;
			case 2:
				drawBeatPulse();
				break;
			case 3:
				drawBassRipple();
				break;
			default:
				drawSpectrum();
				break;
		}

		// Allow mode cycling via BLE MODE control
		// Map MODE (0-255) to visualization mode
		/*
		static uint8_t lastMode = 255;  // Initialize to invalid so first check triggers
		if (MODE != lastMode) {
			visualizationMode = MODE % NUM_VIS_MODES;
			lastMode = MODE;
			Serial.print("Switched to visualization mode: ");
			Serial.println(visualizationMode);
		}*/

	} // runAudioTest()
		
}  // namespace audioTest
