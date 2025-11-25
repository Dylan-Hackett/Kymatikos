#include "HardwareManager.h"
#include "AudioConfig.h"
#include <algorithm>

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;
using namespace daisy::patch_sm;

HardwareManager::HardwareManager()
    : touch_sensor_present_(true), sample_rate_(48000.0f) {
}

void HardwareManager::Init() {
    InitHardware();
    InitADCs();
    InitTouchSensor();
    InitLEDs();
    InitGateOutputs();
}

void HardwareManager::InitHardware() {
    // Initialize Daisy Patch SM hardware
    hw_.Init();

    // Run Daisy audio at 48 kHz to match Nimbus SM Clouds processing
    hw_.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw_.SetAudioBlockSize(BLOCK_SIZE);
    sample_rate_ = hw_.AudioSampleRate(); // Should report 48000

    // Initialize CPU load meter
    cpu_meter_.Init(sample_rate_, BLOCK_SIZE);
}

void HardwareManager::InitADCs() {
    // Reuse Daisy Patch SM control ADCs and map them to the active parameters
    constexpr int kCv5    = CV_5;   // ADC 4
    constexpr int kCv6    = CV_6;   // ADC 5
    constexpr int kCv7    = CV_7;   // ADC 6
    constexpr int kPitch  = CV_8;   // ADC 7
    constexpr int kArpPad = ADC_9;  // ADC 8: Arp Pad
    constexpr int kPrev   = ADC_10; // ADC 9: Previous Pad
    constexpr int kNext   = ADC_11; // ADC 10: Next Pad
    constexpr int kMod    = ADC_12; // ADC 11: Mod Wheel Control

    cv5_knob_.Init(hw_.adc.GetPtr(kCv5), sample_rate_);
    cv6_knob_.Init(hw_.adc.GetPtr(kCv6), sample_rate_);
    cv7_knob_.Init(hw_.adc.GetPtr(kCv7), sample_rate_);
    pitch_knob_.Init(hw_.adc.GetPtr(kPitch), sample_rate_);
    arp_pad_.Init(hw_.adc.GetPtr(kArpPad), sample_rate_);
    prev_pad_.Init(hw_.adc.GetPtr(kPrev), sample_rate_);
    next_pad_.Init(hw_.adc.GetPtr(kNext), sample_rate_);
    mod_wheel_.Init(hw_.adc.GetPtr(kMod), sample_rate_);

    hw_.adc.Start();
}

void HardwareManager::InitTouchSensor() {
    // Attempt to initialize the MPR121 touch sensor
    kymatikos_hal::Mpr121::Config touch_config;
    touch_config.Defaults();
    touch_config.i2c_address = 0x5A;
    bool init_ok = touch_sensor_.Init(touch_config);
    if(!init_ok)
    {
        touch_sensor_.ClearError();
        touch_config.i2c_address = 0x5B;
        init_ok                  = touch_sensor_.Init(touch_config);
        if(init_ok)
        {
            hw_.PrintLine("[INFO] MPR121 detected at 0x5B (fallback)");
        }
    }

    if(!init_ok)
    {
        touch_sensor_present_ = false;
        hw_.PrintLine("[WARN] MPR121 init failed at 0x5A/0x5B – continuing without touch sensor");
        return;
    }

    // Override default thresholds for more sensitivity
    touch_sensor_.SetThresholds(6, 3);
    hw_.PrintLine("MPR121 touch controller detected.");
}

void HardwareManager::InitLEDs() {
    GPIO::Config led_cfg;
    led_cfg.mode  = GPIO::Mode::OUTPUT;
    led_cfg.pull  = GPIO::Pull::NOPULL;
    led_cfg.speed = GPIO::Speed::LOW;

    Pin led_pins[12] = {
        DaisyPatchSM::D1,
        DaisyPatchSM::D2,
        DaisyPatchSM::D3,
        DaisyPatchSM::D4,
        DaisyPatchSM::D5,
        DaisyPatchSM::D6,
        DaisyPatchSM::D7,
        DaisyPatchSM::D8,
        DaisyPatchSM::D9,
        DaisyPatchSM::D10,
        DaisyPatchSM::A8,
        DaisyPatchSM::A9
    };

    for(int i = 0; i < 12; ++i) {
        led_cfg.pin = led_pins[i];
        touch_leds_[i].Init(led_cfg);
        touch_leds_[i].Write(true);
    }
}

void HardwareManager::SetTouchLEDs(bool state) {
    for(auto &led : touch_leds_) {
        led.Write(state);
    }
}

void HardwareManager::InitGateOutputs() {
    daisy::GPIO::Config cfg;
    cfg.mode  = daisy::GPIO::Mode::OUTPUT;
    cfg.pull  = daisy::GPIO::Pull::NOPULL;
    cfg.speed = daisy::GPIO::Speed::LOW;
    cfg.pin   = daisy::patch_sm::DaisyPatchSM::B6;
    gate_out2_.Init(cfg);
    gate_out2_.Write(false);
}

void HardwareManager::SetGateOut2(bool state) {
    gate_out2_.Write(state);
}

void HardwareManager::SetPitchCvVoltage(float volts) {
    volts = std::max(0.0f, std::min(5.0f, volts));
    hw_.WriteCvOut(daisy::patch_sm::CV_OUT_1, volts);
}
