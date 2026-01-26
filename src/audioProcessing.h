#pragma once

#include "bleControl.h"
#include "audioInput.h"
#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/audio_processor.h"

namespace myAudio {

    using namespace fl; 

    //=========================================================================
    // Core audio objects
    //=========================================================================

    AudioSample currentSample;
    AudioProcessor audioProcessor;

    //=========================================================================
    // State populated by callbacks (reactive approach)
    //=========================================================================

    // Beat/tempo state
    float currentBPM = 0.0f;
    uint32_t lastBeatTime = 0;
    uint32_t beatCount = 0;
    uint32_t onsetCount = 0;
    bool beatDetected = false;
    float lastOnsetStrength = 0.0f;

    // Energy levels from callbacks
    float bassLevel = 0.0f;
    float midLevel = 0.0f;
    float trebleLevel = 0.0f;
    float energyLevel = 0.0f;
    float peakLevel = 0.0f;

    //=========================================================================
    // FFT configuration
    //=========================================================================

    constexpr uint8_t NUM_FFT_BINS = 16;
    constexpr float FFT_MIN_FREQ = 174.6f;   // ~G3
    constexpr float FFT_MAX_FREQ = 4698.3f;  // ~D8

    //=========================================================================
    // Initialize audio processing with callbacks
    //=========================================================================

    // DIAGNOSTIC: Track callback invocations
    uint32_t callbackEnergyCount = 0;
    uint32_t callbackBassCount = 0;
    uint32_t callbackPeakCount = 0;

    void initAudioProcessing() {

        // Beat detection callbacks
        audioProcessor.onBeat([]() {
            beatCount++;
            lastBeatTime = fl::millis();
            beatDetected = true;
            // Serial.print("BEAT #");
            // Serial.println(beatCount);
        });

        audioProcessor.onOnset([](float strength) {
            onsetCount++;
            lastOnsetStrength = strength;
            // Serial.print("Onset strength=");
            // Serial.println(strength);
        });

        audioProcessor.onTempoChange([](float bpm, float confidence) {
            currentBPM = bpm;
            // Serial.print("Tempo: ");
            // Serial.print(bpm);
            // Serial.print(" BPM (confidence: ");
            // Serial.print(confidence);
            // Serial.println(")");
        });

        // Frequency band callbacks
        audioProcessor.onBass([](float level) {
            bassLevel = level;
            callbackBassCount++;
        });

        audioProcessor.onMid([](float level) {
            midLevel = level;
        });

        audioProcessor.onTreble([](float level) {
            trebleLevel = level;
        });

        // Energy callbacks
        audioProcessor.onEnergy([](float rms) {
            energyLevel = rms;
            callbackEnergyCount++;
        });

        audioProcessor.onPeak([](float peak) {
            peakLevel = peak;
            callbackPeakCount++;
        });

        Serial.println("AudioProcessor initialized with callbacks");
    }

    //=========================================================================
    // Sample audio and process
    //=========================================================================

    void sampleAudio() {

        checkAudioInput();

        // Reset per-frame state
        beatDetected = false;

        // Read audio sample from I2S
        currentSample = audioSource->read();

        // DIAGNOSTIC: Check sample validity and raw data
        static uint32_t sampleCount = 0;
        static uint32_t validCount = 0;
        static uint32_t invalidCount = 0;
        sampleCount++;

        if (!currentSample.isValid()) {
            invalidCount++;
            EVERY_N_MILLISECONDS(500) {
                Serial.print("[DIAG] Sample INVALID. Total: ");
                Serial.print(sampleCount);
                Serial.print(" Valid: ");
                Serial.print(validCount);
                Serial.print(" Invalid: ");
                Serial.println(invalidCount);
            }
            return;
        }

        validCount++;

        // DIAGNOSTIC: Print raw sample data before AudioProcessor
        /*
        EVERY_N_MILLISECONDS(250) {
            Serial.print("[DIAG] Sample VALID. Raw RMS from sample: ");
            Serial.print(currentSample.rms());
            Serial.print(" | PCM size: ");
            Serial.print(currentSample.pcm().size());
            Serial.print(" | Valid/Invalid: ");
            Serial.print(validCount);
            Serial.print("/");
            Serial.println(invalidCount);
        }*/

        // Process through AudioProcessor (triggers callbacks)
        audioProcessor.update(currentSample);
    }

    //=========================================================================
    // Direct access methods (polling approach)
    // Call these AFTER sampleAudio() in your visualization code
    //=========================================================================

    // Get the AudioContext for direct FFT access
    fl::shared_ptr<AudioContext> getContext() {
        return audioProcessor.getContext();
    }

