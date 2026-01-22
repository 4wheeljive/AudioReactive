#pragma once

#include "bleControl.h"

#include "fl/audio.h"
#include "fl/audio_input.h"
#include "platforms/esp/32/audio/sound_util.h"

#include "fl/type_traits.h"
#include "fl/memory.h"
#include "fl/circular_buffer.h"

namespace flAudio {

    using fl::i16;

	const uint8_t numFreqBins = 16; 

    // I2S Configuration
    #define I2S_WS_PIN 5  // Word Select (WS) (YELLOW)
    #define I2S_SD_PIN 8  // Serial Data (SD) (GREEN)
    #define I2S_CLK_PIN 9 // Serial Clock (SCK) (BLUE)
    #define I2S_CHANNEL fl::Right

    // declare fl::audio_input.h objects
    fl::AudioConfig config = fl::AudioConfig::CreateInmp441(I2S_WS_PIN, I2S_SD_PIN, I2S_CLK_PIN, I2S_CHANNEL);
    fl::shared_ptr<fl::IAudioInput> audioSource;

    //=========================================================================
    
    void initAudioInput() {

        fl::string errorMsg;
        flAudio::audioSource = fl::IAudioInput::create(config, &errorMsg);

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

    } // initAudioInput


    void checkAudioInput() {

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
        /*
        if(debug) {
            EVERY_N_MILLISECONDS(1000) {
                Serial.println("Audio source status: OK, reading samples...");
            }
        }*/
    }

} // namespace flAudio        