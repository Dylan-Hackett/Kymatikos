#ifndef KYMATIKOS_H_
#define KYMATIKOS_H_

#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "mpr121.h"
#include "util/CpuLoadMeter.h"
#include <cmath>
#include "Arpeggiator.h"
#include "hardware.h"
#include "controls.h"
#include "stm32h7xx.h"
#include "Nimbus_SM/dsp/granular_processor.h"
#include "Nimbus_SM/dsp/parameters.h"

void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer in, daisy::AudioHandle::InterleavingOutputBuffer out, size_t size);
void InitClouds(float sample_rate);
void RequestArpGatePulse();

extern HardwareManager g_hardware;
extern ControlsManager g_controls;

extern const float kArabicMaqamScale[12];
float PadIndexToVoltage(int pad_index);

constexpr uint32_t kBootloaderMagic = 0xB007B007;

#endif
