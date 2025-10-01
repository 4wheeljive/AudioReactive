#pragma once

#include "bleControl.h"

//#include "fl/ui.h"
#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/xymap.h"
#include "fl/math.h"
#include "fl/math_macros.h"

#include "fl/audio_input.h"
#include "fl/type_traits.h"
#include "platforms/esp/32/audio/sound_util.h"

//#include "fl/audio_reactive.h"
#include "myAudioReactive.h"
#include <math.h>
#include "fl/memory.h"
#include "fl/circular_buffer.h"


namespace flAudio {

	const uint8_t numFreqBins = 16; // AudioReactive provides 16 frequency bins

    // I2S Configuration
    #define I2S_WS_PIN 5  // Word Select (WS) (YELLOW)
    #define I2S_SD_PIN 8  // Serial Data (SD) (GREEN)
    #define I2S_CLK_PIN 9 // Serial Clock (SCK) (BLUE)
    #define I2S_CHANNEL fl::Right

    fl::AudioConfig config = fl::AudioConfig::CreateInmp441(I2S_WS_PIN, I2S_SD_PIN, I2S_CLK_PIN, I2S_CHANNEL);
    fl::shared_ptr<fl::IAudioInput> audioSource;

    SoundLevelMeter soundMeter(0.0f,0.0f);

    AudioReactive audio;
    AudioReactiveConfig audioReactiveConfig;

    using fl::i16;

    //=========================================================================
    
    void initAudio() {

        fl::string errorMsg;
        audioSource = fl::IAudioInput::create(config, &errorMsg);

        Serial.println("Waiting 3000ms for audio device to stdout initialization...");
        delay(3000);

        if (!audioSource) {
            Serial.print("Failed to create audio source: ");
            Serial.println(errorMsg.c_str());
            return;
        }

        // Start audio capture
        Serial.println("Starting audio capture...");
        audioSource->start();

        // Check for start errors
        fl::string startErrorMsg;
        if (audioSource->error(&startErrorMsg)) {
            Serial.print("Audio start error: ");
            Serial.println(startErrorMsg.c_str());
            return;
        }

        Serial.println("Audio capture started!");

        audioReactiveConfig.sampleRate = 44100;
        audioReactiveConfig.gain = 128;
        audioReactiveConfig.sensitivity = 128;      // AGC sensitivity
        audioReactiveConfig.agcEnabled = true;
        audioReactiveConfig.scalingMode = 2;         // 0=none, 1=log, 2=linear, 3=sqrt
        
        //audioReactiveConfig.noiseGate = true;     
        audioReactiveConfig.attack = 50;          
        audioReactiveConfig.decay = 200;             

        audioReactiveConfig.enableSpectralFlux = true;
        audioReactiveConfig.enableMultiBand = true;
        audioReactiveConfig.spectralFluxThreshold = 0.01f;     // More sensitive (was 0.05f)
        audioReactiveConfig.bassThreshold = 0.02f;             // More sensitive (was 0.1f)
        audioReactiveConfig.midThreshold = 0.02f;              // More sensitive (was 0.08f)
        audioReactiveConfig.trebleThreshold = 0.02f;           // More sensitive (was 0.06f)
        
        audio.begin(audioReactiveConfig);

    } // initAudio

    // Global audio data accessible to visualizers
    fl::AudioSample currentSample;
    const AudioData* currentAudioData = nullptr;  // Pointer to current processed audio data

   //=============================================================================

