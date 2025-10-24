# Kymatikos

Kymatikos is a Daisy Seed–based granular instrument that pairs a twelve-pad capacitive keyboard with Mutable Instruments Clouds. Touch pressure modulates Clouds parameters in real time while an internal arpeggiator provides rhythmic control.

## Features
- MPR121 touch keyboard with LED feedback and pressure sensing
- Clouds granular engine with dynamic morph, density, and position control
- Arpeggiator timing sourced from the touch pads
- QSPI execution-in-place firmware with persistent engine storage

## Build & Flash

Prerequisites: Daisy Seed toolchain (`arm-none-eabi-*`, `make`), `dfu-util`.

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
