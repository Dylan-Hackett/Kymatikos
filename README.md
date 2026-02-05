# Kymatikos

Kymatikos is an analog West Coast synthesizer with digital control and FX processing. At its heart is a pure analog signal path employing classic West Coast techniques—voltage-controlled waveshaping, subharmonic generation, and lowpass gating—all orchestrated by a Daisy Patch SM microcontroller acting as the instrument's digital brain. The Daisy manages a capacitive touch keyboard, sends CV to the analog voice architecture, and processes the final audio through a granular/delay FX engine (Nimbus SM port of Mutable Instruments Clouds). This hybrid approach preserves the character of analog synthesis while leveraging digital precision for gesture capture, modulation routing, and temporal effects.


Kymatikos contains:
- Triangle core VCO
- Subharmonic oscillator
- Wavefolder
- Cycling modulation envelope for wavefolder
- Lowpass gate
- Resonant filter
- Granular synthesis/delay engine with reverb
- Fully analog audio path pre-FX

## Features
- MPR121 touch keyboard with LED feedback and pressure sensing
- CV5 drives Clouds Position & Size together, CV6 drives Density & Texture, CV7 simultaneously controls Dry/Wet, Feedback (capped at 0.75), and Reverb
- Mod wheel knob sweeps Clouds pitch ±12 semitones independent of CV pitch out
- Nimbus SM Clouds looping-delay engine with dynamic position, density, and blend control
- Arpeggiator timing sourced from the touch pads
- QSPI execution-in-place firmware with persistent engine storage

## Build & Flash

Prerequisites: Daisy toolchain (`arm-none-eabi-*`, `make`), `dfu-util`.

Lib versions:
- `lib/libdaisy` is vendored locally (current changelog shows v8.0.0).
- `lib/DaisySP` is vendored locally.

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

### Quick Git Push Alias

Use the helper script to stage, commit, and push in one step:

```bash
./scripts/git-quick-push.sh "commit message"
```

Optional shell alias (add to your `.zshrc` / `.bashrc`):

```bash
alias kgp='./scripts/git-quick-push.sh'
```

## Code Layout
- `src/app/` – entry points (`Kymatikos.cpp`, `Interface.cpp`)
- `src/dsp/` – audio ISR and arpeggiator logic
- `src/system/` – hardware, control, and audio-engine managers
- `src/platform/` – hardware drivers (MPR121, QSPI storage)
- `src/config/` – shared constants (block size, etc.)

## Licensing

This project utilizes code from Mutable Instruments, which is licensed under the MIT License. A copy of the MIT License is provided below:

Copyright 2014-2019 Emilie Gillet.

Author: Emilie Gillet (emilie.o.gillet@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

See http://creativecommons.org/licenses/MIT/ for more information.

Additionally, the DaisySP library is used, which is also under the MIT license. Other components may have their own licenses; please refer to individual source files or library documentation for specific licensing information.