    // Get FFT bins directly (convenience wrapper)
    // Returns nullptr if no valid context
    const fl::FFTBins* getFFT() {
        auto ctx = audioProcessor.getContext();
        if (ctx) {
            return &ctx->getFFT(NUM_FFT_BINS, FFT_MIN_FREQ, FFT_MAX_FREQ);
        }
        return nullptr;
    }

    // Get RMS with DC offset correction and spike filtering
    // The INMP441 microphone outputs samples with a DC bias that inflates
    // the raw RMS calculation. This function removes the DC offset first.
    // Additionally, the I2S interface occasionally produces spurious spike
    // samples near int16_t max/min - these are filtered out.
    float getRMS() {
        if (!currentSample.isValid()) return 0.0f;

        auto pcm = currentSample.pcm();
        size_t n = pcm.size();
        if (n == 0) return 0.0f;

        // Spike threshold - samples beyond this are considered glitches
        // Based on observed data: real audio peaks at ~1000-2000, glitches hit 32000+
        // Using 10000 as a safe threshold that won't filter real audio
        constexpr int16_t SPIKE_THRESHOLD = 10000;

        // First pass: calculate DC offset excluding spike samples
        int64_t sum = 0;
        size_t validCount = 0;
        for (size_t i = 0; i < n; i++) {
            if (pcm[i] > -SPIKE_THRESHOLD && pcm[i] < SPIKE_THRESHOLD) {
                sum += pcm[i];
                validCount++;
            }
        }

        // If most samples are spikes, fall back to using all samples
        if (validCount < n / 2) {
            sum = 0;
            for (size_t i = 0; i < n; i++) {
                sum += pcm[i];
            }
            validCount = n;
        }

        int16_t dcOffset = (validCount > 0) ? static_cast<int16_t>(sum / validCount) : 0;

        // Second pass: calculate RMS excluding spike samples
        uint64_t sumSq = 0;
        size_t rmsCount = 0;
        for (size_t i = 0; i < n; i++) {
            if (pcm[i] > -SPIKE_THRESHOLD && pcm[i] < SPIKE_THRESHOLD) {
                int32_t corrected = static_cast<int32_t>(pcm[i]) - dcOffset;
                sumSq += corrected * corrected;
                rmsCount++;
            }
        }

        // Fall back if too few valid samples
        if (rmsCount < n / 2) {
            sumSq = 0;
            for (size_t i = 0; i < n; i++) {
                int32_t corrected = static_cast<int32_t>(pcm[i]) - dcOffset;
                sumSq += corrected * corrected;
            }
            rmsCount = n;
        }

        float rms = (rmsCount > 0) ? fl::sqrtf(static_cast<float>(sumSq) / rmsCount) : 0.0f;

        // Temporal smoothing to filter out occasional bad blocks
        // Use a rolling history to detect and reject outliers
        static float history[4] = {0, 0, 0, 0};
        static uint8_t histIdx = 0;

        // Store current value in history
        history[histIdx] = rms;
        histIdx = (histIdx + 1) % 4;

        // Find median of last 4 values (simple sort for 4 elements)
        float sorted[4];
        for (int i = 0; i < 4; i++) sorted[i] = history[i];
        // Simple bubble sort for 4 elements
        for (int i = 0; i < 3; i++) {
            for (int j = i + 1; j < 4; j++) {
                if (sorted[i] > sorted[j]) {
                    float tmp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = tmp;
                }
            }
        }
        // Median is average of middle two values
        float median = (sorted[1] + sorted[2]) / 2.0f;

        // Additional smoothing on the median
        static float smoothedRMS = 0.0f;
        if (median > smoothedRMS) {
            // Moderate attack: 50% new, 50% old
            smoothedRMS = median * 0.5f + smoothedRMS * 0.5f;
        } else {
            // Moderate decay: 40% new, 60% old
            smoothedRMS = median * 0.4f + smoothedRMS * 0.6f;
        }

        return smoothedRMS;
    }
    
    // Get raw PCM data for waveform visualization
    fl::Slice<const int16_t> getPCM() {
        return currentSample.pcm();
    }

    //=========================================================================
    // Debug output
    //=========================================================================

    void printAudioDebug() {
        EVERY_N_MILLISECONDS(250) {
            Serial.print("RMS: ");
            Serial.print(getRMS());
            Serial.print(" | Bass: ");
            Serial.print(bassLevel);
            Serial.print(" | Mid: ");
            Serial.print(midLevel);
            Serial.print(" | Treble: ");
            Serial.print(trebleLevel);

            const fl::FFTBins* fft = getFFT();
            if (fft && fft->bins_raw.size() > 0) {
                Serial.print(" | FFT[0]: ");
                Serial.print(fft->bins_raw[0]);
                Serial.print(" FFT[8]: ");
                Serial.print(fft->bins_raw[8]);
            }
            Serial.println();
        }
    }

