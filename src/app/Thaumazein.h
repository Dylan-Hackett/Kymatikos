#ifndef THAUMAZEIN_H_
#define THAUMAZEIN_H_

#pragma once

#include "daisy_seed.h"
#include "daisysp.h"
#include "mpr121_daisy.h"
#include "util/CpuLoadMeter.h"
#include <cmath>
#include "Arpeggiator.h"
#include "SynthStateStorage.h"
#include "HardwareManager.h"
#include "ControlsManager.h"
#include "AudioEngine.h"

// Clouds Integration
#include "clouds/dsp/granular_processor.h"
#include "clouds/dsp/parameters.h"
// End Clouds Integration

// NOTE: using namespace directives removed from header to avoid namespace pollution
// Implementation files (.cpp) should add using namespace as needed locally

// Global Constants
#define MAX_DELAY_SAMPLES 48000
// Note: Sample rate is accessed via g_hardware.GetSampleRate()

const float MASTER_VOLUME = 0.7f; // Master output level scaler


void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer in, daisy::AudioHandle::InterleavingOutputBuffer out, size_t size);
void InitializeSynth();
void Bootload();
void UpdateLED();
void PollTouchSensor();
void ProcessControls();
void ReadKnobValues();
void UpdateEngineSelection();
void UpdateArpeggiatorToggle();

// --- Global Manager Instances ---
extern HardwareManager g_hardware;
extern ControlsManager g_controls;
extern AudioEngine g_audio_engine;

#endif // THAUMAZEIN_H_ 
