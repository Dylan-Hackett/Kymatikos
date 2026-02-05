# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Kymatikos is a hybrid analog/digital West Coast synthesizer with a pure analog signal path (VCO, wavefolder, subharmonic oscillator, LPG, filter) controlled by a Daisy Patch SM microcontroller. The Daisy manages the capacitive touch keyboard (MPR121), sends CV voltages to the analog circuitry, and processes audio through a Nimbus SM granular/delay FX engine (Mutable Instruments Clouds port running at native 48 kHz).

## Build Commands

### Prerequisites
- ARM embedded toolchain (`arm-none-eabi-*`)
- `make`
- `dfu-util` for flashing

### Build & Flash Workflow

```bash
# First-time setup: build dependencies
make -C lib/libdaisy -j8
make -C lib/DaisySP -j8

# Build application
make -j8            # Outputs to build/ directory (parallel build)

# Flash workflow (QSPI boot architecture)
make flash-stub        # Flash minimal boot stub to internal flash (one-time)
make -j8 program-dfu   # Build + flash application to QSPI memory
make p              # Quick shorthand for program-app (flash only, no rebuild)

# Advanced targets
make program-sram   # Load to SRAM via OpenOCD (for debugging)
make clean          # Clean build artifacts
```

### Important Build Notes
- This project uses **QSPI execution-in-place** (APP_TYPE=BOOT_QSPI) – the main firmware runs from external QSPI flash at address 0x90040000
- The boot stub in internal flash must be flashed once with `make flash-stub`
- Normal iteration uses `make -j8 program-dfu` to rebuild and flash the QSPI application
- BLOCK_SIZE is 32 samples (defined in [src/settings.h](src/settings.h))
- Sample rate: 48 kHz
- Optimization: `-Os` (size-optimized)

## Architecture

### High-Level Signal Flow

```
Main Loop (1ms UI polling)
  ├─ ProcessControls()      → Read ADCs, process knobs/buttons
  ├─ ReadKnobValues()       → Map hardware controls to ControlSnapshot
  ├─ PollTouchSensor()      → MPR121 I2C reads, LED updates, CV generation
  └─ UpdateDisplay()        → OLED telemetry (CPU, touch state, params)

Audio ISR (BLOCK_SIZE=32 @ 48kHz)
  ├─ g_controls.SyncAudioControlSnapshot()  → Lock-free control sync
  ├─ Arpeggiator.Process()                  → Generate note triggers if enabled
  ├─ UpdateCloudsParams()                   → Map controls to Nimbus params
  ├─ g_clouds.Process()                     → Granular/delay DSP engine
  └─ g_hardware.GetCpuMeter().OnBlockEnd()  → Track CPU load
```

### Key Components

#### 1. Manager Classes (src/)
- **HardwareManager** ([hardware.h](src/hardware.h)/[hardware.cpp](src/hardware.cpp))
  - Owns `DaisyPatchSM`, `Mpr121`, `CpuLoadMeter`
  - Initializes ADC controls (CV5-7 knobs, mod wheel, arp/prev/next pads)
  - Manages 12 touch LEDs, CV outputs (pitch, pressure), gate outputs
  - Touch sensor I2C communication on I2C1

- **ControlsManager** ([controls.h](src/controls.h)/[controls.cpp](src/controls.cpp))
  - Thread-safe control state management via `ControlSnapshot` double-buffering
  - Main loop writes to `latest_snap_`, audio thread reads `audio_snap_`
  - Owns `Arpeggiator`, touch state, ADC raw values
  - Atomic flags for display updates, arpeggiator clear requests

#### 2. Audio Engine (src/audio.cpp)
- **AudioCallback()** – Real-time ISR running at 48 kHz / 32-sample blocks
- **Nimbus Integration:**
  - `GranularProcessorClouds` instance with SDRAM buffers (356KB + 196KB CCM)
  - Fixed playback mode: `PLAYBACK_MODE_LOOPING_DELAY`
  - Parameters mapped from knobs: position, size, density, texture, feedback, reverb, dry/wet, pitch
  - Input/output gain constants: `kInputGain` and `kOutputGain` (both 1.0f)