    /*
    // Noise gate variables
    float gateThreshold = 0.02f;      // RMS threshold for gate open (adjustable)
    float gateHysteresis = 0.015f;    // Lower threshold for gate close
    bool gateOpen = false;            // Current gate state
    float gateSmooth = 0.0f;          // Smoothing factor for gate transitions
    const float gateAttack = 0.95f;   // Gate open speed
    const float gateDecay = 0.98f;    // Gate close speed


    // Buffer for modified PCM data
    static fl::vector<int16_t> gatedPcmBuffer;

    // Noise gate function - applies RMS-based gating with hysteresis
    void applyNoiseGate(fl::AudioSample& sample) {
        if (!sample.isValid()) return;

        auto pcm = sample.pcm();
        if (pcm.size() == 0) return;

        // Calculate RMS of the current sample buffer
        float rmsSum = 0.0f;
        for (size_t i = 0; i < pcm.size(); i++) {
            float normalized = float(pcm[i]) / 32768.0f;
            rmsSum += normalized * normalized;
        }
        float rms = sqrtf(rmsSum / pcm.size());

        // Gate decision with hysteresis
        if (!gateOpen && rms > cGateThreshold) {
            gateOpen = true;
        } else if (gateOpen && rms < gateHysteresis) {
            gateOpen = false;
        }

        // Smooth gate transitions
        float targetSmooth = gateOpen ? 1.0f : 0.0f;
        float smoothRate = gateOpen ? gateAttack : gateDecay;
        gateSmooth = gateSmooth * smoothRate + targetSmooth * (1.0f - smoothRate);

        // Apply gate to PCM data by creating new sample
        if (gateSmooth < 1.0f) {
            // Resize buffer if needed
            if (gatedPcmBuffer.size() != pcm.size()) {
                gatedPcmBuffer.resize(pcm.size());
            }

            // Apply gating to the buffer
            if (gateSmooth < 0.01f) {
                // Gate fully closed - silence
                for (size_t i = 0; i < pcm.size(); i++) {
                    gatedPcmBuffer[i] = 0;
                }
            } else {
                // Gate partially open - apply scaling
                for (size_t i = 0; i < pcm.size(); i++) {
                    gatedPcmBuffer[i] = int16_t(pcm[i] * gateSmooth);
                }
            }

            // Create new AudioSample with modified data
            fl::span<const int16_t> gatedSpan(gatedPcmBuffer.data(), gatedPcmBuffer.size());
            sample = fl::AudioSample(gatedSpan, sample.timestamp());
        }
        // If gateSmooth >= 1.0f, pass audio through unchanged
    
    } // applyNoiseGate()

    */

    void sampleAudio() {

        if (audioReactiveConfig.gain != cInputGain ) {
            Serial.print("Old gain: ");
            Serial.println(audioReactiveConfig.gain);
            audioReactiveConfig.gain = cInputGain;
            Serial.print("New gain: ");
            Serial.println(audioReactiveConfig.gain);
            audio.setConfig(audioReactiveConfig);
        }
        
        if (audioReactiveConfig.agcEnabled != cAutoGain ) {
            //Serial.print("Old gain: ");
            //Serial.println(audioReactiveConfig.gain);
            audioReactiveConfig.agcEnabled = cAutoGain;
            //Serial.print("New gain: ");
            //Serial.println(audioReactiveConfig.gain);
            audio.setConfig(audioReactiveConfig);
        }

        if (audioReactiveConfig.sensitivity != cAgcSensitivity ) {
            //Serial.print("Old gain: ");
            //Serial.println(audioReactiveConfig.gain);
            audioReactiveConfig.sensitivity = cAgcSensitivity;
            //Serial.print("New gain: ");
            //Serial.println(audioReactiveConfig.gain);
            audio.setConfig(audioReactiveConfig);
        }


        // Check if audio is enabled
        if (!cEnableAudio) {
            Serial.println("Audio not enabled!");
            delay(1000);
            return;
        }
                
        // Check if audio source is valid
        if (!audioSource) {
            Serial.println("Audio source is null!");
            delay(1000);
            return;
        }

        // Check for audio errors
        fl::string errorMsg;
        if (audioSource->error(&errorMsg)) {
            Serial.print("Audio error: ");
            Serial.println(errorMsg.c_str());
            delay(100);
            return;
        }
        
        // Check if audioSource is receiving data
        if(debug) {
            EVERY_N_MILLISECONDS(1000) {
                Serial.println("Audio source status: OK, reading samples...");
            }
        }

        // Read audio data
        currentSample = audioSource->read();

        if (currentSample.isValid()) {
            
            /*
            // Apply noise gate to raw PCM data
            fl::AudioSample processedSample = currentSample;
            if (cNoiseGate) {
                Serial.println("Noise gate active");
                processedSample = applyNoiseGate(currentSample);
            }*/

            audio.processSample(currentSample);
	        currentAudioData = &audio.getSmoothedData();  // Store pointer to the processed data
        } // if sample.isValid
        
    } // sampleAudio()    

} // namespace flAudio        