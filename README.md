# Kymatikos

Kymatikos is a Daisy Patch SM–based granular instrument that pairs a twelve-pad capacitive keyboard with the Nimbus SM port of Mutable Instruments Clouds. Running at 48 kHz, it treats touch pressure as a live modulation source while an internal arpeggiator shapes rhythmic gestures.

## Overview
- Daisy Patch SM core with QSPI boot staging for fast iteration.
- Nimbus SM Clouds engine (GranularProcessorClouds) replaces the legacy Parasites code so the DSP runs at the native 48 kHz Daisy rate.
- Hardware manager wraps touch, LEDs, CV IO, and CPU metering while the audio engine owns Clouds buffers and lifecycle.

## Current Tasks
- [ ] Dial in Nimbus parameter scaling (density/texture) for the touch pads.
- [ ] Surface playback-mode selection and status messaging on the OLED.
- [ ] Add regression tests or capture scripts to verify Nimbus output blocks at 48 kHz.

## Codebase Diagram
```text
Kymatikos
├─ src/
│  ├─ app/         # Entry points + UI/ARP glue
│  ├─ dsp/         # Audio ISR, Nimbus parameter mapping
│  ├─ system/      # Hardware, controls, audio engine
│  └─ platform/    # MPR121 + persistent state
├─ eurorack/
│  └─ Nimbus_SM/   # Ported Clouds engine, resources, DSP
├─ lib/
│  ├─ libdaisy/    # Hardware abstraction + startup
│  └─ DaisySP/     # Supplemental DSP blocks
└─ build/          # Compiled artifacts (elf/bin/hex)
```

## Features
- MPR121 touch keyboard with LED feedback and pressure sensing
- CV5 drives Clouds Position & Size together, CV6 drives Density & Texture, CV7 simultaneously controls Dry/Wet, Feedback (capped at 0.75), and Reverb
- Mod wheel knob sweeps Clouds pitch ±12 semitones independent of CV pitch out
- Nimbus SM Clouds looping-delay engine with dynamic position, density, and blend control
- Arpeggiator timing sourced from the touch pads
- QSPI execution-in-place firmware with persistent engine storage

## Build & Flash

Prerequisites: Daisy toolchain (`arm-none-eabi-*`, `make`), `dfu-util`.

```bash
git clone git@github.com:Dylan-Hackett/Kymatikos.git
cd Kymatikos
make -C lib/libdaisy -j8
make -C lib/DaisySP -j8
make    # build artifacts in build/
make flash-stub   # optional: flash boot stub to internal flash
make program-dfu  # build + flash application to QSPI
```

Key firmware targets:
- `make flash-stub` – flash the minimal internal boot stub.
- `make program-dfu` – build and flash the main QSPI-resident firmware.

Resulting binaries live under `build/` (`kymatikos.elf`, `.bin`, `.hex`).

## Code Layout
- `src/app/` – entry points (`Kymatikos.cpp`, `Interface.cpp`)
- `src/dsp/` – audio ISR and arpeggiator logic
- `src/system/` – hardware, control, and audio-engine managers
- `src/platform/` – hardware drivers (MPR121, QSPI storage)
- `src/config/` – shared constants (block size, etc.)

## Licensing

This project incorporates Mutable Instruments code released under the MIT License (© 2014–2019 Emilie Gillet) as well as DaisySP/libDaisy, also MIT-licensed. See the respective upstream projects for full license texts.
