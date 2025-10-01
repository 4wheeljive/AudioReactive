# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO-based ESP32 audio-reactive LED matrix project that creates visual effects synchronized to audio input. The project uses the FastLED library with custom audio processing to drive WS2812B LED strips arranged in matrix configurations.

## Common Development Commands

### Build and Upload
- `pio run` - Build the project
- `pio run -t upload` - Build and upload to connected ESP32
- `pio run -t monitor` - Open serial monitor
- `pio run -t upload -t monitor` - Upload and monitor in one command

### Hardware Configuration
- Target board: `seeed_xiao_esp32s3`
- Platform: ESP32 with Arduino framework
- File system: LittleFS

## Architecture

### Core Components

**Main Application (`src/main.cpp`)**
- Entry point with setup() and loop() functions
- Configures LED matrix dimensions and data pins
- Manages display state and BLE connectivity
- Coordinates between audio processing and LED output

**Audio Processing (`src/audioInput.h`)**
- Handles I2S audio input from INMP441 microphone
- Implements FFT-based audio analysis
- Uses FastLED's audio processing capabilities

**BLE Control (`src/bleControl.h`)**
- Bluetooth Low Energy interface for remote control
- Manages device connection state and characteristics
- Controls program selection, modes, brightness, and speed
- Handles JSON-based configuration persistence via LittleFS

**Matrix Mapping (`src/matrixMap_*.h`)**
- Multiple matrix size configurations (22x22, 24x24, 32x48)
- Various LED ordering patterns (progressive, serpentine, top-down, bottom-up)
- Custom XY coordinate mapping functions

**Test Visualizer (`src/audioTest.hpp`, `src/audioTest_detail.hpp`)**
- Test program with submodes
- Provides audio-reactive visual effects (spectrum bars, radial spectrum, waveform, VU meter)


### Key Configuration

**Matrix Sizes:**
- Small: 22x22 (484 LEDs) - default configuration
- Medium: 24x24 (576 LEDs)
- Large: 32x48 (1536 LEDs, 3-pin configuration)

**Audio Configuration:**
- Sample rate: 441kHz
- FFT size: 512
- I2S pins: WS=8, SD=9, CLK=5

**Hardware Pins:**
- Primary data pin: GPIO 2
- Multi-segment configurations use GPIO 3, 4 for large matrices

## Development Notes

### Matrix Configuration
- Use `#define BIG_BOARD` to switch to 32x48 configuration
- Matrix size affects NUM_LEDS and segment configuration
- XYMap handles coordinate transformations for different matrix layouts

### BLE Integration
- Device name and characteristics defined in bleControl.h
- JSON configuration stored in LittleFS for persistence  
- Connection state managed through global variables

### Build Configuration
- Debugging can be enabled through `debug` boolean
- Monitor filters include ESP32 exception decoder
- Build flags support PSRAM and debug levels (currently commented out)