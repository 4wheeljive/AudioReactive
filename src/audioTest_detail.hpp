#pragma once

#include "bleControl.h"
#include "audioProcessing.h"
//#include "fl/fft.h"
#include "fl/xymap.h"
#include "fl/math.h"
#include "fl/math_macros.h"

using fl::i16;

namespace audioTest {

	using namespace flAudio;

	bool audioTestInstance = false;

    uint16_t (*xyFunc)(uint8_t x, uint8_t y);
	
	uint8_t hue = 0;

	float volumeLid = 10.0f;

    void initAudioTest(uint16_t (*xy_func)(uint8_t, uint8_t)) {
        audioTestInstance = true;
        xyFunc = xy_func;
        
        // Initialize audio input system
        flAudio::initAudioInput();
		// Initialize audio processing system
		flAudio::initAudioProcessing();
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

	// Clear display
	void clearDisplay() {
		if (cFadeSpeed == 0) {
			fill_solid(leds, NUM_LEDS, CRGB::Black);
		} else {
			fadeToBlackBy(leds, NUM_LEDS, cFadeSpeed);
		}
	}

    //===============================================================================================

	void printSampleData(){

		EVERY_N_MILLISECONDS(500) {

			Serial.print("Volume: ");
			Serial.println(flAudio::smoothedAudioData->volume);
			Serial.print("VolumeRaw: ");
			Serial.println(flAudio::smoothedAudioData->volumeRaw);
			Serial.print("Peak: ");
			Serial.println(flAudio::smoothedAudioData->peak);
			Serial.print("Beat Detected: ");
			Serial.println(flAudio::smoothedAudioData->beatDetected);
			Serial.print("Dominant Frequency: ");
			Serial.println(flAudio::smoothedAudioData->dominantFrequency);
			Serial.print("Magnitude: ");
			Serial.println(flAudio::smoothedAudioData->magnitude);
			Serial.print("Volume: ");
			Serial.println(flAudio::smoothedAudioData->volume);
			for (size_t band = 0; band < numFreqBins ; band++) {
				float bandValue = smoothedAudioData->frequencyBins[band];
				Serial.print("Band");
				Serial.print(band);
				Serial.print(": ");
				Serial.println(bandValue);
			}
		}
	}
	
	// Visualization: Spectrum Bars
	void drawSpectrumBars() { 
	
		clearDisplay();
		CRGBPalette16 palette = getCurrentPalette();
		
		int barWidth = WIDTH / numFreqBins;
		//int barWidth = 1;

		if (!smoothedAudioData) return;
		
		for (size_t band = 0; band < numFreqBins ; band++) {

			float magnitude = smoothedAudioData->frequencyBins[band];

			uint16_t maxMagnitude = 1000 * cMagnitudeScale/255.0f;
			int barHeight = map(magnitude,0,maxMagnitude,0,HEIGHT-1);
			int xStart = band * barWidth;
			
			for (int x = 0; x < MAX(barWidth, 1); x++) {
				for (int y = 0; y < barHeight; y++) {

					uint8_t colorIndex = fl::map_range<float, uint8_t>(
						float(y) / HEIGHT, 0, 1, 0, 255
					);
					CRGB color = ColorFromPalette(palette, colorIndex + hue);

					int ledIndex = xyFunc(xStart + x, HEIGHT - 1 - y);

					if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
						leds[ledIndex] = color;
					}
					
					/*
					if (cMirrorMode) {
						int mirrorIndex = xyFunc(WIDTH - 1 - (xStart + x), y);
						if (mirrorIndex >= 0 && mirrorIndex < NUM_LEDS) {
							leds[mirrorIndex] = color;
						}
					}
					*/
				}
			}
		}
	}


