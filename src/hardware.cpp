#include "hardware.h"
#include "settings.h"
#include <algorithm>

using namespace daisy;
using namespace daisy::patch_sm;

HardwareManager::HardwareManager()
    : touch_sensor_present_(true), sample_rate_(48000.0f) {
}

void HardwareManager::Init() {
    InitHardware();
    InitADCs();
    System::Delay(100);  // Give MPR121 time to power up
    InitTouchSensor();
    InitLEDs();
    InitGateOutputs();
}

void HardwareManager::InitHardware() {
    hw_.Init();
    hw_.StartLog(false);  // Don't block boot on USB serial
    System::Delay(50);    // Short delay for USB init
    hw_.PrintLine("=== Kymatikos Init ===");
    hw_.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw_.SetAudioBlockSize(BLOCK_SIZE);
    sample_rate_ = hw_.AudioSampleRate();
    cpu_meter_.Init(sample_rate_, BLOCK_SIZE);
}

void HardwareManager::InitADCs() {
    constexpr int kCv5    = CV_5;
    constexpr int kCv6    = CV_6;
    constexpr int kCv7    = CV_7;
    constexpr int kFSR    = CV_8;
    constexpr int kArpPad = ADC_9;
    constexpr int kPrev   = ADC_10;
    constexpr int kNext   = ADC_11;
    constexpr int kMod    = ADC_12;

    cv5_knob_.Init(hw_.adc.GetPtr(kCv5), sample_rate_);
    cv6_knob_.Init(hw_.adc.GetPtr(kCv6), sample_rate_);
    cv7_knob_.Init(hw_.adc.GetPtr(kCv7), sample_rate_);
    fsr_.Init(hw_.adc.GetPtr(kFSR), sample_rate_);
    arp_pad_.Init(hw_.adc.GetPtr(kArpPad), sample_rate_);
    prev_pad_.Init(hw_.adc.GetPtr(kPrev), sample_rate_);
    next_pad_.Init(hw_.adc.GetPtr(kNext), sample_rate_);
    mod_wheel_.Init(hw_.adc.GetPtr(kMod), sample_rate_);

    // CV1-4 as button inputs
    cv_btn_[0].Init(hw_.adc.GetPtr(CV_1), sample_rate_);
    cv_btn_[1].Init(hw_.adc.GetPtr(CV_2), sample_rate_);
    cv_btn_[2].Init(hw_.adc.GetPtr(CV_3), sample_rate_);
    cv_btn_[3].Init(hw_.adc.GetPtr(CV_4), sample_rate_);

    hw_.adc.Start();
}

void HardwareManager::InitTouchSensor() {
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

void HardwareManager::SetPressureCvVoltage(float volts) {
    volts = std::max(0.0f, std::min(5.0f, volts));
    hw_.WriteCvOut(daisy::patch_sm::CV_OUT_2, volts);
}
