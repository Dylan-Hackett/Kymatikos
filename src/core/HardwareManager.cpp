#include "HardwareManager.h"
#include "AudioConfig.h"

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;

HardwareManager::HardwareManager()
    : touch_sensor_present_(true), sample_rate_(48000.0f) {
}

void HardwareManager::Init() {
    InitHardware();
    InitADCs();
    InitTouchSensor();
    InitLEDs();
}

void HardwareManager::InitHardware() {
    // Initialize Daisy Seed hardware
    hw_.Configure();
    hw_.Init();

    // Set sample rate to 32 kHz for lower CPU load and memory use
    hw_.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ);
    hw_.SetAudioBlockSize(BLOCK_SIZE);
    sample_rate_ = hw_.AudioSampleRate(); // Should be 32000

    // Initialize CPU load meter
    cpu_meter_.Init(sample_rate_, BLOCK_SIZE);
}

void HardwareManager::InitADCs() {
    // Configure 12 ADC channels
    AdcChannelConfig adc_config[12];
    adc_config[0].InitSingle(hw_.GetPin(15));  // ADC 0: Delay Time
    adc_config[1].InitSingle(hw_.GetPin(16));  // ADC 1: Delay Mix & Feedback
    adc_config[2].InitSingle(hw_.GetPin(17));  // ADC 2: Envelope Release
    adc_config[3].InitSingle(hw_.GetPin(18));  // ADC 3: Envelope Attack
    adc_config[4].InitSingle(hw_.GetPin(19));  // ADC 4: Plaits Timbre
    adc_config[5].InitSingle(hw_.GetPin(20));  // ADC 5: Plaits Harmonics
    adc_config[6].InitSingle(hw_.GetPin(21));  // ADC 6: Plaits Morph
    adc_config[7].InitSingle(hw_.GetPin(22));  // ADC 7: Plaits Pitch
    adc_config[8].InitSingle(hw_.GetPin(23));  // ADC 8: Arpeggiator Toggle Pad
    adc_config[9].InitSingle(hw_.GetPin(24));  // ADC 9: Model Select Previous Pad
    adc_config[10].InitSingle(hw_.GetPin(25)); // ADC 10: Model Select Next Pad
    adc_config[11].InitSingle(hw_.GetPin(28)); // ADC 11: Mod Wheel Control

    hw_.adc.Init(adc_config, 12);
    hw_.adc.Start();

    // Initialize AnalogControl objects
    delay_time_knob_.Init(hw_.adc.GetPtr(0), sample_rate_);
    delay_mix_feedback_knob_.Init(hw_.adc.GetPtr(1), sample_rate_);
    env_release_knob_.Init(hw_.adc.GetPtr(2), sample_rate_);
    env_attack_knob_.Init(hw_.adc.GetPtr(3), sample_rate_);
    timbre_knob_.Init(hw_.adc.GetPtr(4), sample_rate_);
    harmonics_knob_.Init(hw_.adc.GetPtr(5), sample_rate_);
    morph_knob_.Init(hw_.adc.GetPtr(6), sample_rate_);
    pitch_knob_.Init(hw_.adc.GetPtr(7), sample_rate_);
    arp_pad_.Init(hw_.adc.GetPtr(8), sample_rate_);
    model_prev_pad_.Init(hw_.adc.GetPtr(9), sample_rate_);
    model_next_pad_.Init(hw_.adc.GetPtr(10), sample_rate_);
    mod_wheel_.Init(hw_.adc.GetPtr(11), sample_rate_);
}

void HardwareManager::InitTouchSensor() {
    // Attempt to initialize the MPR121 touch sensor
    thaumazein_hal::Mpr121::Config touch_config;
    touch_config.Defaults();

    if (!touch_sensor_.Init(touch_config)) {
        // Touch sensor failed to initialize – continue without it
        touch_sensor_present_ = false;
        hw_.PrintLine("[WARN] MPR121 init failed – continuing without touch sensor");
        return;
    }

    // Override default thresholds for more sensitivity
    touch_sensor_.SetThresholds(6, 3);
}

void HardwareManager::InitLEDs() {
    GPIO::Config led_cfg;
    led_cfg.mode  = GPIO::Mode::OUTPUT;
    led_cfg.pull  = GPIO::Pull::NOPULL;
    led_cfg.speed = GPIO::Speed::LOW;

    Pin led_pins[12] = {
        seed::D14, seed::D13, seed::D10, seed::D9,
        seed::D8,  seed::D7,  seed::D6,  seed::D5,
        seed::D4,  seed::D3,  seed::D2,  seed::D1
    };

    for(int i = 0; i < 12; ++i) {
        led_cfg.pin = led_pins[i];
        touch_leds_[i].Init(led_cfg);
        touch_leds_[i].Write(true);
    }
}