	// Visualization: Radial Spectrum
	void drawRadialSpectrum() {

		clearDisplay();
		CRGBPalette16 palette = getCurrentPalette();

		int centerX = WIDTH / 2;
		int centerY = HEIGHT / 2;

		if (!smoothedAudioData) return;  // Safety check

		for (size_t angle = 0; angle < 360; angle += 6) {  // Reduced resolution
			size_t band = (angle / 6) % numFreqBins;

			// Get frequency bin magnitude
			float magnitude = smoothedAudioData->frequencyBins[band];

			EVERY_N_MILLISECONDS(100){ Serial.println(magnitude); }

			//magnitude = MAX(0.0f, magnitude ); // - cNoiseFloor
			//magnitude *= cGainAdjust;
			//magnitude = fl::clamp(magnitude, 0.0f, 1.0f);

			//int radius = magnitude * (MIN(WIDTH, HEIGHT) / 2);
			uint16_t maxMagnitude = 20 * cMagnitudeScale/255.0f;
			int radius = map(magnitude,0,maxMagnitude,0,1);

			for (int r = 0; r < radius; r++) {
				int x = centerX + (r * cosf(angle * PI / 180.0f));
				int y = centerY + (r * sinf(angle * PI / 180.0f));

				if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
					uint8_t colorIndex = fl::map_range<int, uint8_t>(r, 0, radius, 255, 0);
					int ledIndex = xyFunc(x, y);
					if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
						leds[ledIndex] = ColorFromPalette(palette, colorIndex + hue);
					}
				}
			}
		}
	}
	
	// Visualization: Waveform using AudioReactive processing
	void drawWaveform(const Slice<const int16_t>& pcm) {
		clearDisplay();
		CRGBPalette16 palette = getCurrentPalette();

		// Safety check for processed audio data
		if (!smoothedAudioData) return;

		int samplesPerPixel = pcm.size() / WIDTH;
		int centerY = HEIGHT / 2;

		/*
		// Use AudioReactive's volume for adaptive scaling
		float globalVolume = smoothedAudioData->volume;
		float adaptiveGain = 1.0f + (globalVolume * 2.0f); // Boost based on overall volume
		*/
		float magnitudeScale = cMagnitudeScale / 255.0f;
		
		for (size_t x = 0; x < WIDTH; x++) {
			size_t sampleIndex = x * samplesPerPixel;
			if (sampleIndex >= pcm.size()) break;

			// Get the raw sample value and normalize
			float sample = float(pcm[sampleIndex]) / 32768.0f;  // Normalize to -1.0 to 1.0
			float absSample = fabsf(sample);

			// Keep some logarithmic compression for better visual response to quiet sounds
			float logAmplitude = 0.0f;
			if (absSample > 0.001f) {
				// Logarithmic compression with AudioReactive-informed scaling
				float scaledSample = absSample * magnitudeScale * 3.0f; // Boost factor
				logAmplitude = log10f(1.0f + absSample * 9.0f) / log10f(10.0f); // Normalize to 0-1   replaced scaledSample with absSample
			}

			// Apply some gamma correction for better visual response
			logAmplitude = powf(logAmplitude, 0.6f);

			// Calculate amplitude in pixels
			int amplitudePixels = int(logAmplitude * (HEIGHT / 2));
			amplitudePixels = fl::clamp(amplitudePixels, 0, HEIGHT / 2);

			// Preserve the sign for proper waveform display
			if (sample < 0) amplitudePixels = -amplitudePixels;

			// Color mapping based on amplitude intensity
			uint8_t colorIndex = fl::map_range<int, uint8_t>(abs(amplitudePixels), 0, HEIGHT/2, 40, 255);
			CRGB color = ColorFromPalette(palette, colorIndex + hue);

			// Apply brightness scaling for low amplitudes
			if (abs(amplitudePixels) < HEIGHT / 4) {
				color.fadeToBlackBy(128 - (abs(amplitudePixels) * 512 / HEIGHT));
			}

			// Draw vertical line from center
			if (amplitudePixels == 0) {
				// Draw center point for zero amplitude
				int ledIndex = xyFunc(x, centerY);
				if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
					leds[ledIndex] = color.fadeToBlackBy(200);
				}
			} else {
				// Draw line from center to amplitude
				int startY = (amplitudePixels > 0) ? centerY : centerY + amplitudePixels;
				int endY = (amplitudePixels > 0) ? centerY + amplitudePixels : centerY;

				for (int y = startY; y <= endY; y++) {
					if (y >= 0 && y < HEIGHT) {
						int ledIndex = xyFunc(x, y);
						if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
							// Fade edges for smoother appearance
							CRGB pixelColor = color;
							if (y == startY || y == endY) {
								pixelColor.fadeToBlackBy(100);
							}
							leds[ledIndex] = pixelColor;
						}
					}
				}
			}
		}
	}