    //=========================================================================
    // Comprehensive audio diagnostic for calibration
    // Prints CSV-friendly output for analysis
    //=========================================================================

    void runAudioDiagnostic() {
        if (!currentSample.isValid()) {
            EVERY_N_MILLISECONDS(500) {
                Serial.println("[DIAG] No valid sample");
            }
            return;
        }

        auto pcm = currentSample.pcm();
        size_t n = pcm.size();
        if (n == 0) return;

        // Calculate statistics
        int64_t sum = 0;
        int16_t minVal = pcm[0], maxVal = pcm[0];

        // Count samples at extremes (saturation detection)
        uint16_t countAtMax = 0;  // samples >= 32000
        uint16_t countAtMin = 0;  // samples <= -32000
        uint16_t countNearZero = 0;  // samples between -100 and 100

        for (size_t i = 0; i < n; i++) {
            sum += pcm[i];
            if (pcm[i] < minVal) minVal = pcm[i];
            if (pcm[i] > maxVal) maxVal = pcm[i];

            // Saturation detection
            if (pcm[i] >= 32000) countAtMax++;
            if (pcm[i] <= -32000) countAtMin++;
            if (pcm[i] >= -100 && pcm[i] <= 100) countNearZero++;
        }
        int16_t dcOffset = static_cast<int16_t>(sum / n);

        // Use int32_t for peak-to-peak to avoid overflow
        int32_t peakToPeak = static_cast<int32_t>(maxVal) - static_cast<int32_t>(minVal);

        // Calculate DC-corrected RMS
        uint64_t sumSq = 0;
        for (size_t i = 0; i < n; i++) {
            int32_t corrected = static_cast<int32_t>(pcm[i]) - dcOffset;
            sumSq += corrected * corrected;
        }
        float rmsCorr = fl::sqrtf(static_cast<float>(sumSq) / n);

        // Also get the uncorrected RMS for comparison
        float rmsRaw = currentSample.rms();

        // Track saturation statistics over time
        static uint32_t totalSamples = 0;
        static uint32_t saturatedBlocks = 0;
        static uint32_t goodBlocks = 0;
        totalSamples++;

        bool isSaturated = (countAtMax > 0 || countAtMin > 0);
        if (isSaturated) {
            saturatedBlocks++;
        } else {
            goodBlocks++;
        }

        // Get the filtered RMS (what we'll actually use for visualizations)
        float rmsFiltered = getRMS();

        // Print header once
        static bool headerPrinted = false;
        if (!headerPrinted) {
            Serial.println();
            Serial.println("=== AUDIO DIAGNOSTIC ===");
            Serial.println("Spikes,RMS_Unfilt,RMS_Filt,DC,Min,Max,Status");
            headerPrinted = true;
        }

        // Print CSV data every 250ms
        EVERY_N_MILLISECONDS(250) {
            Serial.print(countAtMax + countAtMin);
            Serial.print(",");
            Serial.print(rmsCorr, 0);
            Serial.print(",");
            Serial.print(rmsFiltered, 0);
            Serial.print(",");
            Serial.print(dcOffset);
            Serial.print(",");
            Serial.print(minVal);
            Serial.print(",");
            Serial.print(maxVal);
            Serial.print(",");

            // Status indicator based on FILTERED RMS
            // Calibrated for INMP441 with spike filtering:
            //   Quiet room: 10-50, Speech/claps: 100-400, Loud: 400+
            if (rmsFiltered < 50) {
                Serial.print("quiet");
            } else if (rmsFiltered < 150) {
                Serial.print("low");
            } else if (rmsFiltered < 400) {
                Serial.print("medium");
            } else {
                Serial.print("LOUD");
            }

            // Note if spikes were filtered
            if (countAtMax > 0 || countAtMin > 0) {
                Serial.print(" (");
                Serial.print(countAtMax + countAtMin);
                Serial.print(" spikes filtered)");
            }
            Serial.println();
        }

        // Print summary statistics periodically
        EVERY_N_SECONDS(10) {
            float spikePercent = (totalSamples > 0) ? (saturatedBlocks * 100.0f) / totalSamples : 0;
            Serial.println();
            Serial.print("--- Stats: ");
            Serial.print(goodBlocks);
            Serial.print(" clean blocks, ");
            Serial.print(saturatedBlocks);
            Serial.print(" with spikes (");
            Serial.print(spikePercent, 0);
            Serial.print("%) - spikes filtered OK ---");
            Serial.println();
        }
    }

} // namespace myAudio