#### 3. Nimbus SM (eurorack/Nimbus_SM/)
- Port of Mutable Instruments Clouds granular engine
- DSP modules: [granular_processor.cpp](eurorack/Nimbus_SM/dsp/granular_processor.cpp), [looping_sample_player.h](eurorack/Nimbus_SM/dsp/looping_sample_player.h), fx/ (reverb, etc.)
- Resources: LUTs and lookup tables ([resources.cpp](eurorack/Nimbus_SM/resources.cpp))
- Phase vocoder code in dsp/pvoc/

#### 4. Touch Keyboard ([src/mpr121.cpp](src/mpr121.cpp))
- MPR121 I2C capacitive touch controller (12 electrodes)
- Features:
  - Touch detection with baseline deviation tracking
  - Pressure sensing via baseline deviation magnitude (0-140 range)
  - Weighted centroid calculation for continuous pitch sliding
  - Vibrato injection based on touch pressure changes + slider motion
  - LED feedback mapped via `kMprToLed[12]` remapping array
- Touch data → CV pitch voltage + pressure CV voltage

#### 5. Arpeggiator ([src/Arpeggiator.h](src/Arpeggiator.h)/[Arpeggiator.cpp](src/Arpeggiator.cpp))
- Modes: Forward, Random, AsPlayed
- Polyrhythm support with configurable tempo ratio
- Note trigger callbacks fire `RequestArpGatePulse()` which sets gate output and LED timestamps
- Tempo sourced from CV5 knob
- Enabled/disabled via arp pad toggle (hold >0.30f to toggle)

### Control Mapping

| Hardware Input | Destination | Range/Notes |
|---------------|-------------|-------------|
| CV5 Knob | Clouds Position + Size | 0.0–0.5V scaled to 0.0–1.0 |
| CV6 Knob | Clouds Density (also arp tempo) | 0.0–0.5V scaled to 0.0–1.0 |
| CV7 Knob | Clouds Texture (inverted) | 0.0–0.5V inverted |
| Mod Wheel | Clouds Pitch (±12 semitones) | Independent of CV pitch out |
| Touch Pads | Pitch CV (Arabic Maqam scale) | 2.5V base ± semitone offset |
| Touch Pressure | Pressure CV | 0–5V, smoothed exponentially |

### Thread Safety & Synchronization

- **Control Snapshot Double-Buffering:** Main loop writes to `buffers_[write_idx_]`, audio thread reads from `buffers_[read_idx_]`. Swap happens in `SyncAudioControlSnapshot()` via atomic index update.
- **Atomics Used For:**
  - Touch state (uint16_t bitmask)
  - Touch CV value (float)
  - Arpeggiator enable flag
  - Display update flags
  - LED timestamps (for arpeggiator blink timing)
  - Gate deadline (for pulse generation)

### QSPI Boot Architecture

1. **Internal Flash (0x08000000):** Minimal boot stub that jumps to QSPI
2. **QSPI Flash (0x90040000):** Main application code (execute-in-place)
3. **Vector Table Relocation:** VTOR set to 0x90040000 in `InitializeSynth()`
4. **Linker Script:** `STM32H750IB_qspi.lds` (no 256k offset)
5. **Bootloader Mode:** Hold Arp + Prev + Next pads for 500ms to enter DFU

### Scale System

- **Arabic Maqam Scale:** 12-note microtonal scale defined in `kArabicMaqamScale[12]`
- Pitch calculation: Base 2.5V ± semitone offset centered on pad 6
- Touch slider interpolates between adjacent pads for continuous pitch
- Vibrato: 6 Hz sine wave modulated by touch pressure delta

## Development Guidelines