//=========================================

	// Visualization: VU Meter using AudioReactive data
	void drawVUMeter(float volume, float peak) {
		
		clearDisplay();
		CRGBPalette16 palette = getCurrentPalette();

		if (!smoothedAudioData) return;

		volumeLid = 20.0f;	

		bool beatDetected = smoothedAudioData->beatDetected;

	    // Volume level bar
		uint8_t volumeWidth = ( volume / volumeLid ) * WIDTH;
    	volumeWidth = MIN(volumeWidth, WIDTH);
		for (int x = 0; x < volumeWidth; x++) {
			for (int y = HEIGHT/3; y < 2*HEIGHT/3; y++) {
				uint8_t colorIndex = fl::map_range<int, uint8_t>(x, 0, WIDTH, 0, 255);
				int ledIndex = xyFunc(x, y);
				if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
					leds[ledIndex] = ColorFromPalette(palette, colorIndex + hue);
				}
			}
		}

		// Peak indicator
		/*int peakX = peak * WIDTH;
    	peakX = MIN(peakX, WIDTH - 1);

		for (int y = HEIGHT/4; y < 3*HEIGHT/4; y++) {
			int ledIndex = xyFunc(peakX, y);
			if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
				leds[ledIndex] = CRGB::Red;
			}
		}*/

		// Beat indicator (flash top/bottom edges)
		/*if (beatDetected) {

			for (int x = 0; x < WIDTH; x++) {
				// Top and bottom edges
				int ledIndex1 = xyFunc(x, 0);
				int ledIndex2 = xyFunc(x, HEIGHT - 1);
				if (ledIndex1 >= 0 && ledIndex1 < NUM_LEDS) leds[ledIndex1] = CRGB::White;
				if (ledIndex2 >= 0 && ledIndex2 < NUM_LEDS) leds[ledIndex2] = CRGB::White;
			}

			for (int y = 0; y < HEIGHT; y++) {
				// Left and right edges
				int ledIndex1 = xyFunc(0, y);
				int ledIndex2 = xyFunc(WIDTH - 1, y);
				if (ledIndex1 >= 0 && ledIndex1 < NUM_LEDS) leds[ledIndex1] = CRGB::White;
				if (ledIndex2 >= 0 && ledIndex2 < NUM_LEDS) leds[ledIndex2] = CRGB::White;
			}
		}*/
	}

	/*
	// Visualization: Matrix Rain
	void drawMatrixRain(float peak) {
		// Shift everything down
		for (int x = 0; x < WIDTH; x++) {
			for (int y = HEIGHT - 1; y > 0; y--) {
				int currentIndex = xyFunc(x, y);
				int aboveIndex = xyFunc(x, y - 1);
				if (currentIndex >= 0 && currentIndex < NUM_LEDS && 
					aboveIndex >= 0 && aboveIndex < NUM_LEDS) {
					leds[currentIndex] = leds[aboveIndex];
					leds[currentIndex].fadeToBlackBy(40);
				}
			}
		}
		
		// Add new drops based on audio
		int numDrops = peak * WIDTH * cAudioGain * autoGainValue;
		for (int i = 0; i < numDrops; i++) {
			int x = random(WIDTH);
			int ledIndex = xyFunc(x, 0);
			if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
				leds[ledIndex] = CHSV(96, 255, 255);  // Green
			}
		}
	}

	// Visualization: Fire Effect (simplified for WebAssembly)
	void drawFireEffect(float peak) {
		// Simple fire effect without buffer
		clearDisplay();
		
		// Add heat at bottom based on audio
		int heat = 100 + (peak * 155 * cAudioGain * autoGainValue);
		heat = MIN(heat, 255);
		
		for (int x = 0; x < WIDTH; x++) {
			for (int y = 0; y < HEIGHT; y++) {
				// Simple gradient from bottom to top
				int heatLevel = heat * (HEIGHT - y) / HEIGHT;
				heatLevel = heatLevel * random(80, 120) / 100;  // Add randomness
				heatLevel = MIN(heatLevel, 255);
				
				int ledIndex = xyFunc(x, y);
				if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
					leds[ledIndex] = HeatColor(heatLevel);
				}
			}
		}
	}

	// Visualization: Plasma Wave
	void drawPlasmaWave(float peak) {
		static float time = 0;
		time += 0.05f + (peak * 0.2f);
		
		CRGBPalette16 palette = getCurrentPalette();
		
		for (int x = 0; x < WIDTH; x++) {
			for (int y = 0; y < HEIGHT; y++) {
				float value = sinf(x * 0.1f + time) + 
							sinf(y * 0.1f - time) +
							sinf((x + y) * 0.1f + time) +
							sinf(sqrtf(x * x + y * y) * 0.1f - time);
				
				value = (value + 4) / 8;  // Normalize to 0-1
				value *= cAudioGain * autoGainValue;
				
				uint8_t colorIndex = value * 255;
				int ledIndex = xyFunc(x, y);
				if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
					leds[ledIndex] = ColorFromPalette(palette, colorIndex + hue);
				}
			}
		}
	}
	*/

	//==================================================================================

	void runAudioTest() {

		hue += 1;
		
		flAudio::updateConfig();
		flAudio::sampleAudio();

		//printSampleData();
			
		switch (MODE) {

			case 0:  // Spectrum Bars
				drawSpectrumBars();
				break;
				
			case 1:  // Radial Spectrum
				drawRadialSpectrum();
				break;
		
			case 2:  // Waveform
				drawWaveform(currentSample.pcm());
				break;
			
			case 3:  // VU Meter
				drawVUMeter(smoothedAudioData->volume, smoothedAudioData->peak);
				break;
			/*	
			case 4:  // Matrix Rain
				drawMatrixRain(peakLevel);
				break;
				
			case 5:  // Fire Effect
				drawFireEffect(peakLevel);
				break;
				
			case 6:  // Plasma Wave
				drawPlasmaWave(peakLevel);
				break;
			*/
		}
	} // runAudioTest()
		
}  // namespace audioTest
