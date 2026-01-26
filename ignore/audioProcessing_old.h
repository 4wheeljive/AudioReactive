#pragma once

#include "bleControl.h"

#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include <math.h>

#include "fl/type_traits.h"

#include "audioInput.h"
#include "myAudioReactive.h"

#include "fl/memory.h"
#include "fl/circular_buffer.h"


namespace flAudio {

    using fl::i16;

    // declare audio.h objects
    fl::AudioSample currentSample; 
    SoundLevelMeter soundMeter(0.0, 0.0);

    // declare flAudio objects
    AudioReactive audio;
    AudioReactiveConfig audioReactiveConfig;

    const AudioData* rawAudioData = nullptr;  // Pointer to current raw audio data
    const AudioData* smoothedAudioData = nullptr;  // Pointer to current processed audio data

    //=========================================================================
    
    void initAudioProcessing() {

        audioReactiveConfig.sampleRate = 44100;
        audioReactiveConfig.gain = 128;
        audioReactiveConfig.sensitivity = 128;      // AGC sensitivity
        audioReactiveConfig.agcEnabled = true;
        audioReactiveConfig.scalingMode = 2;         // 0=none, 1=log, 2=linear, 3=sqrt
        //audioReactiveConfig.noiseGate = true;     
        audioReactiveConfig.attack = 50;          
        audioReactiveConfig.decay = 200;             

        audioReactiveConfig.enableSpectralFlux = false;
        audioReactiveConfig.enableMultiBand = false;
        audioReactiveConfig.spectralFluxThreshold = 0.01f;     // More sensitive (was 0.05f)
        audioReactiveConfig.bassThreshold = 0.02f;             // More sensitive (was 0.1f)
        audioReactiveConfig.midThreshold = 0.02f;              // More sensitive (was 0.08f)
        audioReactiveConfig.trebleThreshold = 0.02f;           // More sensitive (was 0.06f)
        
        audio.begin(audioReactiveConfig);

    } // initAudioProcessing


   //=============================================================================

   void updateConfig() {

        if (audioReactiveConfig.gain != cInputGain ) {
            audioReactiveConfig.gain = cInputGain;
            if (debug) {
                Serial.print("New gain: ");
                Serial.println(audioReactiveConfig.gain);
            }
            audio.setConfig(audioReactiveConfig);
        }
        
        if (audioReactiveConfig.agcEnabled != cAutoGain ) {
            audioReactiveConfig.agcEnabled = cAutoGain;
            if (debug) {
                Serial.print("Auto gain enabled: ");
                Serial.println(audioReactiveConfig.agcEnabled);
            }
            audio.setConfig(audioReactiveConfig);
        }

        if (audioReactiveConfig.sensitivity != cAgcSensitivity ) {
            audioReactiveConfig.sensitivity = cAgcSensitivity;
            if (debug) {
                Serial.print("Auto gainsensitivity: ");
                Serial.println(audioReactiveConfig.sensitivity);
            }
            audio.setConfig(audioReactiveConfig);
        }
   }

   //=============================================================================

    void sampleAudio() {
        
        checkAudioInput();
        
        // Read audio data into audio.h AudioSample object
        currentSample = audioSource->read();
        
        float rms = currentSample.rms(); // / 32768.0f;

        soundMeter.processBlock(currentSample.pcm());
        float dBFS = soundMeter.getDBFS();
        float SPL = soundMeter.getSPL();
    
        if (currentSample.isValid()) {
            // processes the audio.h AudioSample into an flAudio AudioData object
            audio.processSample(currentSample);
            rawAudioData = &audio.getData();  // pointer to unprocessed processed data
	        smoothedAudioData = &audio.getSmoothedData();  // pointer to processed data
        } // if sample.isValid
        
        EVERY_N_MILLISECONDS(100) {
            //FASTLED_DBG("Input RMS: " << rms);
            //FASTLED_DBG("Input dBFS: " << dBFS);
            //FASTLED_DBG("Input SPL: " << SPL);
            FASTLED_DBG("SPL,vol: " << SPL << "," << smoothedAudioData->volume);
            //FASTLED_DBG("Raw peak: " << rawAudioData->peak);
            //FASTLED_DBG("Smoothed volume: " << smoothedAudioData->volume);
            //FASTLED_DBG("Smoothed peak: " << smoothedAudioData->peak);
            //FASTLED_DBG("Data: " << rms <<","<< dBFS <<","<< SPL <<","<< rawAudioData->volume <<","<< rawAudioData->peak <<","<< smoothedAudioData->volume <<","<< smoothedAudioData->peak);
            //FASTLED_DBG("Beat detected: " << smoothedAudioData->beatDetected);
            //FASTLED_DBG("Dominant Freq: " << smoothedAudioData->dominantFrequency);
            //FASTLED_DBG("Smoothed magnitude: " << smoothedAudioData->magnitude);
            //FASTLED_DBG("-------");
            //FASTLED_DBG("Frequency bins: " << smoothedAudioData->frequencyBins[16]);
        }

    } // sampleAudio()    

} // namespace flAudio        