### CRITICAL: Always Build and Verify Changes

**MANDATORY RULE:** After making ANY code changes requested by the user, you MUST:

1. **Build the project** using `make` or `make program-dfu`
2. **Verify compilation succeeded** - check for errors/warnings
3. **Report the build status** to the user explicitly
4. **If build fails:** Fix compilation errors before considering the task complete
5. **Never leave broken code** - a change is not complete until it compiles successfully

This applies to ALL code modifications, no matter how small (single line changes, typo fixes, refactors, new features, etc.).

**Example workflow:**
```bash
# After editing source files
make                    # Build and check for errors
make program-dfu        # Flash to device (if build succeeded)
# Report results to user
```

**Why this matters:** Embedded systems have complex build chains with cross-compilation, linker scripts, and hardware-specific dependencies. What looks like a simple change can easily break the build. Always verify before considering a task complete.

### When Modifying Audio Code
- Changes to `AudioCallback()` or Nimbus parameters affect real-time performance
- Monitor CPU load via OLED display or `GetCpuMeter()` – target <80% average
- Avoid dynamic allocation in audio callback
- Buffer sizes are fixed: `CLOUD_BUF_SIZE` (356352 bytes) + `CLOUD_CCM_SIZE` (196224 bytes) in SDRAM

### When Changing Block Size
- Update `BLOCK_SIZE` in [src/settings.h](src/settings.h)
- Recompile **all** dependencies (libdaisy, DaisySP, application)
- Verify Nimbus buffers are sized appropriately

### Touch Sensor Initialization
- MPR121 has error detection/recovery in `PollTouchSensor()`
- Default thresholds: touch=6, release=3
- If sensor not present, `IsTouchSensorPresent()` flag disables I2C polling
- LEDs default to heartbeat if no sensor detected

### Adding New Parameters
1. Add field to `ControlSnapshot` in [controls.h](src/controls.h)
2. Update `ReadKnobValues()` in [Kymatikos.cpp](src/Kymatikos.cpp)
3. Map to Nimbus params in `UpdateCloudsParams()` in [audio.cpp](src/audio.cpp)
4. Optionally add to OLED display format in `UpdateDisplay()`

### Debugging
- OLED shows: CPU avg/max, input peak level, touch state bitmask, current Clouds parameters
- `hw_.StartLog(false)` disables verbose logging (set to `true` for debug)
- Use `program-sram` target to load into RAM via OpenOCD for GDB debugging
- Gate Out 2 pulses on note triggers (arpeggiator or direct touch)

## External Dependencies

- **libdaisy** (lib/libdaisy): Hardware abstraction for Daisy platform
- **DaisySP** (lib/DaisySP): DSP building blocks (not heavily used; Nimbus is self-contained)
- **Nimbus_SM** (eurorack/Nimbus_SM): Mutable Instruments Clouds port (MIT licensed)

## Key Constants

```cpp
BLOCK_SIZE = 32                  // Audio buffer size (samples)
SAMPLE_RATE = 48000             // Audio sample rate (Hz)
QSPI_ADDRESS = 0x90040000       // Application start address in QSPI
kBootloaderMagic = 0xDEADBEEF   // RTC backup register magic value
kArabicMaqamScale[12]           // Microtonal pitch scale
kMprToLed[12]                   // Touch electrode → LED index mapping
```

## Common Pitfalls

1. **Forgetting to flash boot stub:** If device doesn't boot after flashing application, run `make flash-stub` once
2. **VTOR not set:** QSPI apps must set `SCB->VTOR = 0x90040000UL` early in initialization
3. **I2C conflicts:** Touch sensor shares I2C1 bus; don't initialize other I2C peripherals on same bus
4. **Audio buffer overruns:** If CPU load exceeds ~90%, reduce complexity in `AudioCallback()` or increase optimization level
5. **Touch sensor errors:** MPR121 auto-recovers but may need power cycle if I2C bus locks up
